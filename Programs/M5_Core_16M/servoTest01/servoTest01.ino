#include <Arduino.h>

static constexpr uint32_t SERVO_HZ = 50;
static constexpr uint8_t RES_BITS = 16;
static constexpr uint32_t PERIOD_US = 1000000UL / SERVO_HZ;
static constexpr uint32_t DUTY_MAX = (1UL << RES_BITS) - 1;

static constexpr uint8_t SERVO1_PIN = 21;  // Core Port.A
static constexpr uint8_t SERVO2_PIN = 22;  // Core Port.A
static constexpr uint8_t X = 6;
static constexpr uint8_t Y = 5;

// SG90の一般的レンジ（あなたの動作実績に合わせて）
static constexpr int MIN_US_X = 500;
static constexpr int MAX_US_X = 2400;
static constexpr int MIN_US_Y = 500;
static constexpr int MAX_US_Y = 2400;

static inline uint32_t us_to_duty(uint8_t ch, int us) {
  if (ch == X) {
    if (us < MIN_US_X) us = MIN_US_X;
    if (us > MAX_US_X) us = MAX_US_X;
    return (uint32_t)us * DUTY_MAX / PERIOD_US;
  } else {
    if (us < MIN_US_Y) us = MIN_US_Y;
    if (us > MAX_US_Y) us = MAX_US_Y;
    return (uint32_t)us * DUTY_MAX / PERIOD_US;
  }
}

static inline void servoWriteUs(uint8_t ch, int us) {
  ledcWriteChannel(ch, us_to_duty(ch, us));
}

// -90..+90 → MIN_US..MAX_US（0→1500付近）
static inline void servoWriteDeg(uint8_t ch, int deg) {
  if (deg < -90) deg = -90;
  if (deg > 90) deg = 90;

  // 線形マップ
  int us;
  if (ch == X) {
    us = MIN_US_X + (deg + 90) * (MAX_US_X - MIN_US_X) / 180;
  } else {
    us = MIN_US_Y + (deg + 90) * (MAX_US_Y - MIN_US_Y) / 180;
  }
  servoWriteUs(ch, us);
}

void setup() {
  ledcAttachChannel(SERVO1_PIN, SERVO_HZ, RES_BITS, Y);
  ledcAttachChannel(SERVO2_PIN, SERVO_HZ, RES_BITS, X);
  servoWriteDeg(X, 0);
  servoWriteDeg(Y, 0);
}

void loop() {
  servoWriteDeg(X, -90);
  servoWriteDeg(Y, 90);
  delay(800);
  servoWriteDeg(X, 0);
  servoWriteDeg(Y, 0);
  delay(800);
  servoWriteDeg(X, 90);
  servoWriteDeg(Y, -90);
  delay(800);
  servoWriteDeg(X, 0);
  servoWriteDeg(Y, 0);
  delay(2000);
}
