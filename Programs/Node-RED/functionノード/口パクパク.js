const DEFAULT_DEVICE_ID = "m5-a";
const ROOT = "device";

// Routerが payload length==0 を捨てる運用なら true に
const FORCE_NONEMPTY_STOP_PAYLOAD = true;

function norm(v) { return String(v || "").trim().toLowerCase(); }

function asNum(v, name) {
  if (typeof v === "string") v = v.trim();
  const n = Number(v);
  if (!Number.isFinite(n)) throw new Error(`${name} must be number`);
  return n;
}

function clamp(n, lo, hi) { return Math.min(hi, Math.max(lo, n)); }

try {
  let deviceId =
    (typeof msg.deviceId === "string" && msg.deviceId.trim())
      ? msg.deviceId.trim()
      : DEFAULT_DEVICE_ID;

  let cmd = "";
  let size = 0.6;   // default
  let ms = 200;     // default

  const p = msg.payload;

  if (typeof p === "string") {
    cmd = norm(p);
  } else if (p && typeof p === "object") {
    cmd = norm(p.cmd ?? p.command);
    if (cmd === "start") {
      if (p.size !== undefined) size = asNum(p.size, "size");
      if (p.ms !== undefined) ms = asNum(p.ms, "ms");
      else if (p.speed !== undefined) ms = asNum(p.speed, "speed");
    }
  } else {
    throw new Error("payload must be string or JSON object");
  }

  if (cmd !== "start" && cmd !== "stop") throw new Error(`unknown cmd: ${cmd}`);

  if (cmd === "start") {
    size = clamp(size, 0.0, 1.0);
    ms = Math.trunc(clamp(ms, 1, 60000));
    msg.payload = `${size} ${ms}`;
    msg.qos = Number.isFinite(msg.qos) ? msg.qos : 0;
    node.status({ text: `start ${msg.payload}` });
  } else {
    msg.payload = FORCE_NONEMPTY_STOP_PAYLOAD ? "1" : "";
    msg.qos = Number.isFinite(msg.qos) ? msg.qos : 1; // stopはQoS1推奨
    node.status({ text: "stop" });
  }

  msg.topic = `${ROOT}/${deviceId}/face/speech/${cmd}`;
  return msg;

} catch (e) {
  node.error(e.message, msg);
  return null;
}
