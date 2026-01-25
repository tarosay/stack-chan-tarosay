#ifndef SDCARD_HPP
#define SDCARD_HPP

#include <Arduino.h>
#include <M5Unified.h>
#include <SD.h>

class SdCard {
public:
  // コンストラクタ
  SdCard();
  size_t countWiFiEntriesInSD(const char* path);
  bool begin(
    uint8_t ssPin = SS, SPIClass& spi = SPI, uint32_t frequency = 4000000, const char* mountpoint = "/sd", uint8_t max_files = 5, bool format_if_empty = false);
  bool loadWiFiByIndexFromSD(size_t index, String& ssid, String& password, const char* path);

private:
  bool extractQuotedOrBareValue(const String& line, String& out);
};

// グローバルインスタンスの宣言
extern SdCard sdCard;

#endif