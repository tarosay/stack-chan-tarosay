const now = Date.now();
const payload = (typeof msg.payload?.content === "string") ? msg.payload.content : msg.payload;

function fail(reason) { node.warn(reason); return null; }

msg.my_device_id = flow.get("MY_DEVICE_ID") || "m5-a"

// payload must be string
if (typeof payload !== "string") return fail("payload must be string");

// normalize newlines
const s = payload.replace(/\r\n/g, "\n");
const lines = s.split("\n");
const headerRaw = (lines[0] ?? "").trim();
const text = (lines.length >= 2) ? lines.slice(1).join("\n") : "";

// header must start with dot
if (!headerRaw.startsWith(".")) return null; // ヘッダ無しは無視（仕様）

// split tokens
const parts = headerRaw.split(".").filter(p => p.length > 0);
if (parts.length < 1) return fail("device id missing");

const device = parts[0].trim();
if (device.toUpperCase() !== msg.my_device_id.toUpperCase()) return null; // 自分宛て以外は破棄

const tokens = parts.slice(1);

// ------------------------------------------------------------
// last-wins parse
// ------------------------------------------------------------
let dStr = null;   // YYYY/MM/DD
let tStr = null;   // HH:MM:SSmsMMM / 
let dtMs = null;   // number

// move tokens (last-wins)
let xDeg = null;   // number (deg)
let yDeg = null;   // number (deg)
let mMs = null;   // number (ms)

// face / mouth (last-wins)
let face = null;   // "N"|"A"|"S"|"H"|"D"|"L"
let mouth = null;  // 0..10

for (const tokRaw of tokens) {
  const tok = String(tokRaw).trim();
  if (!tok) continue;

  // --- scheduling ---
  if (/^d\d{4}\/\d{1,2}\/\d{1,2}$/i.test(tok)) { dStr = tok.substring(1); continue; }
  if (/^t\d{1,2}:\d{1,2}:\d{1,2}(ms\d{3})?$/i.test(tok)) { tStr = tok.substring(1); continue; }

  if (/^dt\d+(\.\d+)?s$/i.test(tok)) {
    const sec = parseFloat(tok.substring(2, tok.length - 1));
    dtMs = Math.round(sec * 1000);
    continue;
  }
  if (/^dt\d+ms$/i.test(tok)) {
    dtMs = parseInt(tok.substring(2, tok.length - 2), 10);
    continue;
  }

  // --- move (last-wins) ---
  if (/^x-?\d+$/i.test(tok)) { xDeg = parseInt(tok.substring(1), 10); continue; }
  if (/^y-?\d+$/i.test(tok)) { yDeg = parseInt(tok.substring(1), 10); continue; }
  if (/^m\d+$/i.test(tok)) { mMs = parseInt(tok.substring(1), 10); continue; }

  // --- face (finalized set only: N,A,S,H,D,L) ---
  const mf = /^f([NASHDL])$/i.exec(tok);
  if (mf) { face = mf[1].toUpperCase(); continue; }

  // --- mouth size oN (0..10 clamp) ---
  const mo = /^o(-?\d+)$/i.exec(tok);
  if (mo) {
    let v = parseInt(mo[1], 10);
    if (Number.isNaN(v)) v = 0;
    if (v < 0) v = 0;
    if (v > 10) v = 10;
    mouth = v;
    continue;
  }

  // unknown tokens: ignore
}

// ------------------------------------------------------------
// validation per spec (有効条件)
// ------------------------------------------------------------
const hasText = (text.trim().length > 0);
const hasMove = (xDeg !== null) || (yDeg !== null) || (mMs !== null);
const hasFace = (face !== null);
const hasMouth = (mouth !== null);

if (!hasText && !hasMove && !hasFace && !hasMouth) {
  return fail("invalid: no text, no move, no face, no mouth");
}

// ------------------------------------------------------------
// schedule validation & compute runAtEpochMs
// ------------------------------------------------------------
const hasAbs = (tStr !== null);
const hasRel = (dtMs !== null);

if (hasAbs && hasRel) return fail("t and dt cannot coexist");
if (!hasAbs && !hasRel && dStr !== null) return fail("date-only is not allowed (t missing)");

let runAtEpochMs;
let mode;

if (!hasAbs && !hasRel) {
  mode = "asap";
  runAtEpochMs = now;
} else if (hasRel) {
  mode = "rel";
  runAtEpochMs = now + dtMs;
} else {
  mode = "abs";

  // date default = today JST
  let yyyy, MM, dd;
  if (dStr) {
    [yyyy, MM, dd] = dStr.split("/").map(v => parseInt(v, 10));
  } else {
    const jstNow = new Date(now + 9 * 3600 * 1000);
    yyyy = jstNow.getUTCFullYear();
    MM = jstNow.getUTCMonth() + 1;
    dd = jstNow.getUTCDate();
  }

  const m1 = /^(\d{1,2}):(\d{1,2}):(\d{1,2})(?:ms(\d{3}))?$/.exec(tStr);
  if (!m1) return fail("invalid t format");

  const HH = parseInt(m1[1], 10);
  const mm = parseInt(m1[2], 10);
  const SS = parseInt(m1[3], 10);
  const ms = (m1[4] !== undefined) ? parseInt(m1[4], 10) : 0;

  if (HH < 0 || HH > 23) return fail("invalid hour");
  if (mm < 0 || mm > 59) return fail("invalid minute");
  if (SS < 0 || SS > 59) return fail("invalid second");

  runAtEpochMs = Date.UTC(yyyy, MM - 1, dd, HH, mm, SS, ms) - 9 * 3600 * 1000;
}

// ------------------------------------------------------------
// wav object: ONLY when hasText
// ------------------------------------------------------------
function toJstParts(epochMs) {
  const d = new Date(epochMs + 9 * 3600 * 1000);
  const Y = d.getUTCFullYear();
  const M = String(d.getUTCMonth() + 1).padStart(2, "0");
  const D = String(d.getUTCDate()).padStart(2, "0");
  const h = String(d.getUTCHours()).padStart(2, "0");
  const m = String(d.getUTCMinutes()).padStart(2, "0");
  const s = String(d.getUTCSeconds()).padStart(2, "0");
  const ms = String(d.getUTCMilliseconds()).padStart(3, "0");
  return { Y, M, D, h, m, s, ms };
}

let wav = null;
if (hasText) {
  if (mode === "asap") {
    wav = { dir: "/home/pi/common", name: "asap.wav", name16: "asap_16.wav" };
  } else {
    const p = toJstParts(runAtEpochMs);
    wav = {
      dir: "/home/pi/common",
      name: `${p.Y}${p.M}${p.D}${p.h}${p.m}${p.s}.${p.ms}.wav`,
      name16: `${p.Y}${p.M}${p.D}${p.h}${p.m}${p.s}.${p.ms}_16.wav`
    };
  }
}

// ------------------------------------------------------------
// output normalized payload
// ------------------------------------------------------------
msg.payload = {
  device,
  text, // 本文なしなら空文字

  schedule: { mode, runAtEpochMs },

  // ★本文があるときだけ生成対象
  wav, // hasText ? {dir,name,name16} : null

  move: hasMove ? { x: xDeg, y: yDeg, m: mMs } : null,
  face: hasFace ? face : null,
  mouth: hasMouth ? mouth : null,

  meta: { receivedAtEpochMs: now, headerRaw, tokensRaw: parts }
};

return msg;