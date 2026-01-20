#include <M5Unified.h>
#include <Wire.h>
#include "esp_chip_info.h"

static const char* boardName(uint8_t b) {
  // M5Unified appendix: 1=M5Stack, 2=Core2 ... に合わせた最低限マップ
  switch (b) {
    case 1: return "M5Stack (Basic/Gray/Fire/GO)";
    case 2: return "M5Stack Core2";
    default: return "Unknown/Other";
  }
}

static void i2cScan(TwoWire& w) {
  Serial.println("\n== I2C scan (Wire) ==");
  for (uint8_t addr = 1; addr < 127; ++addr) {
    w.beginTransmission(addr);
    uint8_t err = w.endTransmission();
    if (err == 0) {
      Serial.printf("FOUND 0x%02X\n", addr);
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(200);

  auto cfg = M5.config();
  M5.begin(cfg);

  Serial.println("\n===== M5Unified Probe =====");

  uint8_t b = M5.getBoard();
  Serial.printf("M5.getBoard() = %u (%s)\n", b, boardName(b));

  // Chip info
  esp_chip_info_t info{};
  esp_chip_info(&info);
  Serial.printf("Chip cores=%d  rev=%d\n", info.cores, info.revision);

  // Flash / PSRAM
  Serial.printf("Flash size=%u bytes\n", ESP.getFlashChipSize());
  Serial.printf("PSRAM found=%d  size=%u bytes  free=%u bytes\n",
                psramFound(), ESP.getPsramSize(), ESP.getFreePsram());

  Serial.printf("Heap free=%u bytes  min_free=%u bytes\n",
                ESP.getFreeHeap(), ESP.getMinFreeHeap());

  // I2C: 内部I2Cは多くのM5で 21/22。M5.begin後なら Wire は設定済みのことが多いが、
  // 念のため begin() しておく（ピン指定なし）。
  Wire.begin();
  Wire.setClock(400000);
  i2cScan(Wire);

  Serial.println("===== Probe done =====");
}

void loop() {
  delay(1000);
}
