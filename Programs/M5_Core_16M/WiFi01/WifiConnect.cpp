#include "WifiConnect.hpp"
#include "SdCard.hpp"

// グローバルインスタンスの定義
WifiConnect wifiConnect;

WifiConnect::WifiConnect()
  : WiFi(::WiFi)  // ← ここが肝。参照メンバは必ず初期化子で束縛
    ,
    ipAddress("0.0.0.0") {
}

bool WifiConnect::connectToWiFi(const char* path) {
  ipAddress = "0.0.0.0";

  // “あとから勝手に繋がる” を避けたいなら自動再接続を切る
  WiFi.setAutoReconnect(false);  // :contentReference[oaicite:3]{index=3}

  size_t total = sdCard.countWiFiEntriesInSD(path);
  if (total == 0) {
    return false;
  }

  String ssid, pass;
  const unsigned long timeoutMs = 10 * 1000;
  size_t kai = 2;
  size_t index;
  for (size_t i = 0; i < total * kai; i++) {
    index = i % total;
    // 前状態を確実に止める（radio OFF）
    WiFi.disconnect(true, false);
    delay(100);

    if (!sdCard.loadWiFiByIndexFromSD(i, ssid, pass, path)) {
      //WiFi.disconnect(true, false);
      return false;
    }
    M5_LOGI("wifi #%u ssid=%s", (unsigned)i, ssid.c_str());

    WiFi.begin(ssid.c_str(), pass.c_str());

    uint8_t r = WiFi.waitForConnectResult(timeoutMs);

    // “connected だけどIP未取得” を避けるため、localIPも見る
    if (r == WL_CONNECTED && WiFi.localIP() != IPAddress(0, 0, 0, 0)) {
      ipAddress = WiFi.localIP().toString();
      return true;
    }
  }
  // 失敗/タイムアウト：後から繋がる芽を潰す
  WiFi.disconnect(true, false);
  return false;
}
