#include "ServoXY.hpp"
#include "JsonRead.hpp"

// --- public ---

ServoXY::ServoXY()
  : cfg_(Config{}) {}

ServoXY::ServoXY(const Config& cfg)
  : cfg_(cfg) {}

void ServoXY::begin(int x0, int y0) {
  // Precompute PWM conversion parameters
  periodUs_ = 1000000UL / cfg_.hz;
  dutyMax_ = (cfg_.resBits >= 31) ? 0xFFFFFFFFUL : ((1UL << cfg_.resBits) - 1UL);

  // Attach PWM channels (Arduino-ESP32 3.x API)
  ledcAttachChannel(cfg_.pinX, cfg_.hz, cfg_.resBits, cfg_.chX);
  ledcAttachChannel(cfg_.pinY, cfg_.hz, cfg_.resBits, cfg_.chY);

  // Set initial logical position
  cur_.x = clampDeg(x0);
  cur_.y = clampDeg(y0);
  next_ = cur_;
  mv_.active = false;

  // Apply initial output (with offset/limits)
  applyXY_(cur_);
  setNext(x0, y0);
  moveBlocking(500);
}

bool ServoXY::begin(const char* jsonPath) {
  if (!jsonPath) return false;

  // Read required pins first (if this fails, treat as JSON load failure)
  int32_t pinX = cfg_.pinX;
  int32_t pinY = cfg_.pinY;
  const bool okPin = jsonRead.loadDataMulti(jsonPath, "servo/pin",
                                            "x", &pinX, JsonRead::ValueType::I32,
                                            "y", &pinY, JsonRead::ValueType::I32,
                                            nullptr);
  if (!okPin) return false;

  cfg_.pinX = (uint8_t)pinX;
  cfg_.pinY = (uint8_t)pinY;

  // Optional parameters (missing keys are OK: defaults stay)
  int32_t ox = offset_.x;
  int32_t oy = offset_.y;
  (void)jsonRead.loadDataMulti(jsonPath, "servo/offset",
                               "x", &ox, JsonRead::ValueType::I32,
                               "y", &oy, JsonRead::ValueType::I32,
                               nullptr);
  offset_.x = (int)ox;
  offset_.y = (int)oy;

  int32_t cx = 0;
  int32_t cy = 0;
  (void)jsonRead.loadDataMulti(jsonPath, "servo/center",
                               "x", &cx, JsonRead::ValueType::I32,
                               "y", &cy, JsonRead::ValueType::I32,
                               nullptr);

  int32_t llx = lower_.x;
  int32_t lly = lower_.y;
  (void)jsonRead.loadDataMulti(jsonPath, "servo/lower_limit",
                               "x", &llx, JsonRead::ValueType::I32,
                               "y", &lly, JsonRead::ValueType::I32,
                               nullptr);
  lower_.x = (int)llx;
  lower_.y = (int)lly;

  int32_t ulx = upper_.x;
  int32_t uly = upper_.y;
  (void)jsonRead.loadDataMulti(jsonPath, "servo/upper_limit",
                               "x", &ulx, JsonRead::ValueType::I32,
                               "y", &uly, JsonRead::ValueType::I32,
                               nullptr);
  upper_.x = (int)ulx;
  upper_.y = (int)uly;

  // speed
  int32_t tickMs = (int32_t)cfg_.tickMs;
  (void)jsonRead.loadDataMulti(jsonPath, "servo/speed",
                               "tick_ms", &tickMs, JsonRead::ValueType::I32,
                               nullptr);
  if (tickMs > 0) cfg_.tickMs = (uint32_t)tickMs;

  int32_t sx = (int32_t)cfg_.maxDegPerSecX;
  int32_t sy = (int32_t)cfg_.maxDegPerSecY;
  (void)jsonRead.loadDataMulti(jsonPath, "servo/speed/max_deg_per_sec",
                               "x", &sx, JsonRead::ValueType::I32,
                               "y", &sy, JsonRead::ValueType::I32,
                               nullptr);
  if (sx >= 0) cfg_.maxDegPerSecX = (uint32_t)sx;
  if (sy >= 0) cfg_.maxDegPerSecY = (uint32_t)sy;

  // pulse ranges
  int32_t minUsX = cfg_.minUsX;
  int32_t maxUsX = cfg_.maxUsX;
  (void)jsonRead.loadDataMulti(jsonPath, "servo/pulse_us/x",
                               "min", &minUsX, JsonRead::ValueType::I32,
                               "max", &maxUsX, JsonRead::ValueType::I32,
                               nullptr);
  if (minUsX > 0) cfg_.minUsX = (int)minUsX;
  if (maxUsX > 0) cfg_.maxUsX = (int)maxUsX;

  int32_t minUsY = cfg_.minUsY;
  int32_t maxUsY = cfg_.maxUsY;
  (void)jsonRead.loadDataMulti(jsonPath, "servo/pulse_us/y",
                               "min", &minUsY, JsonRead::ValueType::I32,
                               "max", &maxUsY, JsonRead::ValueType::I32,
                               nullptr);
  if (minUsY > 0) cfg_.minUsY = (int)minUsY;
  if (maxUsY > 0) cfg_.maxUsY = (int)maxUsY;

  // Now attach and apply using center
  begin((int)cx, (int)cy);
  return true;
}

