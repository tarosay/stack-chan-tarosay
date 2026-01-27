#include <Arduino.h>
#include <M5Unified.h>
#include <SD.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

#include "WavStreamPlayer.hpp"
#include "SpiBusLock.hpp"
#include "WifiConnect.hpp"
#include "JsonRead.hpp"

//static const char* WAV_PATH = "/wav/UchuuKomichi.wav";
//static const char* WAV_PATH = "/upload.wav";
#ifndef SDCARD_CSPIN
#define SDCARD_CSPIN 4
#endif

static IPAddress gBrokerIp;
static uint16_t gBrokerPort = 1883;

//WavStreamPlayer player(32768, 2, 0);
WavStreamPlayer player(24576, 2, 0, 65536);

// MQTT
static WiFiClient gWiFiClient;
static PubSubClient gMqtt(gWiFiClient);

// stream state
static String gActiveSid = "";
static bool gStreaming = false;
static uint32_t gExpectSeq = 0;

static bool endsWith(const char* s, const char* suf) {
  size_t ls = strlen(s), l = strlen(suf);
  if (ls < l) return false;
  return memcmp(s + (ls - l), suf, l) == 0;
}

// topic: pcm16/<sid>/ctrl or pcm16/<sid>/pcm
static bool extractSid(const char* topic, String& sidOut) {
  const char* pfx = "pcm16/";
  size_t lp = strlen(pfx);
  if (strncmp(topic, pfx, lp) != 0) return false;

  const char* p = topic + lp;
  const char* slash = strchr(p, '/');
  if (!slash) return false;

  sidOut = String(p).substring(0, (int)(slash - p));
  return sidOut.length() > 0;
}

static void mqttEnsureConnected() {
  if (gMqtt.connected()) return;

  gMqtt.setServer(gBrokerIp, gBrokerPort);

  // clientId は適当にユニークに
  String cid = "M5PCM16-" + String((uint32_t)ESP.getEfuseMac(), HEX);

  while (!gMqtt.connected()) {
    M5_LOGI("MQTT connecting...");
    if (gMqtt.connect(cid.c_str())) {
      M5_LOGI("MQTT connected");

      bool ok1 = gMqtt.subscribe("pcm16/+/ctrl");
      bool ok2 = gMqtt.subscribe("pcm16/+/pcm");
      M5_LOGI("sub ctrl=%d pcm=%d", ok1, ok2);
    } else {
      M5_LOGI("MQTT connect failed rc=%d", gMqtt.state());
      delay(500);
    }
  }
}

