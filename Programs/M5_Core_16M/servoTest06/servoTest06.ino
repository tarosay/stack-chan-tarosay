#include <Arduino.h>
#include <M5Unified.h>

static constexpr int SERVO_PIN = 22;
static constexpr int SERVO_CH = 6;  // ch0 を避ける

void setup() {
  Serial.begin(115200);
  delay(2000);

  auto cfg = M5.config();
  cfg.internal_imu = false;
  cfg.internal_rtc = false;
  cfg.internal_mic = false;
  cfg.internal_spk = false;
  M5.begin(cfg);

  bool ok = ledcAttachChannel(SERVO_PIN, 50, 16, SERVO_CH);
  Serial.printf("ledcAttachChannel ok=%d\n", ok);
}

void loop() {
  ledcWriteChannel(SERVO_CH, 1638);
  delay(800);
  ledcWriteChannel(SERVO_CH, 4751);
  delay(800);
  ledcWriteChannel(SERVO_CH, 7864);
  delay(800);
  ledcWriteChannel(SERVO_CH, 4751);
  delay(2000);
}