bool ServoXY::begin(const char* jsonPath, int cx, int cy) {
  if (!begin(jsonPath)) return false;
  // override center with given logical origin
  begin(cx, cy);
  return true;
}

void ServoXY::setNext(int x1, int y1) {
  // logical setpoint (before offset)
  x1 = clampDeg(x1);
  y1 = clampDeg(y1);

  // clamp by logical limits (order-insensitive)
  const int lx = min(lower_.x, upper_.x);
  const int ux = max(lower_.x, upper_.x);
  const int ly = min(lower_.y, upper_.y);
  const int uy = max(lower_.y, upper_.y);
  if (x1 < lx) x1 = lx;
  if (x1 > ux) x1 = ux;
  if (y1 < ly) y1 = ly;
  if (y1 > uy) y1 = uy;

  next_.x = x1;
  next_.y = y1;
}

ServoXY::XYPos ServoXY::cur() const {
  return cur_;
}
ServoXY::XYPos ServoXY::next() const {
  return next_;
}

void ServoXY::moveBegin(int x, int y, uint32_t durationMs) {
  setNext(x, y);
  moveBegin(durationMs);
}

void ServoXY::moveBegin(uint32_t durationMs) {
  mv_.start = cur_;
  mv_.target = next_;

  uint32_t ms = clampDurationMs_(mv_.start, mv_.target, durationMs);

  if (ms == 0 || (mv_.start.x == mv_.target.x && mv_.start.y == mv_.target.y)) {
    cur_ = mv_.target;
    applyXY_(cur_);
    mv_.active = false;
    return;
  }

  mv_.duration = ms;
  mv_.t0 = millis();
  mv_.nextTick = mv_.t0;  // 即時更新してからtickで進める
  mv_.active = true;
}

bool ServoXY::moving() const {
  return mv_.active;
}

void ServoXY::update() {
  if (!mv_.active) return;

  const uint32_t now = millis();

  // tickより早い呼び出しは無視
  if ((int32_t)(now - mv_.nextTick) < 0) return;
  mv_.nextTick += cfg_.tickMs;

  const uint32_t elapsed = now - mv_.t0;
  if (elapsed >= mv_.duration) {
    cur_ = mv_.target;
    applyXY_(cur_);
    mv_.active = false;
    return;
  }

  // 補間（float無し）
  const int x = mv_.start.x + (int32_t)(mv_.target.x - mv_.start.x) * (int32_t)elapsed / (int32_t)mv_.duration;
  const int y = mv_.start.y + (int32_t)(mv_.target.y - mv_.start.y) * (int32_t)elapsed / (int32_t)mv_.duration;

  writeDeg_(cfg_.chX, x);
  writeDeg_(cfg_.chY, y);
}

