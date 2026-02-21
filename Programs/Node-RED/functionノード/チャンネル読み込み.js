// === 設定 ===
//const TOKEN = 'MTQyNTEzNDU2MTUxOTQ2ODY0Ng.G4FK4g.gHDJ3LCLr_mjoQsga8nf5H8RDtwZscPWo3jbJo'
const CHANNEL_ID = '1425128928472727572'
const INTENTS = 1 /*Guilds*/ + 512 /*GuildMessages*/ + 32768 /*MessageContent*/; // = 33281
const MY_DEVICE_ID = "M5-8"; // ←このRasPiの担当IDに固定

flow.set("MY_DEVICE_ID", MY_DEVICE_ID)

// === ユーティリティ ===
function sendToWS(msgs) {
    // 1つ目の出力は WebSocket OUT へ
    node.send([].concat(msgs, [null]));
}
function forwardEvent(msg) {
    // 2つ目の出力は アプリ側へ（Debugなど）
    node.send([null, msg]);
}

// コンテキスト（接続ごとの状態）
const s = context.get('s') || null;              // 直近のシーケンス番号
let hbTimer = context.get('hbTimer') || null;    // setInterval のID
let hbInterval = context.get('hbInterval') || 0; // ms

// 受信処理
let payload;
try {
    payload = (typeof msg.payload === 'string') ? JSON.parse(msg.payload) : msg.payload;
} catch (e) {
    node.warn('JSON parse error');
    return; // 無視
}

// Discord Gateway opcode で分岐
const op = payload.op;
const t = payload.t;   // イベント名 (DISPATCH のとき)
const d = payload.d;   // データ
const seq = payload.s;   // シーケンス番号

if (seq !== undefined && seq !== null) {
    context.set('s', seq);
}

switch (op) {
    case 10: { // HELLO
        hbInterval = d.heartbeat_interval; // ms
        context.set('hbInterval', hbInterval);
        node.status({ fill: "green", shape: "dot", text: `HELLO interval=${hbInterval}ms` });

        // 既存のタイマーを止める
        if (hbTimer) { clearInterval(hbTimer); hbTimer = null; }

        // HEARTBEAT送信タイマー開始
        hbTimer = setInterval(() => {
            const sNow = context.get('s') || null;
            const hb = { op: 1, d: sNow }; // HEARTBEAT
            sendToWS({ payload: JSON.stringify(hb) });
        }, hbInterval);
        context.set('hbTimer', hbTimer);

        // IDENTIFY を送信
        if (!TOKEN) {
            node.error('DISCORD_TOKEN が設定されていません');
            return;
        }
        const identify = {
            op: 2,
            d: {
                token: `Bot ${TOKEN}`,
                intents: INTENTS,
                properties: { os: "linux", browser: "nodered", device: "nodered" }
            }
        };
        sendToWS({ payload: JSON.stringify(identify) });
        return; // ここで終了（下の共通処理は通さない）
    }
    case 11: { // HEARTBEAT ACK
        node.status({ fill: "green", shape: "ring", text: "HB ACK" });
        return;
    }
    case 9: { // INVALID SESSION
        node.status({ fill: "red", shape: "dot", text: "INVALID SESSION" });
        // 簡易: 再IDENTIFY（本来は RESUME を考慮）
        const identify = {
            op: 2,
            d: {
                token: `Bot ${TOKEN}`,
                intents: INTENTS,
                properties: { os: "linux", browser: "nodered", device: "nodered" }
            }
        };
        sendToWS({ payload: JSON.stringify(identify) });
        return;
    }
    case 7: { // RECONNECT 要求
        node.status({ fill: "yellow", shape: "dot", text: "RECONNECT requested" });
        // Node-REDのWSクライアントが自動再接続する想定。何もしない。
        return;
    }
    case 0: { // DISPATCH（イベント）
        if (t === 'READY') {
            node.status({ fill: "blue", shape: "dot", text: `READY ${d.user?.username || ''}` });
        }
        if (t === 'MESSAGE_CREATE') {
            // チャネルでフィルタ
            if (!CHANNEL_ID || d.channel_id === CHANNEL_ID) {
                forwardEvent({
                    payload: {
                        type: 'message',
                        id: d.id,
                        channel_id: d.channel_id,
                        guild_id: d.guild_id,
                        author: d.author?.username + '#' + d.author?.discriminator,
                        content: d.content || '',
                        timestamp: d.timestamp
                    }
                });
            }
        }
        return;
    }
    default:
        // そのまま生パケットを観察したい場合は下行のコメント解除
        // forwardEvent({ payload });
        return;
}