static inline uint32_t u32le(const uint8_t* p) {
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static inline uint16_t u16le(const uint8_t* p) {
  return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static void onCtrlBegin(JsonDocument& doc) {
  const char* sid = doc["sid"] | "";
  uint32_t sr = doc["sr"] | 0;
  uint16_t ch = doc["ch"] | 0;
  uint16_t bits = doc["bits"] | 0;
  const char* codec = doc["codec"] | "";
  uint32_t seq0 = doc["seq0"] | 0;
  uint32_t chunkBytes = doc["chunkBytes"] | 4096;

  if (!sid[0] || sr == 0 || !(ch == 1 || ch == 2) || bits != 16) {
    M5_LOGI("[CTRL] begin invalid");
    return;
  }
  if (strcmp(codec, "pcm16") != 0) {
    M5_LOGI("[CTRL] begin codec != pcm16 (%s)", codec);
    return;
  }

  // 既存ストリーム停止→新規開始
  player.streamClear();

  if (!player.streamBegin(sr, ch, bits, chunkBytes)) {
    M5_LOGI("[CTRL] streamBegin failed");
    gActiveSid = "";
    gStreaming = false;
    return;
  }

  gActiveSid = sid;
  gStreaming = true;
  gExpectSeq = seq0;

  M5_LOGI("[CTRL] begin sid=%s sr=%u ch=%u bits=%u chunk=%u seq0=%u",
          gActiveSid.c_str(), (unsigned)sr, (unsigned)ch, (unsigned)bits,
          (unsigned)chunkBytes, (unsigned)seq0);
}

static void onCtrlEnd(JsonDocument& doc) {
  const char* sid = doc["sid"] | "";
  if (!sid[0]) return;

  if (!gStreaming || gActiveSid != sid) {
    M5_LOGI("[CTRL] end sid mismatch (got=%s active=%s)", sid, gActiveSid.c_str());
    return;
  }

  M5_LOGI("[CTRL] end sid=%s lastSeq=%u chunks=%u pcmBytes=%u",
          sid,
          (unsigned)(doc["lastSeq"] | 0),
          (unsigned)(doc["chunks"] | 0),
          (unsigned)(doc["pcmBytes"] | 0));

  player.streamEnd();  // FIFOが枯れて停止（A側実装の方針）
  gStreaming = false;
  gActiveSid = "";
}

static void mqttCallback(char* topic, uint8_t* payload, unsigned int length) {
  if (!topic || !payload || length == 0) return;

  String sid;
  if (!extractSid(topic, sid)) return;

  // ctrl
  if (endsWith(topic, "/ctrl")) {
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, payload, length);
    if (err) {
      M5_LOGI("[CTRL] JSON parse error");
      return;
    }

    const char* type = doc["type"] | "";
    if (strcmp(type, "begin") == 0) {
      onCtrlBegin(doc);
    } else if (strcmp(type, "end") == 0) {
      onCtrlEnd(doc);
    } else {
      M5_LOGI("[CTRL] unknown type=%s", type);
    }
    return;
  }

  // pcm
  if (endsWith(topic, "/pcm")) {
    //M5_LOGI("[PCM] rx topic=%s len=%u", topic, (unsigned)length);

    if (!gStreaming || sid != gActiveSid) {
      // active 以外は捨てる
      return;
    }
    if (length < 8) return;

    uint32_t seq = u32le(payload + 0);
    uint16_t payloadBytes = u16le(payload + 4);
    uint16_t flags = u16le(payload + 6);

    const uint8_t* pcm = payload + 8;
    size_t pcmLen = (size_t)length - 8;

    // 任意検査：payloadBytes が一致しない時はログだけ
    if (payloadBytes != 0 && payloadBytes != pcmLen) {
      M5_LOGI("[PCM] payloadBytes mismatch hdr=%u actual=%u",
              (unsigned)payloadBytes, (unsigned)pcmLen);
    }

    // seq 飛び検知（まずログのみ）
    if (seq != gExpectSeq) {
      M5_LOGI("[PCM] seq jump got=%u expect=%u", (unsigned)seq, (unsigned)gExpectSeq);
      gExpectSeq = seq;  // とりあえず追従
    }
    gExpectSeq++;

    // FIFOへ投入（満杯なら drop）
    if (!player.streamPush(pcm, pcmLen)) {
      M5_LOGI("[PCM] drop (fifo full?) seq=%u len=%u", (unsigned)seq, (unsigned)pcmLen);
    }

    // flags last（現状0x0002）
    if (flags & 0x0002) {
      // last chunk が来た、という合図を使うならここで streamEnd してもOK
      // player.streamEnd();
    }
    return;
  }
}


void setup() {
  Serial.begin(115200);

  auto cfg = M5.config();
  M5.begin(cfg);
  M5.Log.setLogLevel(m5::log_target_display, ESP_LOG_NONE);
  M5.Log.setLogLevel(m5::log_target_serial, ESP_LOG_INFO);
  M5.Log.setEnableColor(m5::log_target_serial, false);

  M5_LOGI("PCM16 MQTT SUB -> WavStreamPlayer");
  delay(200);

  //画面とSDのSPI競合を避けるためのmutexセット
  ensure_spi_mutex();

  bool sdOK = false;
  {
    SpiGuard g;
    sdOK = SD.begin(SDCARD_CSPIN, SPI, 25000000);
  }

  if (!sdOK) {
    Serial.println("SD init failed");
    for (;;) delay(1000);
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

  auto spk = M5.Speaker.config();
  M5.Speaker.config(spk);
  M5.Speaker.begin();
  M5.Speaker.setVolume(180);

  player.beginAsync(4096, 2, 1, 1);  // stack, prio, core, periodMs

  // MQTT
  gMqtt.setBufferSize(8192);  // ★これが本命（topic含めても余裕）
  gMqtt.setCallback(mqttCallback);
  mqttEnsureConnected();

  {
    SpiGuard g;
    M5.Display.clear();
    M5.Display.setTextSize(2);
    //M5.Display.println("A=Play  B=Stop");
  }
}

void loop() {
  M5.update();

  mqttEnsureConnected();
  gMqtt.loop();

  if (M5.BtnB.wasPressed()) {
    player.streamEnd();    // drain/stop
    player.streamClear();  // immediate clear
    gStreaming = false;
    gActiveSid = "";
    M5_LOGI("manual stop");
  }

  delay(5);
}
