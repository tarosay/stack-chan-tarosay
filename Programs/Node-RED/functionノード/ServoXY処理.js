// Node-RED Function node: ServoXY (topic B方式)
// Input : msg.payload = JSON object 例 {"cmd":"move","x":20,"y":-10,"ms":350}
// Output: msg.topic   = "device/<id>/servoxy/<cmd>"
//         msg.payload = "<args>" (space-separated numbers)
//
// デバイス区別しない運用でも、topicの <id> は任意文字列でOK。
// （M5側は device/+/servoxy/# を購読し、<id> は見ない設計）

const DEFAULT_DEVICE_ID = "m5-a";
const ROOT = "device";

// ranges
const X_MIN = -90, X_MAX = 90;
const Y_MIN = -90, Y_MAX = 90;
const MS_MIN = 0, MS_MAX = 60000;
const DEGSP_MIN = 0, DEGSP_MAX = 2000;

let last_x = context.get('last_x') || 0;
let last_y = context.get('last_y') || 0;



function clamp(n, lo, hi) { return Math.min(hi, Math.max(lo, n)); }
function asInt(v, name) {
  if (typeof v === "string") v = v.trim();
  const n = Number(v);
  if (!Number.isFinite(n)) throw new Error(`${name} must be number`);
  return Math.trunc(n);
}

try {
  // deviceId: msg.deviceId or msg.payload.deviceId (optional) or default
  let deviceId = null;
  if (typeof msg.deviceId === "string" && msg.deviceId.length) deviceId = msg.deviceId.trim();
  else if (msg.payload && typeof msg.payload === "object" && typeof msg.payload.deviceId === "string" && msg.payload.deviceId.length)
    deviceId = msg.payload.deviceId.trim();
  else deviceId = DEFAULT_DEVICE_ID;

  const p = msg.payload;
  if (!p || typeof p !== "object") throw new Error("payload must be JSON object");

  const cmd = String(p.cmd || p.command || "").trim().toLowerCase();
  if (!cmd) throw new Error("cmd is required");

  let payload = "";

  if (cmd === "move") {
    if (p.x == null) {
      p.x = last_x
    }
    if (p.y == null) {
      p.y = last_y
    }
    const x = clamp(asInt(p.x, "x"), X_MIN, X_MAX);
    const y = clamp(asInt(p.y, "y"), Y_MIN, Y_MAX);
    const ms = clamp(asInt(p.ms ?? p.duration, "ms"), MS_MIN, MS_MAX);
    payload = `${x} ${y} ${ms}`;
    msg.qos = Number.isFinite(msg.qos) ? msg.qos : 0;
    context.set('last_x', x)
    context.set('last_y', y)

  } else if (cmd === "stop") {
    payload = "";
    msg.qos = Number.isFinite(msg.qos) ? msg.qos : 1; // stop は QoS1 推奨

  } else if (cmd === "home") {
    const ms = clamp(asInt(p.ms ?? p.duration, "ms"), MS_MIN, MS_MAX);
    payload = `${ms}`;
    msg.qos = Number.isFinite(msg.qos) ? msg.qos : 0;

  } else if (cmd === "speed") {
    const sx = clamp(asInt(p.sx ?? p.degps_x, "sx"), DEGSP_MIN, DEGSP_MAX);
    const sy = clamp(asInt(p.sy ?? p.degps_y, "sy"), DEGSP_MIN, DEGSP_MAX);
    payload = `${sx} ${sy}`;
    msg.qos = Number.isFinite(msg.qos) ? msg.qos : 0;

  } else if (cmd === "pulse") {
    // pulseは安全のため値域チェックは入れず、整数化だけ（必要ならclamp追加）
    const minX = asInt(p.minX ?? p.min_us_x, "minX");
    const maxX = asInt(p.maxX ?? p.max_us_x, "maxX");
    const minY = asInt(p.minY ?? p.min_us_y, "minY");
    const maxY = asInt(p.maxY ?? p.max_us_y, "maxY");
    payload = `${minX} ${maxX} ${minY} ${maxY}`;
    msg.qos = Number.isFinite(msg.qos) ? msg.qos : 0;

  } else {
    throw new Error(`unknown cmd: ${cmd}`);
  }

  msg.topic = `${ROOT}/${deviceId}/servoxy/${cmd}`;
  msg.payload = payload;

  node.status({ text: `${cmd} ${payload}`.trim() });
  return msg;

} catch (e) {
  node.error(e.message, msg);
  return null;
}
