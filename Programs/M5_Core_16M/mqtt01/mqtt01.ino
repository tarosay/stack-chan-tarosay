#include <Arduino.h>
#include <M5Unified.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <PubSubClient.h>

#include "SdCard.hpp"
#include "WifiConnect.hpp"

// -----------------------------
// MQTT settings
// -----------------------------
static WiFiClient gNet;
static PubSubClient gMqtt(gNet);

static IPAddress gBrokerIp;
static uint16_t gBrokerPort = 1883;

static const char* TOPIC_SUB = "stack/mp3/#";
static const char* TOPIC_BEGIN = "stack/mp3/begin";
static const char* TOPIC_RAW = "stack/mp3/raw";
static const char* TOPIC_END = "stack/mp3/end";

// PubSubClient needs a buffer big enough to hold topic + payload.
// Node-RED side is publishing 8192-byte chunks, so keep some headroom.
static const uint16_t MQTT_BUF_SIZE = 16384;

// -----------------------------
// RX file state (SD)
// -----------------------------
static String gRxPath = "/rx.mp3";
static size_t gRxBytes = 0;
static uint32_t gLastRxMs = 0;
static bool gBeginSeen = false;

// Very small “JSON-ish” extractor (Node-RED begin payload is JSON string).
// Extracts value for "name":"...". If not found, returns empty string.
static String extractJsonStringField(const uint8_t* payload, unsigned int len, const char* key) {
  if (!payload || len == 0 || !key) return "";
  String s;
  s.reserve(len + 1);
  for (unsigned int i = 0; i < len; i++) s += (char)payload[i];

  String k = "\"";
  k += key;
  k += "\"";
  int p = s.indexOf(k);
  if (p < 0) return "";
  p = s.indexOf(':', p + k.length());
  if (p < 0) return "";
  while (p < (int)s.length() && (s[p] == ':' || s[p] == ' ' || s[p] == '\t')) p++;
  if (p >= (int)s.length() || s[p] != '"') return "";
  p++;
  int e = s.indexOf('"', p);
  if (e < 0) return "";
  return s.substring(p, e);
}

static void rxBegin(const uint8_t* payload, unsigned int len) {
  // Decide filename:
  String name = extractJsonStringField(payload, len, "name");
  if (name.length() == 0) {
    name = "rx_" + String(millis()) + ".mp3";
  }
  gBeginSeen = true;

  // sanitize (very conservative)
  name.replace("..", "_");
  name.replace("/", "_");
  name.replace("\\", "_");

  gRxPath = "/" + name;
  gRxBytes = 0;
  gLastRxMs = millis();

  // Truncate by removing then creating empty file
  SD.remove(gRxPath.c_str());
  File f = SD.open(gRxPath.c_str(), FILE_WRITE);
  if (f) {
    f.close();
    M5_LOGI("[RX] begin -> %s", gRxPath.c_str());
  } else {
    M5_LOGE("[RX] begin: failed to create %s", gRxPath.c_str());
  }
}

static void rxAppend(const uint8_t* payload, unsigned int len) {
  if (!payload || len == 0) return;

#if defined(FILE_APPEND)
  File f = SD.open(gRxPath.c_str(), FILE_APPEND);
#else
  File f = SD.open(gRxPath.c_str(), FILE_WRITE);
  if (f) f.seek(f.size());
#endif

  if (!f) {
    M5_LOGE("[RX] append: open failed %s", gRxPath.c_str());
    return;
  }

  size_t w = f.write(payload, len);
  f.close();

  gRxBytes += w;
  gLastRxMs = millis();

  M5_LOGI("[RX] raw %u bytes (w=%u, total=%u)", (unsigned)len, (unsigned)w, (unsigned)gRxBytes);
}

static void rxEnd() {
  M5_LOGI("[RX] end -> %s (total=%u bytes)", gRxPath.c_str(), (unsigned)gRxBytes);
  gBeginSeen = false;  // 次のセッションに備える（好みで）
}

static void mqttCallback(char* topic, uint8_t* payload, unsigned int length) {
  if (!topic) return;

  if (strcmp(topic, TOPIC_BEGIN) == 0) {
    rxBegin(payload, length);
    return;
  }

  if (strcmp(topic, TOPIC_RAW) == 0) {
    // If begin wasn't received (or you publish raw-only), start a fresh file when idle gap is large.
    const uint32_t now = millis();
    if (!gBeginSeen || (now - gLastRxMs) > 1500) {
      const char* fallback = "{\"name\":\"rx_raw.mp3\"}";
      rxBegin((const uint8_t*)fallback, strlen(fallback));
    }
    rxAppend(payload, length);
    return;
  }

  if (strcmp(topic, TOPIC_END) == 0) {
    rxEnd();
    return;
  }

  // Other topics under stack/mp3/# (if any)
  M5_LOGI("[MQTT] topic=%s len=%u", topic, (unsigned)length);
}

static bool mqttEnsureConnected() {
  if (gMqtt.connected()) return true;

  // unique-ish clientId
  uint32_t r = (uint32_t)esp_random();
  String clientId = "m5stack-" + String((uint32_t)ESP.getEfuseMac(), HEX) + "-" + String(r, HEX);

  M5_LOGI("[MQTT] connecting to %s:%u ...", gBrokerIp.toString().c_str(), (unsigned)gBrokerPort);

  if (!gMqtt.connect(clientId.c_str())) {
    M5_LOGW("[MQTT] connect failed, state=%d", gMqtt.state());
    return false;
  }

  if (!gMqtt.subscribe(TOPIC_SUB, 0)) {
    M5_LOGE("[MQTT] subscribe failed: %s", TOPIC_SUB);
    return false;
  }

  M5_LOGI("[MQTT] subscribed: %s", TOPIC_SUB);
  return true;
}

void setup() {
  Serial.begin(115200);
  delay(500);
  M5.begin();
  M5.Log.setLogLevel(m5::log_target_display, ESP_LOG_NONE);
  M5.Log.setLogLevel(m5::log_target_serial, ESP_LOG_INFO);
  M5.Log.setEnableColor(m5::log_target_serial, false);

  M5_LOGI("MQTT Test");

  if (!sdCard.begin(GPIO_NUM_4, SPI, 25000000)) {
    M5_LOGI("[ERROR] SDカードの初期化に失敗しました");
    while (true) delay(1);
  }

  if (!wifiConnect.connectToWiFi()) {
    M5_LOGI("[ERROR] WiFiの接続に失敗しました");
    while (true) delay(1);
  }

  M5_LOGI("ip = %s", wifiConnect.ipAddress.c_str());

  if (!wifiConnect.discoverMqttBroker("DISCOVER_MQTT_V1", gBrokerIp, gBrokerPort)) {
    M5_LOGI("[WARN] discoverMqttBroker failed");
    while (true) delay(1);
  }

  M5_LOGI("broker = %s:%u", gBrokerIp.toString().c_str(), (unsigned)gBrokerPort);

  gMqtt.setServer(gBrokerIp, gBrokerPort);
  gMqtt.setCallback(mqttCallback);
  gMqtt.setBufferSize(MQTT_BUF_SIZE);
  gMqtt.setKeepAlive(15);

  mqttEnsureConnected();
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    delay(200);
    return;
  }

  if (!gMqtt.connected()) {
    static uint32_t lastTry = 0;
    uint32_t now = millis();
    if (now - lastTry > 1000) {
      lastTry = now;
      mqttEnsureConnected();
    }
  }

  gMqtt.loop();
  delay(1);
}
