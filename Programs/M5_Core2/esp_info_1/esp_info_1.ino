#include <M5Unified.h>
#include <Wire.h>

TwoWire I2C1 = TwoWire(1); // M5UnifiedがWire(0)を触っても衝突しにくいよう別コントローラを使う

static bool ping(TwoWire& w, uint8_t addr) {
  w.beginTransmission(addr);
  return (w.endTransmission() == 0);
}

static void scanBus(const char* name, int sda, int scl) {
  Serial.printf("\n== I2C scan: %s (SDA=%d SCL=%d) ==\n", name, sda, scl);

  I2C1.end();
  I2C1.begin(sda, scl, 400000);

  for (uint8_t a = 0x08; a <= 0x77; ++a) {
    if (ping(I2C1, a)) Serial.printf("FOUND 0x%02X\n", a);
  }
}

void setup() {
  auto cfg = M5.config();
  cfg.serial_baudrate = 115200;
  M5.begin(cfg);

  Serial.println("\n===== Core2 I2C dual-scan =====");

  // 内蔵I2C（仕様上：0x34/0x38/0x51/0x68 が見えるはず）
  scanBus("INTERNAL", 21, 22);

  // PORT.A (Grove I2C兼用)
  scanBus("PORT.A", 32, 33);

  Serial.println("===== done =====");
}

void loop() { delay(1000); }
