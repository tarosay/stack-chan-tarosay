const DEFAULT_DEVICE_ID = "m5-a";
const ROOT = "device";

msg.deviceId = msg.payload?.device;

function norm(v) {
  return String(v || "").trim().toLowerCase();
}

const faceMap = new Map([
  ["d", "doubt"],
  ["n", "neutral"],
  ["a", "angry"],
  ["s", "sleepy"],
  ["h", "happy"],
  ["l", "sad"],
]);

let deviceId =
  (typeof msg.deviceId === "string" && msg.deviceId.trim())
    ? msg.deviceId.trim()
    : DEFAULT_DEVICE_ID;

// --- ここから複数 publish 対応 ---
const out = [];


// 1) face/expr
{
  // face が無いなら何もしない
  const faceRaw = msg.payload?.face;
  const code = norm(faceRaw);
  if (code) {
    const expr = faceMap.get(code);
    if (!expr) {
      node.warn(`invalid face: ${faceRaw} (skip)`);
    } else {
      out.push({
        topic: `${ROOT}/${deviceId}/face/expr/${expr}`,
        payload: "",
        qos: Number.isFinite(msg.qos) ? msg.qos : 0,
        retain: false,
      });
      node.status({ text: `expr:${expr}` });
    }
  }
}

// 2) face/mouth / size  payload: "0.65"(0..1想定)
{
  const v = msg.payload?.mouth
  if (v !== undefined && v !== null && String(v).trim() !== "") {
    const f = Number(v);
    if (!Number.isFinite(f)) {
      node.warn(`invalid mouthSize: ${v} (skip)`);
    } else {
      // 0..1 に丸め（デバイス側でも丸めてるなら削ってOK）
      let s = f;
      if (s < 0) s = 0;
      if (s > 1) s = 1;

      out.push({
        topic: `${ROOT}/${deviceId}/face/mouth/size`,
        payload: String(s),
        qos: Number.isFinite(msg.qos) ? msg.qos : 0,
        retain: false,
      });
      node.status({ text: `mouth:${s}` });
    }
  }
}

// 何も出さないなら null
if (out.length === 0) return null;

// MQTT out へは複数メッセージを配列で返す
return [out];