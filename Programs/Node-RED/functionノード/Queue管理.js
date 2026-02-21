function fail(r) { node.warn(r); return [null, { payload: { error: r } }]; }

const now = Date.now();
let q = flow.get("playQueue") || [];
let busy = flow.get("playBusy") || false;

//node.warn(q);
//node.warn(busy);

// ヘルパ
function sortQ() { q.sort((a, b) => a.runAtEpochMs - b.runAtEpochMs); }
function save() { flow.set("playQueue", q); flow.set("playBusy", busy); }

if (msg.topic === "enqueue") {
  const p = msg.payload;
  if (!p?.schedule?.runAtEpochMs || !p?.device) {
    return fail("enqueue payload invalid");
  }
  const jobId = msg.jobId || `${p.device}:${p.wav.name16}`;

  // 追加（後勝ちの重複排除をしたい場合はここで同jobIdを消す）
  // 例：同jobIdがあれば消して最後勝ち
  q = q.filter(it => it.jobId !== jobId);

  q.push({
    jobId,
    runAtEpochMs: p.schedule.runAtEpochMs,
    payload: p
  });
  sortQ();
  save();

  return [null, { payload: { enqueued: true, jobId, runAtEpochMs: p.schedule.runAtEpochMs, qlen: q.length } }];
}

// if (msg.topic === "playing") {
//   // 再生開始を知らせる（必要ならjobIdも）
//   busy = true;
//   save();
//   return [null, { payload: { playing: true, jobId: msg.jobId || null } }];
// }

// if (msg.topic === "done") {
//   busy = false;
//   save();
//   return [null, { payload: { done: true, jobId: msg.jobId || null } }];
// }

if (msg.topic === "tick") {
  if (busy) return null;         // 再生中は新規開始しない（直列再生）

  if (q.length === 0) return null;

  //node.warn(q[0].runAtEpochMs);
  //node.warn(now);

  // 期限到来判定（秒精度運用：now >= runAt で発火）
  if (q[0].runAtEpochMs > now) return null;

  const item = q.shift();
  save();

  // ここで busy を true にして「再生開始」扱いにするなら以下を有効化
  // busy = true; save();

  const out = {
    jobId: item.jobId,
    payload: item.payload
  };

  // 再生ノードへ渡す
  msg.jobId = out.jobId;
  msg.payload = out.payload;
  return [msg, { payload: { dequeued: true, jobId: out.jobId, qlen: q.length } }];
}

// 未知topicは無視
return null;
