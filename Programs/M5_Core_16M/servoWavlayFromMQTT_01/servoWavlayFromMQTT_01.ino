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
#include "VolumeHandler.hpp"
#include "ServoXY.hpp"

#ifndef SDCARD_CSPIN
#define SDCARD_CSPIN 4
#endif

static IPAddress gBrokerIp;
static uint16_t gBrokerPort = 1883;


WavStreamPlayer player(4096, 2, 0, 28672);
ServoXY servo;

// MQTT
static WiFiClient gWiFiClient;
static PubSubClient gMqtt(gWiFiClient);
static MqttRouter gRouter(gMqtt);

static Pcm16StreamHandler gPcm16(player);
static VolumeHandler gVol(player, 180);

static TaskHandle_t hMqttTask = nullptr;
static TaskHandle_t hDispatchTask = nullptr;

void mqttTask(void*) {
  for (;;) {
    gRouter.loop();  // 受信を回す
    vTaskDelay(1);   // 1tick
  }
}

void dispatchTask(void*) {
  for (;;) {
    gRouter.dispatchOne(portMAX_DELAY);  // 受信キューを処理（ブロック待ち）
  }
}

void setup() {
  Serial.begin(115200);

  auto cfg = M5.config();
  M5.begin(cfg);
  M5.Log.setLogLevel(m5::log_target_display, ESP_LOG_NONE);
  M5.Log.setLogLevel(m5::log_target_serial, ESP_LOG_WARN);
  M5.Log.setEnableColor(m5::log_target_serial, false);

  // M5_LOGI("PCM16 MQTT SUB -> WavStreamPlayer");  // (disabled) verbose
  // M5_LOGI("[BUILD] %s %s", __DATE__, __TIME__);  // (disabled) verbose

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

  if (!servo.begin()) {
    Serial.println("servo begin error");
    servo.begin(0, 0);
  }

  if (!wifiConnect.connectToWiFi()) {
    M5_LOGE("[ERROR] WiFiの接続に失敗しました");
    while (true) delay(1);
  }
  // M5_LOGI("ip = %s", wifiConnect.ipAddress.c_str());  // (disabled) verbose

  if (!wifiConnect.discoverMqttBroker("DISCOVER_MQTT_V1", gBrokerIp, gBrokerPort)) {
    M5_LOGW("[WARN] discoverMqttBroker failed");
    while (true) delay(1);
  }
  // M5_LOGI("broker = %s:%u", gBrokerIp.toString().c_str(), (unsigned)gBrokerPort);  // (disabled) verbose

  auto spk = M5.Speaker.config();
  M5.Speaker.config(spk);
  M5.Speaker.begin();
  gVol.apply();

  player.beginAsync(4096, 3, 1, 1);  // prioを上げる（音声を優先）

  // MQTT
  gRouter.enableAsyncDispatch(8);  // ★まず有効化（キュー深さは適当に）
  gRouter.begin(gBrokerIp, gBrokerPort, 5120);

  //Subの追加セット
  gRouter.addSubscription("pcm16/+/ctrl", [&](const char* t, uint8_t* p, unsigned int n) {
    gPcm16.handle(t, p, n);
  });
  gRouter.addSubscription("pcm16/+/pcm", [&](const char* t, uint8_t* p, unsigned int n) {
    gPcm16.handle(t, p, n);
  });
  // Volume control (plain integer payload): device/+/audio/vol
  gRouter.addSubscription("device/+/audio/vol", [&](const char* t, uint8_t* p, unsigned int n) {
    gVol.handle(t, p, n);
  });

  // タスク起動
  xTaskCreatePinnedToCore(mqttTask, "mqttTask", 3072, nullptr, 2, &hMqttTask, 0);
  xTaskCreatePinnedToCore(dispatchTask, "dispatchTask", 3072, nullptr, 1, &hDispatchTask, 0);

  {
    SpiGuard g;
    M5.Display.clear();
    M5.Display.setTextSize(2);
    M5.Display.println("A=Stop B=Up C=Right");
  }
}

void loop() {
  M5.update();

  if (M5.BtnA.wasPressed()) {
    servo.moveBlocking(0, 0, 1500);
    gPcm16.stop(true);
      }

  if (M5.BtnB.wasPressed()) {
    servo.moveBlocking(0, -45, 1500);
  }

  if (M5.BtnC.wasPressed()) {
    servo.moveBlocking(-90, 0, 1500);
  }

  delay(5);
}
