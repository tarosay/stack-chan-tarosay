#ifndef WIFICONNECT_HPP
#define WIFICONNECT_HPP

#include <Arduino.h>
#include <M5Unified.h>
#include <WiFi.h>

#define WIFISSID "/SC_SecConfig.yaml"

class WifiConnect {
public:
  // コンストラクタ
  WifiConnect();

  // これで wifiConnect.WiFi.disconnect(...) ができる
  WiFiClass& WiFi;
  String ipAddress;

  bool connectToWiFi(const char* path = WIFISSID);

private:
};

// グローバルインスタンスの宣言
extern WifiConnect wifiConnect;

#endif