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
#include "MqttRouter.hpp"
#include "Pcm16StreamHandler.hpp"

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
static MqttRouter gRouter(gMqtt);

static Pcm16StreamHandler gPcm16(player);

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
  gRouter.begin(gBrokerIp, gBrokerPort, 8192);
  // PCM16 handler を登録（ここだけ）
  gRouter.addSubscription("pcm16/+/ctrl", [&](const char* t, uint8_t* p, unsigned int n) {
    gPcm16.handle(t, p, n);
  });
  gRouter.addSubscription("pcm16/+/pcm", [&](const char* t, uint8_t* p, unsigned int n) {
    gPcm16.handle(t, p, n);
  });

  {
    SpiGuard g;
    M5.Display.clear();
    M5.Display.setTextSize(2);
    //M5.Display.println("A=Play  B=Stop");
  }
}

void loop() {
  M5.update();

  gRouter.loop();

  if (M5.BtnB.wasPressed()) {
    gPcm16.stop(true);
  }
  delay(5);
}
