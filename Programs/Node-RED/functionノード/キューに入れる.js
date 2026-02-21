function fail(r) { node.warn(r); return null; }

const p = msg.metaData; // ← 退避している正規化payload
if (!p) return fail("metaData missing");

node.warn(p);

if (!p.device || !p.schedule?.runAtEpochMs) {
    return fail("metaData missing required fields");
}

msg.topic = "enqueue";
msg.payload = p;                 // Queueノードへ渡す実体
if (p.wav == null) {
    msg.jobId = `${p.device}:${p.schedule?.runAtEpochMs}`;
}
else {
    msg.jobId = `${p.device}:${p.wav.name16}`;
}
return msg;