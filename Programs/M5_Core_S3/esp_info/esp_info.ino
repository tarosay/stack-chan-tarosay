#include <M5Unified.h>
#include <Wire.h>

#include "esp_chip_info.h"
#include "esp_partition.h"
#include "esp_spi_flash.h"

// --------------------------------------
// 内部I2Cピン
// Core2  : SDA=21, SCL=22
// CoreS3 : SDA=12, SCL=11
// --------------------------------------
static int g_i2c_sda = 21;
static int g_i2c_scl = 22;
static const char* g_board_name = "Unknown";

static void decideInternalI2CPins() {
#if defined(ARDUINO_M5STACK_CORES3)
  g_i2c_sda = 12;
  g_i2c_scl = 11;
  g_board_name = "M5Stack CoreS3";
#elif defined(ARDUINO_M5STACK_CORE2)
  g_i2c_sda = 21;
  g_i2c_scl = 22;
  g_board_name = "M5Stack Core2";
#else
  auto b = M5.getBoard();
  switch (b) {
    case m5::board_t::board_M5StackCoreS3:
      g_i2c_sda = 12;
      g_i2c_scl = 11;
      g_board_name = "M5Stack CoreS3";
      break;

    case m5::board_t::board_M5StackCore2:
      g_i2c_sda = 21;
      g_i2c_scl = 22;
      g_board_name = "M5Stack Core2";
      break;

    default:
      g_i2c_sda = 21;
      g_i2c_scl = 22;
      g_board_name = "Unknown/Default(Core2 pins)";
      break;
  }
#endif
}

static bool i2cExists(uint8_t addr) {
  Wire.beginTransmission(addr);
  return (Wire.endTransmission() == 0);
}

// iterator列挙をやめて、既知ラベルを find_first で拾う安全版
static void printPartitionByLabel(const char* label) {
  const esp_partition_t* p =
    esp_partition_find_first(ESP_PARTITION_TYPE_ANY, ESP_PARTITION_SUBTYPE_ANY, label);

  if (!p) {
    Serial.printf("label=%s  (not found)\n", label);
    return;
  }

  Serial.printf("type=%02x sub=%02x  addr=0x%06x  size=0x%06x  label=%s\n",
                (unsigned)p->type,
                (unsigned)p->subtype,
                (unsigned)p->address,
                (unsigned)p->size,
                p->label);
}

static void dumpPartitions() {
  Serial.println("\n== Partitions ==");

  // あなたの実機ログで出ている既知ラベル
  static const char* labels[] = {
    "nvs",
    "otadata",
    "app0",
    "app1",
    "ffat",
    "coredump"
  };

  for (size_t i = 0; i < sizeof(labels) / sizeof(labels[0]); ++i) {
    printPartitionByLabel(labels[i]);
  }
}

static void dumpI2CScan() {
  Serial.println("\n== I2C scan (internal bus) ==");
  for (uint8_t a = 0x08; a <= 0x77; ++a) {
    if (i2cExists(a)) {
      Serial.printf("FOUND 0x%02X", a);

      if (a == 0x34) Serial.print("  (PMIC?)");
      if (a == 0x36) Serial.print("  (Amp AW88298)");
      if (a == 0x38) Serial.print("  (Touch controller?)");
      if (a == 0x40) Serial.print("  (Audio codec ES7210)");
      if (a == 0x51) Serial.print("  (RTC?)");
      if (a == 0x58) Serial.print("  (IO expander AW9523)");
      if (a == 0x68) Serial.print("  (IMU? MPU6886/MPU9250系)");
      if (a == 0x69) Serial.print("  (IMU? BMI270)");

      Serial.println();
    }
  }
}

void setup() {
  auto cfg = M5.config();
  cfg.serial_baudrate = 115200;
  M5.begin(cfg);

  decideInternalI2CPins();

  Serial.println("\n===== M5Stack Hardware Probe =====");

  Serial.printf("M5.getBoard() = %d\n", (int)M5.getBoard());
  Serial.printf("Detected board = %s\n", g_board_name);

  esp_chip_info_t chip;
  esp_chip_info(&chip);
  Serial.printf("Chip model=%s  rev=%d  cores=%d\n",
                ESP.getChipModel(),
                (int)ESP.getChipRevision(),
                (int)ESP.getChipCores());
  Serial.printf("CPU freq=%d MHz\n", (int)ESP.getCpuFreqMHz());
  Serial.printf("SDK=%s\n", ESP.getSdkVersion());

  Serial.printf("Flash size=%u bytes  speed=%u Hz  mode=%d\n",
                (unsigned)ESP.getFlashChipSize(),
                (unsigned)ESP.getFlashChipSpeed(),
                (int)ESP.getFlashChipMode());
  Serial.printf("PSRAM found=%d  size=%u bytes  free=%u bytes\n",
                (int)psramFound(),
                (unsigned)ESP.getPsramSize(),
                (unsigned)ESP.getFreePsram());

  Serial.printf("Heap free=%u bytes  min_free=%u bytes\n",
                (unsigned)ESP.getFreeHeap(),
                (unsigned)ESP.getMinFreeHeap());

  dumpPartitions();

  Serial.printf("\nInternal I2C pins: SDA=%d SCL=%d\n", g_i2c_sda, g_i2c_scl);
  Wire.begin(g_i2c_sda, g_i2c_scl, 400000);
  dumpI2CScan();

  Serial.println("===== Probe done =====");
}

void loop() {
  delay(1000);
}