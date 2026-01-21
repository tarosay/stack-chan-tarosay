// SD_Card_WiFiList.ino
// M5Stack Core (初期M5Stack) + M5Unified + SD.h
// SD root: /SC_SecConfig.yaml
//
// 推奨YAML形式（wifi配列）:
// wifi:
//   - ssid: "higashi-2f"
//     password: "hama1297noiti"
//   - ssid: "office"
//     password: "xxxx"

#include <Arduino.h>
#include <M5Unified.h>
//#include <SPI.h>
//#include <SD.h>
#include <WiFi.h>

#include "SdCard.hpp"
#include "WifiConnect.hpp"

void setup() {
  Serial.begin(115200);
  delay(500);
  M5.begin();
  M5.Log.setLogLevel(m5::log_target_display, ESP_LOG_NONE);
  M5.Log.setLogLevel(m5::log_target_serial, ESP_LOG_INFO);
  M5.Log.setEnableColor(m5::log_target_serial, false);

  M5_LOGI("SD_Card WiFiList Test");

  if (!sdCard.begin(GPIO_NUM_4, SPI, 25000000)) {
    M5_LOGI("[ERROR] SDカードの初期化に失敗しました");
    while (true) delay(1);
  }

  if (!wifiConnect.connectToWiFi()) {
    M5_LOGI("[ERROR] WiFiの接続に失敗しました");
    while (true) delay(1);
  }

  M5_LOGI("ip = %s", wifiConnect.ipAddress.c_str());
}

void loop() {}
