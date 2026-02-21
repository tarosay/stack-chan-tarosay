// Node-RED Function node: WAV(->pcmBuffer) を MQTT でストリーム送信
// 前提:
//   msg.payload   = WAVヘッダ情報オブジェクト { sampleRate, numChannels, bitsPerSample, ... }
//   msg.pcmBuffer = PCM本体 Buffer（44Bヘッダ除去済み）
//   ※もし msg.pcmbuffer になっている場合も吸収する

const CHUNK_BYTES = 4096;        // 4096〜8192 推奨
const PREBURST_CHUNKS = 6;      // 開始時に一気送り（プレバッファ）
const BASE_PREFIX = "pcm16";     // base topic prefix
const QOS_CTRL = 1;
const QOS_PCM = 0;
const BEGIN_GUARD_MS = 80;   // 50〜150msで調整（まず80ms推奨）
const END_GUARD_MS = 300; // まず300〜800msで

// 入力取り出し
const hdr = msg.payload; // ヘッダ情報JSON（あなたの前段で作ったやつ）
const pcm = msg.pcmBuffer || msg.pcmbuffer;

if (!hdr || typeof hdr !== "object") {
    node.error("msg.payload (header JSON) is missing", msg);
    return null;
}
if (!Buffer.isBuffer(pcm)) {
    node.error("msg.pcmBuffer (PCM Buffer) is missing or not a Buffer", msg);
    return null;
}

const sr = hdr.sampleRate;
const ch = hdr.numChannels;
const bits = hdr.bitsPerSample;

if (!Number.isFinite(sr) || !Number.isFinite(ch) || !Number.isFinite(bits)) {
    node.error("header JSON must include sampleRate/numChannels/bitsPerSample", msg);
    return null;
}
if (bits !== 16) {
    node.error(`Only PCM16 supported for now (bitsPerSample=${bits})`, msg);
    return null;
}

// ファイル名（任意）
let name = "rx.wav";
if (typeof msg.filename === "string" && msg.filename.length > 0) {
    name = msg.filename.split("/").pop();
}

// セッションID（streamId）
const sid = Date.now().toString(36) + "-" + Math.random().toString(36).slice(2, 8);
const BASE = `${BASE_PREFIX}/${sid}`;

// 送信間隔を「音声時間」に合わせる
const bytesPerSec = sr * ch * (bits / 8);
const chunkMs = Math.max(1, Math.round(1000 * CHUNK_BYTES / bytesPerSec)); // 最低1ms

// 送信ユーティリティ（MQTT out ノードへ渡す想定）
function sendCtrl(obj, retain = false) {
    node.send({
        topic: `${BASE}/ctrl`,
        qos: QOS_CTRL,
        payload: JSON.stringify(obj)
    });

}

function sendPcm(seq, part, flags) {
    const h = Buffer.alloc(8);
    h.writeUInt32LE(seq >>> 0, 0);
    h.writeUInt16LE(part.length & 0xFFFF, 4);
    h.writeUInt16LE(flags & 0xFFFF, 6);
    node.send({
        topic: `${BASE}/pcm`,
        qos: QOS_PCM,
        payload: Buffer.concat([h, part])
    });
}

// 1) begin（ctrl）
sendCtrl({
    type: "begin",
    sid,
    name,
    sr,
    ch,
    bits,
    codec: "pcm16",
    seq0: 0,
    chunkBytes: CHUNK_BYTES,
    pcmBytes: pcm.length,
    ts: Date.now()
}, false);

// 2) pcm chunks
let seq = 0;
let sentChunks = 0;

for (let off = 0; off < pcm.length; off += CHUNK_BYTES) {
    const part = pcm.subarray(off, Math.min(off + CHUNK_BYTES, pcm.length));
    const isLast = (off + CHUNK_BYTES) >= pcm.length;
    const flags = isLast ? 0x0002 : 0x0000; // 例: bit1=last

    // プレバッファ分は即時、それ以降は chunkMs 間隔で送る
    //const delay = (seq < PREBURST_CHUNKS) ? 0 : (seq - PREBURST_CHUNKS + 1) * chunkMs;
    const delay = BEGIN_GUARD_MS + ((seq < PREBURST_CHUNKS) ? 0 : (seq - PREBURST_CHUNKS + 1) * chunkMs);
    ((s, p, f) => {
        setTimeout(() => {
            sendPcm(s, p, f);
            sentChunks++;
        }, delay);
    })(seq, part, flags);

    seq++;
}

// 3) end（ctrl）
const endDelay =
    BEGIN_GUARD_MS +
    ((seq <= PREBURST_CHUNKS) ? 0 : (seq - PREBURST_CHUNKS + 2) * chunkMs) +
    END_GUARD_MS;

setTimeout(() => {
    sendCtrl({
        type: "end",
        sid,
        lastSeq: seq - 1,
        chunks: seq,
        pcmBytes: pcm.length,
        ts: Date.now()
    });
    node.status({ text: `sent ${seq} chunks, ${pcm.length} bytes, chunkMs=${chunkMs}ms` });
}, endDelay);

return null;
