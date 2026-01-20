#include <M5Unified.h>
#include <Wire.h>

#include "esp_chip_info.h"
#include "esp_partition.h"
#include "esp_spi_flash.h"

// Core2 の内部I2Cは通常 SDA=21, SCL=22
static constexpr int I2C_SDA = 21;
static constexpr int I2C_SCL = 22;

static bool i2cExists(uint8_t addr) {
  Wire.beginTransmission(addr);
  return (Wire.endTransmission() == 0);
}

static void dumpPartitions() {
  Serial.println("\n== Partitions ==");
  esp_partition_iterator_t it = esp_partition_find(ESP_PARTITION_TYPE_ANY, ESP_PARTITION_SUBTYPE_ANY, NULL);
  while (it) {
    const esp_partition_t* p = esp_partition_get(it);
    Serial.printf("type=%02x sub=%02x  addr=0x%06x  size=0x%06x  label=%s\n",
                  (unsigned)p->type, (unsigned)p->subtype,
                  (unsigned)p->address, (unsigned)p->size, p->label);
    it = esp_partition_next(it);
  }
  esp_partition_iterator_release(it);
}

static void dumpI2CScan() {
  Serial.println("\n== I2C scan (internal bus) ==");
  // Core2でよく見える代表（推定）:
  // 0x34: AXP192(PMIC), 0x38: Touch(FT系), 0x51: RTC(BM8563), 0x68: IMU(MPU6886等)
  for (uint8_t a = 0x08; a <= 0x77; ++a) {
    if (i2cExists(a)) {
      Serial.printf("FOUND 0x%02X", a);
      if (a == 0x34) Serial.print("  (PMIC? AXP192)");
      if (a == 0x38) Serial.print("  (Touch controller?)");
      if (a == 0x51) Serial.print("  (RTC?)");
      if (a == 0x68) Serial.print("  (IMU?)");
      Serial.println();
    }
  }
}

void setup() {
  auto cfg = M5.config();
  cfg.serial_baudrate = 115200;
  M5.begin(cfg);

  Serial.println("\n===== M5Stack Hardware Probe =====");

  // ボード判定（M5Unified）
  Serial.printf("M5.getBoard() = %d\n", (int)M5.getBoard());  // board_M5StackCore2 なら Core2 :contentReference[oaicite:1]{index=1}

  // ESPチップ情報
  esp_chip_info_t chip;
  esp_chip_info(&chip);
  Serial.printf("Chip model=%s  rev=%d  cores=%d\n",
                ESP.getChipModel(), (int)ESP.getChipRevision(), (int)ESP.getChipCores());
  Serial.printf("CPU freq=%d MHz\n", (int)ESP.getCpuFreqMHz());
  Serial.printf("SDK=%s\n", ESP.getSdkVersion());

  // Flash / PSRAM
  Serial.printf("Flash size=%u bytes  speed=%u Hz  mode=%d\n",
                (unsigned)ESP.getFlashChipSize(),
                (unsigned)ESP.getFlashChipSpeed(),
                (int)ESP.getFlashChipMode());
  Serial.printf("PSRAM found=%d  size=%u bytes  free=%u bytes\n",
                (int)psramFound(),
                (unsigned)ESP.getPsramSize(),
                (unsigned)ESP.getFreePsram());

  // Heap
  Serial.printf("Heap free=%u bytes  min_free=%u bytes\n",
                (unsigned)ESP.getFreeHeap(),
                (unsigned)ESP.getMinFreeHeap());

  // パーティション
  dumpPartitions();

  // I2Cスキャン（内部バス）
  Wire.begin(I2C_SDA, I2C_SCL, 400000);
  dumpI2CScan();

  Serial.println("===== Probe done =====");
}

void loop() {
  delay(1000);
}
