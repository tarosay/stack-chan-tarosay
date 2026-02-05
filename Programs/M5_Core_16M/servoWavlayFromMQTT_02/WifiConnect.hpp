#ifndef WIFICONNECT_HPP
#define WIFICONNECT_HPP

#include <Arduino.h>
#include <M5Unified.h>
#include <WiFi.h>
#include <WiFiUdp.h>

#define WIFISSID "/json/SC_SecConfig.json"

class WifiConnect {
public:
  // コンストラクタ
  WifiConnect();

  // これで wifiConnect.WiFi.disconnect(...) ができる
  WiFiClass& WiFi;
  String ipAddress;

  bool connectToWiFi(const char* path = WIFISSID);

  // UDPブロードキャストで MAGIC を投げ、最初に返ってきた応答を返す（mDNS不要）
  // - outReply     : 受信payload（UTF-8文字列）
  // - outFromIp    : 応答元IP
  // - outFromPort  : 応答元ポート（通常は不要。デバッグ用）
  // - timeoutMs    : 1回の待ち時間
  // - retries      : 送信回数（取りこぼし対策）
  bool magicQueryBroadcast(
    const char* magic,
    uint16_t servicePort,
    String& outReply,
    IPAddress& outFromIp,
    uint16_t& outFromPort,
    uint32_t timeoutMs = 300,
    uint8_t retries = 5);

  // 「DISCOVER_MQTT_V1」→「MQTT:<port>」を前提に、ブローカ(IP/port)を返す
  // - outBrokerIp   : 応答元IP（=ブローカがいるホスト）
  // - outBrokerPort : 応答payloadのポート（例: 1883）
  bool discoverMqttBroker(
    const char* magic,
    IPAddress& outBrokerIp,
    uint16_t& outBrokerPort,
    uint16_t servicePort = 45678,
    uint32_t timeoutMs = 300,
    uint8_t retries = 5);

private:
};

// グローバルインスタンスの宣言
extern WifiConnect wifiConnect;

#endif