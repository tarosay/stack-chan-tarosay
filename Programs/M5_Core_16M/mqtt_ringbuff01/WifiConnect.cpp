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

bool WifiConnect::magicQueryBroadcast(
  const char* magic,
  uint16_t servicePort,
  String& outReply,
  IPAddress& outFromIp,
  uint16_t& outFromPort,
  uint32_t timeoutMs,
  uint8_t retries) {

  outReply = "";
  outFromIp = IPAddress(0, 0, 0, 0);
  outFromPort = 0;

  if (!magic || !*magic) return false;
  if (WiFi.status() != WL_CONNECTED) return false;

  // ブロードキャストアドレス計算（例: 192.168.137.255）
  IPAddress bcast = ~WiFi.subnetMask() | WiFi.localIP();

  WiFiUDP udp;
  if (!udp.begin(0)) return false;  // ローカルポート自動確保
  udp.setTimeout(timeoutMs);

  // 応答は UTF-8 文字列想定（例: "MQTT:1883"）
  char buf[128];

  for (uint8_t i = 0; i < retries; i++) {
    udp.beginPacket(bcast, servicePort);
    udp.write((const uint8_t*)magic, strlen(magic));
    udp.endPacket();

    uint32_t t0 = millis();
    while (millis() - t0 < timeoutMs) {
      int len = udp.parsePacket();
      if (len > 0) {
        int r = udp.read(buf, (int)sizeof(buf) - 1);
        if (r > 0) {
          buf[r] = '\0';
          outReply = String(buf);
          outFromIp = udp.remoteIP();
          outFromPort = udp.remotePort();
          udp.stop();
          return true;
        }
      }
      delay(10);
    }
  }

  udp.stop();
  return false;
}

static bool parseMqttPortFromReply(const String& reply, uint16_t& outPort) {
  // 受信例: "MQTT:1883"（末尾に改行が入っても許容）
  String s = reply;
  s.trim();

  const char* pfx = "MQTT:";
  if (!s.startsWith(pfx)) return false;

  const char* num = s.c_str() + strlen(pfx);
  if (!*num) return false;

  long port = 0;
  while (*num >= '0' && *num <= '9') {
    port = port * 10 + (*num - '0');
    if (port > 65535) return false;
    ++num;
  }

  if (port <= 0 || port > 65535) return false;
  outPort = (uint16_t)port;
  return true;
}

bool WifiConnect::discoverMqttBroker(
  const char* magic,
  IPAddress& outBrokerIp,
  uint16_t& outBrokerPort,
  uint16_t servicePort,
  uint32_t timeoutMs,
  uint8_t retries) {

  outBrokerIp = IPAddress(0, 0, 0, 0);
  outBrokerPort = 0;

  String reply;
  IPAddress fromIp;
  uint16_t fromPort;

  if (!magicQueryBroadcast(magic, servicePort, reply, fromIp, fromPort, timeoutMs, retries)) {
    return false;
  }

  uint16_t port;
  if (!parseMqttPortFromReply(reply, port)) {
    return false;
  }

  outBrokerIp = fromIp;
  outBrokerPort = port;
  return true;
}
