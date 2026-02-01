#include "ServoXY.hpp"

// --- public ---

ServoXY::ServoXY()
  : cfg_(Config{}) {}

ServoXY::ServoXY(const Config& cfg)
  : cfg_(cfg) {}

void ServoXY::begin(int x0, int y0) {
  x0 = clampDeg(x0);
  y0 = clampDeg(y0);

  periodUs_ = 1000000UL / cfg_.hz;
  dutyMax_ = (1UL << cfg_.resBits) - 1;

  ledcAttachChannel(cfg_.pinX, cfg_.hz, cfg_.resBits, cfg_.chX);
  ledcAttachChannel(cfg_.pinY, cfg_.hz, cfg_.resBits, cfg_.chY);
  Serial.printf("test03 %d %d %d %d\n", cfg_.pinX, cfg_.hz,cfg_.resBits, cfg_.chX);
  Serial.printf("test03 %d %d %d %d\n", cfg_.pinY, cfg_.hz,cfg_.resBits, cfg_.chY);

  delay(1000);
  Serial.printf("2300\n");
  ledcWriteChannel(cfg_.chX, 2300);
  delay(5000);
  Serial.printf("600\n");
  ledcWriteChannel(cfg_.chX, 800);
  delay(5000);

  Serial.printf("2300\n");
  ledcWriteChannel(cfg_.chX, 2300);
  delay(5000);
  Serial.printf("600\n");
  ledcWriteChannel(cfg_.chX, 800);
  delay(5000);


  cur_ = { x0, y0 };
  next_ = cur_;
  applyXY_(cur_);
  mv_.active = false;
}

void ServoXY::setNext(int x1, int y1) {
  next_.x = clampDeg(x1);
  next_.y = clampDeg(y1);
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
  deg = clampDeg(deg);
  int us;
  if (ch == cfg_.chX) us = cfg_.minUsX + (deg + 90) * (cfg_.maxUsX - cfg_.minUsX) / 180;
  else us = cfg_.minUsY + (deg + 90) * (cfg_.maxUsY - cfg_.minUsY) / 180;
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