void ServoXY::moveBlocking(int x1, int y1, uint32_t durationMs) {
  setNext(x1, y1);
  moveBlocking(durationMs);
}

void ServoXY::moveBlocking(uint32_t durationMs) {
  moveBegin(durationMs);
  while (moving()) {
    update();
    delay(1);  // ★重要：MQTT xTask等にCPUを譲る
  }
}

void ServoXY::setSpeedLimit(uint32_t degPerSecX, uint32_t degPerSecY) {
  cfg_.maxDegPerSecX = degPerSecX;
  cfg_.maxDegPerSecY = degPerSecY;
}

void ServoXY::setPulseRangeX(int minUs, int maxUs) {
  cfg_.minUsX = minUs;
  cfg_.maxUsX = maxUs;
}
void ServoXY::setPulseRangeY(int minUs, int maxUs) {
  cfg_.minUsY = minUs;
  cfg_.maxUsY = maxUs;
}

// --- private ---

int ServoXY::clampDeg(int deg) {
  if (deg < -90) deg = -90;
  if (deg > 90) deg = 90;
  return deg;
}

uint32_t ServoXY::ceil_div_u32(uint32_t a, uint32_t b) {
  return (a + b - 1) / b;
}

uint32_t ServoXY::usToDuty_(uint32_t us) const {
  return us * dutyMax_ / periodUs_;
}

void ServoXY::writeUs_(uint8_t ch, int us) const {
  if (ch == cfg_.chX) {
    if (us < cfg_.minUsX) us = cfg_.minUsX;
    if (us > cfg_.maxUsX) us = cfg_.maxUsX;
  } else {
    if (us < cfg_.minUsY) us = cfg_.minUsY;
    if (us > cfg_.maxUsY) us = cfg_.maxUsY;
  }
  ledcWriteChannel(ch, usToDuty_((uint32_t)us));
}

void ServoXY::writeDeg_(uint8_t ch, int deg) const {
  // deg is logical (-90..+90). Apply logical limits, then offset, then final clamp.
  int d = clampDeg(deg);

  // limits (order-insensitive)
  if (ch == cfg_.chX) {
    const int lo = min(lower_.x, upper_.x);
    const int hi = max(lower_.x, upper_.x);
    if (d < lo) d = lo;
    if (d > hi) d = hi;
    d += offset_.x;
  } else {
    const int lo = min(lower_.y, upper_.y);
    const int hi = max(lower_.y, upper_.y);
    if (d < lo) d = lo;
    if (d > hi) d = hi;
    d += offset_.y;
  }

  d = clampDeg(d);

  int us;
  if (ch == cfg_.chX) us = cfg_.minUsX + (d + 90) * (cfg_.maxUsX - cfg_.minUsX) / 180;
  else us = cfg_.minUsY + (d + 90) * (cfg_.maxUsY - cfg_.minUsY) / 180;
  writeUs_(ch, us);
}

void ServoXY::applyXY_(const XYPos& p) const {
  writeDeg_(cfg_.chX, p.x);
  writeDeg_(cfg_.chY, p.y);
}

uint32_t ServoXY::clampDurationMs_(const XYPos& s, const XYPos& t, uint32_t reqMs) const {
  const uint32_t dx = (uint32_t)abs(t.x - s.x);
  const uint32_t dy = (uint32_t)abs(t.y - s.y);

  const uint32_t minMsX = (cfg_.maxDegPerSecX == 0) ? 0 : ceil_div_u32(dx * 1000UL, cfg_.maxDegPerSecX);
  const uint32_t minMsY = (cfg_.maxDegPerSecY == 0) ? 0 : ceil_div_u32(dy * 1000UL, cfg_.maxDegPerSecY);
  uint32_t minMs = (minMsX > minMsY) ? minMsX : minMsY;

  uint32_t ms = reqMs;
  if (ms < minMs) ms = minMs;

  // tick境界へ丸め（0は許可）
  if (ms != 0 && cfg_.tickMs != 0) {
    ms = ceil_div_u32(ms, cfg_.tickMs) * cfg_.tickMs;
  }
  return ms;
}
