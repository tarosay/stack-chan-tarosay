#include <M5Unified.h>

void setup() {
  auto cfg = M5.config();
  cfg.serial_baudrate = 115200;
  M5.begin(cfg);

  M5.Speaker.begin();
  M5.Speaker.setVolume(128);   // 0-255

  // 1kHzを200ms鳴らす（鳴らなければスピーカー初期化/ボード選択/配線が怪しい）
  M5.Speaker.tone(1000, 200);
}

void loop() {
  delay(1000);
}
