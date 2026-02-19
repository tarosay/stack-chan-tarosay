// M5Stack Core(初代) 向け：Flash/Sketch/Partition を表示（改良・日本語版）
// - Flash実測(esp_flash_get_size) + Arduino API(ChipSize) 併記
// - JEDEC ID（フラッシュID）を表示して「物理4MB/16MB」を確定
// - Partition subtype 名を拡充（otadata/coredump/littlefs 等）
// - 一時Stringのc_str()寿命問題を排除
// - B と MiB を併記、合計サイズの整合チェックも表示
// Arduino-ESP32 / m5stack:esp32 v3.2.x（IDF v5系想定）
// M5Unified があればLCD表示、なければシリアルのみ

#include <Arduino.h>
#include "esp_system.h"
#include "esp_partition.h"

#if __has_include(<esp_idf_version.h>)
#include <esp_idf_version.h>
#else
#define ESP_IDF_VERSION_MAJOR 4
#define ESP_IDF_VERSION_MINOR 0
#define ESP_IDF_VERSION_PATCH 0
#endif

#if (ESP_IDF_VERSION_MAJOR >= 5)
#if __has_include(<esp_flash.h>)
#include "esp_flash.h"
#endif
#else
#include "esp_spi_flash.h"
#endif

#if __has_include(<M5Unified.h>)
#include <M5Unified.h>
#define USE_M5UNIFIED 1
#else
#define USE_M5UNIFIED 0
#endif

// ---------------- Utility ----------------
static inline float toMiB(uint32_t bytes) {
  return (float)bytes / 1024.0f / 1024.0f;
}

static void lcdPrintln(const String& s) {
#if USE_M5UNIFIED
  static bool inited = false;
  static int y = 0;
  if (!inited) {
    auto cfg = M5.config();
    M5.begin(cfg);
    M5.Display.fillScreen(TFT_BLACK);
    M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
    M5.Display.setTextSize(2);
    M5.Display.setCursor(0, 0);
    inited = true;
  }
  M5.Display.println(s);
  y += 16;
  if (y > (int)M5.Display.height() - 16) {
    y = 0;
    M5.Display.fillScreen(TFT_BLACK);
    M5.Display.setCursor(0, 0);
  }
#else
  (void)s;
#endif
}

static void out(const String& s) {
  Serial.println(s);
  lcdPrintln(s);
}

static const char* partTypeToStr(esp_partition_type_t t) {
  switch (t) {
    case ESP_PARTITION_TYPE_APP: return "APP";
    case ESP_PARTITION_TYPE_DATA: return "DATA";
    default: return "UNKNOWN";
  }
}

// DATA系のよくあるsubtypeを補完（環境で定義が無いものもあるので#ifdefで守る）
static const char* partSubTypeToStr(esp_partition_subtype_t st) {
  switch (st) {
    // APP
    case ESP_PARTITION_SUBTYPE_APP_FACTORY: return "factory";
    case ESP_PARTITION_SUBTYPE_APP_OTA_0: return "ota_0";
    case ESP_PARTITION_SUBTYPE_APP_OTA_1: return "ota_1";

    // DATA
    case ESP_PARTITION_SUBTYPE_DATA_NVS: return "nvs";
    case ESP_PARTITION_SUBTYPE_DATA_PHY: return "phy";
    case ESP_PARTITION_SUBTYPE_DATA_FAT: return "fat";
    case ESP_PARTITION_SUBTYPE_DATA_SPIFFS: return "spiffs";

#ifdef ESP_PARTITION_SUBTYPE_DATA_OTA
    case ESP_PARTITION_SUBTYPE_DATA_OTA: return "otadata";
#endif
#ifdef ESP_PARTITION_SUBTYPE_DATA_COREDUMP
    case ESP_PARTITION_SUBTYPE_DATA_COREDUMP: return "coredump";
#endif
#ifdef ESP_PARTITION_SUBTYPE_DATA_NVS_KEYS
    case ESP_PARTITION_SUBTYPE_DATA_NVS_KEYS: return "nvs_keys";
#endif
#ifdef ESP_PARTITION_SUBTYPE_DATA_EFUSE_EM
    case ESP_PARTITION_SUBTYPE_DATA_EFUSE_EM: return "efuse_em";
#endif
#ifdef ESP_PARTITION_SUBTYPE_DATA_LITTLEFS
    case ESP_PARTITION_SUBTYPE_DATA_LITTLEFS: return "littlefs";
#endif
    default: return nullptr;
  }
}

