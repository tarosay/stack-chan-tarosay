#include <Arduino.h>
#include <M5Unified.h>
#include <SD.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <esp_system.h>

#include "WavStreamPlayer.hpp"
#include "SpiBusLock.hpp"
#include "WifiConnect.hpp"
#include "JsonRead.hpp"
#include "MqttRouter.hpp"
#include "Pcm16StreamHandler.hpp"
#include "VolumeHandler.hpp"
#include "ServoXY.hpp"
#include "ServoXYHandler.hpp"
#include "FaceAvatar.hpp"
#include "FaceHandler.hpp"
#include "config_paths.hpp"

#ifndef SDCARD_CSPIN
#define SDCARD_CSPIN 4
#endif

static IPAddress gBrokerIp;
static uint16_t gBrokerPort = 1883;

// Devices / services
WavStreamPlayer player(4096, 2, 0, 28672);
ServoXY servoXY;

// MQTT
static WiFiClient gWiFiClient;
static PubSubClient gMqtt(gWiFiClient);
static MqttRouter gRouter(gMqtt);

// Handlers
static Pcm16StreamHandler gPcm16StreamHandler(player);
static VolumeHandler gVolumeHandler(player, 180);
static ServoXYHandler gServoXYHandler(servoXY);
static FaceHandler gFaceHandler(faceAvatar);

static TaskHandle_t hMqttTask = nullptr;
static TaskHandle_t hDispatchTask = nullptr;

void mqttTask(void*) {
  for (;;) {
    if (WiFi.status() != WL_CONNECTED) {
      vTaskDelay(pdMS_TO_TICKS(200));
      continue;
    }

    gRouter.loop();  // 受信を回す
    if (!gMqtt.connected()) {
      vTaskDelay(pdMS_TO_TICKS(200));
    } else {
      vTaskDelay(pdMS_TO_TICKS(10));
    }
  }
}

void dispatchTask(void*) {
  for (;;) {
    gRouter.dispatchOne(portMAX_DELAY);  // 受信キューを処理（ブロック待ち）
  }
}

static void dumpHeap(const char* tag) {
  size_t free8 = heap_caps_get_free_size(MALLOC_CAP_8BIT);
  size_t large8 = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
  size_t freeInt = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
  size_t largeInt = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);
  Serial.printf("[HEAP] %s free8=%u largest8=%u freeInt=%u largestInt=%u\n",
                tag, (unsigned)free8, (unsigned)large8, (unsigned)freeInt, (unsigned)largeInt);
}

