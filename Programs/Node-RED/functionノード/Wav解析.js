const buf = msg.payload;
if (!Buffer.isBuffer(buf)) {
    node.error("msg.payload is not a Buffer", msg);
    return null;
}
if (buf.length < 44) {
    node.error(`WAV too short: ${buf.length} bytes`, msg);
    return null;
}
const riff = buf.toString("ascii", 0, 4);          // "RIFF"
const wave = buf.toString("ascii", 8, 12);         // "WAVE"
const fmt = buf.toString("ascii", 12, 16);        // "fmt "
const data = buf.toString("ascii", 36, 40);        // "data" (※標準44Bヘッダの場合)

const header = {
    riff,
    riffSize: buf.readUInt32LE(4),                 // fileSize-8
    wave,

    fmtChunkId: fmt,
    fmtChunkSize: buf.readUInt32LE(16),            // 通常16
    audioFormat: buf.readUInt16LE(20),             // 1=PCM
    numChannels: buf.readUInt16LE(22),
    sampleRate: buf.readUInt32LE(24),
    byteRate: buf.readUInt32LE(28),
    blockAlign: buf.readUInt16LE(32),
    bitsPerSample: buf.readUInt16LE(34),

    dataChunkId: data,
    dataSize: buf.readUInt32LE(40),

    // 参考（方式Aで使うとき便利）
    bytesPerSec: buf.readUInt32LE(24) * buf.readUInt16LE(22) * (buf.readUInt16LE(34) / 8),
    headerBytes: 44,
    dataOffset: 44
};

// PCM本体（ヘッダ44Bを除去）
msg.pcmBuffer = Buffer.from(buf.subarray(44));

// JSON化（Node-REDではオブジェクトでOK。必要なら downstream で JSON node）
msg.payload = header;

return msg;