// IDF/Arduinoの両系統でFlash容量を取得（可能な限り“実測”優先）
static uint32_t get_flash_total_bytes() {
#if (ESP_IDF_VERSION_MAJOR >= 5) && __has_include(<esp_flash.h>)
  uint32_t sz = 0;
  if (esp_flash_get_size(NULL, &sz) == ESP_OK && sz > 0) return sz;
  return ESP.getFlashChipSize();
#else
#if __has_include(<esp_spi_flash.h>)
  return spi_flash_get_chip_size();
#else
  return ESP.getFlashChipSize();
#endif
#endif
}

static void printBytesLine(const char* label, uint32_t bytes) {
  char buf[128];
  snprintf(buf, sizeof(buf), "%s: %u B (%.2f MiB)", label, (unsigned)bytes, toMiB(bytes));
  out(buf);
}

// JEDEC ID（メーカー/容量判定の決め手）を表示
static void printJedecId() {
#if (ESP_IDF_VERSION_MAJOR >= 5) && __has_include(<esp_flash.h>)
  uint32_t id = 0;
  esp_err_t err = esp_flash_read_id(NULL, &id);  // NULL=内蔵フラッシュ
  if (err == ESP_OK) {
    char buf[64];
    snprintf(buf, sizeof(buf), "JEDEC ID: 0x%06X", (unsigned)id);
    out(buf);
  } else {
    out("JEDEC ID: 読み取り失敗");
  }
#else
  out("JEDEC ID: このビルドでは未対応");
#endif
}

void setup() {
  Serial.begin(115200);
  delay(1200);

#if USE_M5UNIFIED
  out("=== Flash / Sketch 情報 (LCD+Serial) ===");
#else
  out("=== Flash / Sketch 情報 (Serialのみ) ===");
#endif

  const uint32_t flash_total = get_flash_total_bytes();
  const uint32_t sketch_size = ESP.getSketchSize();
  const uint32_t free_sketch = ESP.getFreeSketchSpace();
  const uint32_t heap_free = ESP.getFreeHeap();
  const uint32_t psram_total = ESP.getPsramSize();
  const uint32_t psram_free = ESP.getFreePsram();

  printBytesLine("Flash total", flash_total);
  printBytesLine("Sketch size", sketch_size);
  printBytesLine("Sketch free", free_sketch);
  printBytesLine("Heap free  ", heap_free);

  // Arduino API（設定/ボード定義由来の可能性あり）も併記
  printBytesLine("FlashChipSize(Arduino)", ESP.getFlashChipSize());

  // 物理チップ確定用（最重要）
  printJedecId();

  if (psram_total) {
    printBytesLine("PSRAM total", psram_total);
    printBytesLine("PSRAM free ", psram_free);
  }

  out("");
  out("=== パーティションテーブル ===");

  uint32_t sum_sizes = 0;
  uint32_t app_max = 0;

  esp_partition_iterator_t it =
    esp_partition_find(ESP_PARTITION_TYPE_ANY, ESP_PARTITION_SUBTYPE_ANY, NULL);

  while (it) {
    const esp_partition_t* p = esp_partition_get(it);
    sum_sizes += p->size;
    if (p->type == ESP_PARTITION_TYPE_APP && p->size > app_max) app_max = p->size;

    const char* sub = partSubTypeToStr(p->subtype);
    char stbuf[24];
    if (!sub) {
      snprintf(stbuf, sizeof(stbuf), "sub:%u", (unsigned)p->subtype);
      sub = stbuf;
    }

    char line[220];
    snprintf(line, sizeof(line),
             "%-10s | %s/%-8s | addr 0x%06X | size %10u B (%.2f MiB)",
             (p->label ? p->label : "(no label)"),
             partTypeToStr(p->type),
             sub,
             (unsigned)p->address,
             (unsigned)p->size,
             toMiB(p->size));
    out(line);

    it = esp_partition_next(it);
  }
  esp_partition_iterator_release(it);

  out("");
  printBytesLine("最大APPスロット", app_max);

  {
    char buf[180];
    snprintf(buf, sizeof(buf),
             "パーティション合計: %u B (%.2f MiB)  [flash_total=%u B]",
             (unsigned)sum_sizes, toMiB(sum_sizes), (unsigned)flash_total);
    out(buf);
    out("注: 合計がFlash totalより小さいのは正常です（bootloader/パーティションテーブル/予約領域）。");
  }

  out("");
  out("ヒント: Sketch free は概ね「現在のAPPスロットサイズ」です（Partition Schemeで変わります）。");
}

void loop() {
  delay(1000);
}