void setup() {
  Serial.begin(115200);

  auto cfg = M5.config();
  M5.begin(cfg);
  dumpHeap("after M5.begin");

  M5.Log.setLogLevel(m5::log_target_display, ESP_LOG_NONE);
  M5.Log.setLogLevel(m5::log_target_serial, ESP_LOG_WARN);
  M5.Log.setEnableColor(m5::log_target_serial, false);

  delay(1000);

  //画面とSDのSPI競合を避けるためのmutexセット
  ensure_spi_mutex();

  M5.Display.setTextSize(2);              // 文字サイズ
  M5.Display.setTextColor(WHITE, BLACK);  // (文字色, 背景色)
  M5.Display.setCursor(0, 0);             // 描画位置 (x,y)

  M5.Display.println("MQTT StackChan v0.9");

  bool sdOK = false;
  {
    SpiGuard g;
    sdOK = SD.begin(SDCARD_CSPIN, SPI, 25000000);
  }

  if (!sdOK) {
    Serial.println("SD init failed");
    M5.Display.println("SD init failed");
    for (;;) delay(1000);
  }
  dumpHeap("after SD.begin");

  M5.Display.println("Servo data reding.");
  if (!servoXY.begin()) {
    Serial.println("servo begin error");
    M5.Display.println("servo begin error");
    servoXY.begin(0, 0);
  }
  dumpHeap("after servoXY.begin");

  M5.Display.println("WiFi connecting.");
  if (!wifiConnect.connectToWiFi()) {
    M5_LOGE("[ERROR] WiFiの接続に失敗しました");
    M5.Display.println("[ERROR] WiFi connect");
    while (true) delay(1);
  }

  // M5_LOGI("ip = %s", wifiConnect.ipAddress.c_str());  // (disabled) verbose
  M5.Display.println("MQTT Broker Searching.");
  dumpHeap("after connectToWiFi");
  {
    String device_id;
    if (!jsonRead.loadDataMulti(config_paths::kBasicConfig, "mqtt", "device_id",
                                &device_id, JsonRead::ValueType::STR,
                                nullptr)) {
      M5_LOGE("[ERROR] デバイスIDの取得に失敗しました");
      M5.Display.println("[ERROR] Device id read");
      while (true) delay(1);
    }
    strlcpy(gRouter.device_id, device_id.c_str(), MQTTROUTER_DEVICE_ID_MAX);

    //M5_LOGE("device_id = %s", gRouter.device_id);
    M5.Display.printf("Router device id = %s\n", gRouter.device_id);

    if (!wifiConnect.discoverMqttBroker("DISCOVER_MQTT_V1", gRouter.device_id, gBrokerIp, gBrokerPort)) {
      M5_LOGW("[WARN] discoverMqttBroker failed");
      M5.Display.println("[ERROR]  discoverMqttBroker failed");
      while (true) delay(1);
    }
    // M5_LOGI("broker = %s:%u", gBrokerIp.toString().c_str(), (unsigned)gBrokerPort);  // (disabled) verbose
    M5.Display.printf("\nbroker = %s\n\n", gBrokerIp.toString().c_str());
    dumpHeap("after discoverMqttBroker");
  }

  M5.Display.println("Speaker setup.");
  auto spk_cfg = M5.Speaker.config();
  spk_cfg.dma_buf_count = 4;  // 8 → 4
  spk_cfg.dma_buf_len = 128;  // 256 → 128（まずはここ）
  M5.Speaker.config(spk_cfg);
  M5.Speaker.begin();
  gVolumeHandler.apply();
  dumpHeap("after Speaker.begin");

  player.beginAsync(2048, 3, 0, 1);  // prioを上げる（音声を優先）
  dumpHeap("after beginAsync");

  // MQTT
  M5.Display.println("MQTT setup.");
  gRouter.enableAsyncDispatch(8);  // ★まず有効化（キュー深さは適当に）
  gRouter.begin(gBrokerIp, gBrokerPort, 5120);
  dumpHeap("after gRouter.begin");

  BaseType_t ok1 = xTaskCreatePinnedToCore(mqttTask, "mqttTask", 3072, nullptr, 2, &hMqttTask, 0);
  BaseType_t ok2 = xTaskCreatePinnedToCore(dispatchTask, "dispatchTask", 3072, nullptr, 1, &hDispatchTask, 0);
  Serial.printf("[TASK] mqttTask=%d dispatchTask=%d hMqtt=%p hDisp=%p\n", (int)ok1, (int)ok2, hMqttTask, hDispatchTask);
  dumpHeap("after xTaskCreatePinnedToCore");

  delay(3000);

  //顔を出す（8bitを成立させたいので先にAvatar側を確保させる）
  faceAvatar.begin(4);
  dumpHeap("after faceAvatar");

  //Subの追加セット
  gRouter.addSubscription("pcm16/+/ctrl", [&](const char* t, uint8_t* p, unsigned int n) {
    gPcm16StreamHandler.handle(t, p, n);
  });
  gRouter.addSubscription("pcm16/+/pcm", [&](const char* t, uint8_t* p, unsigned int n) {
    gPcm16StreamHandler.handle(t, p, n);
  });
  gRouter.addSubscription("device/+/audio/vol", [&](const char* t, uint8_t* p, unsigned int n) {
    gVolumeHandler.handle(t, p, n);
  });
  gRouter.addSubscription("device/+/servoxy/#", [&](const char* t, uint8_t* p, unsigned int n) {
    (void)gServoXYHandler.handle(t, p, n);
  });
  // ...
  gRouter.addSubscription("device/+/face/#", [&](const char* t, uint8_t* p, unsigned int n) {
    (void)gFaceHandler.handle(t, p, n);
  });
}

void loop() {
  M5.update();
  servoXY.update();
  faceAvatar.update();

  if (M5.BtnA.wasPressed()) {
    servoXY.moveBlocking(0, 0, 1500);
  }

  if (M5.BtnB.wasPressed()) {
  }

  if (M5.BtnC.wasPressed()) {
  }
  delay(5);
}
