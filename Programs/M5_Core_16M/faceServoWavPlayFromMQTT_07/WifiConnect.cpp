#include "WifiConnect.hpp"
#include "JsonRead.hpp"

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

  size_t n = jsonRead.countEntries(path, "wifi");
  //Serial.printf("n=%d\n", n);

  if (n == 0) {
    return false;
  }

  String ssid, pass;
  const unsigned long timeoutMs = 10 * 1000;
  size_t kai = 2;
  size_t index;
  bool ok;
  for (size_t i = 0; i < n * kai; i++) {
    index = i % n;
    // 前状態を確実に止める（radio OFF）
    WiFi.disconnect(true, false);
    delay(100);

    ok = jsonRead.loadDataByIndex(path, "wifi", i, "ssid", ssid, "password", &pass);
    // Serial.printf("ok=%d SSID=%s PASS=%s\n", ok, ssid.c_str(), pass.c_str());  // (disabled) avoid verbose log + password leak

    if (!ok) {
      //WiFi.disconnect(true, false);
      return false;
    }
    // M5_LOGI("wifi #%u ssid=%s", (unsigned)i, ssid.c_str());  // (disabled) verbose

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
  const char* id,
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
  if (!id || !*id) return false;
  if (WiFi.status() != WL_CONNECTED) return false;

  // ブロードキャストアドレス計算（例: 192.168.137.255）
  IPAddress bcast = ~WiFi.subnetMask() | WiFi.localIP();

  WiFiUDP udp;
  if (!udp.begin(0)) return false;  // ローカルポート自動確保
  udp.setTimeout(timeoutMs);

  // 送信 payload = "MAGIC:ID"
  String query = String(magic) + ":" + String(id);

  char buf[128];

  // 受信側の期待 suffix = ":ID"
  String expectSuffix = ":" + String(id);

  for (uint8_t i = 0; i < retries; i++) {
    udp.beginPacket(bcast, servicePort);
    udp.write((const uint8_t*)query.c_str(), query.length());
    udp.endPacket();

    uint32_t t0 = millis();
    while (millis() - t0 < timeoutMs) {
      int len = udp.parsePacket();
      if (len > 0) {
        int r = udp.read(buf, (int)sizeof(buf) - 1);
        if (r > 0) {
          buf[r] = '\0';
          String reply(buf);

          // ★IDが付いていて、かつ一致するものだけ成功
          // 例: "MQTT:1883:ABC123" の末尾が ":ABC123" ならOK
          if (reply.endsWith(expectSuffix)) {
            outReply = reply;
            outFromIp = udp.remoteIP();
            outFromPort = udp.remotePort();
            udp.stop();
            return true;
          }
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
  const char* id,
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

  if (!magicQueryBroadcast(magic, id, servicePort, reply, fromIp, fromPort, timeoutMs, retries)) {
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
