//return[表情, moveXY, 音声]
let p = msg.payload;

let out0 = null;
let out1 = null;
let out2 = null;

if (p.face != null || p.mouth != null) {
    const m0 = RED.util.cloneMessage(msg);
    if (p.mouth != null) {
        m0.payload.mouth = p.mouth / 10.0
    }
    out0 = m0;
}

if (p.move != null) {
    const m1 = RED.util.cloneMessage(msg);
    const move = p?.move;
    const payload = {};
    payload.cmd = "move"
    payload.deviceId = p.device;
    if (move?.x != null) payload.x = move.x;
    if (move?.y != null) payload.y = move.y;
    if (move?.m != null) {
        payload.ms = move.m;
    }
    else {
        payload.ms = 800
    }

    if (payload?.x != null || payload?.y != null) {
        m1.payload = payload;
        out1 = m1;
    }
}

if (p.wav != null) {
    const m2 = RED.util.cloneMessage(msg);
    m2.uploadfilepath = p.wav.dir + "/" + p.wav.name16;
    out2 = m2;
}

if (out0 === null && out1 === null && out2 === null) return null;

return [out0, out1, out2];
