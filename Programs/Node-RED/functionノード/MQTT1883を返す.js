const MAGIC = "DISCOVER_MQTT_V1";
const MyID = "M5-8"

let s;
if (Buffer.isBuffer(msg.payload)) {
    s = msg.payload.toString("utf8").trim();
} else {
    s = String(msg.payload).trim();
}

node.warn(s);

if (!s.startsWith(MAGIC)) return null;

// 形式: "DISCOVER_MQTT_V1:<id>"
let id = "";
const rest = s.slice(MAGIC.length).trim();
if (rest.startsWith(":")) id = rest.slice(1).trim();

// ID無しは切り捨て
if (!id) return null;

//IDがMyIDと違ったら切り捨て
if (id != MyID) return null;

// 返信にIDを付与
msg.payload = Buffer.from(`MQTT:1883:${id}`, "utf8");

// 送信先＝受信元に返す
const ip = msg.ip || msg.remoteAddress || (msg._session && msg._session.remoteAddress);
const port = msg.port || msg.remotePort || (msg._session && msg._session.remotePort);
msg.ip = ip;
msg.port = port;

return msg;