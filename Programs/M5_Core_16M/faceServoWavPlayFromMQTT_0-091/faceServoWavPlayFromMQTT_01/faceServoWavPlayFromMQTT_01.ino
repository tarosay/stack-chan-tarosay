#include <Arduino.h>
#include <M5Unified.h>
#include <SD.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <esp_system.h>
#include <esp_heap_caps.h>

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

static TaskHandle_t hMqttTask = nullptr;
static TaskHandle_t hDispatchTask = nullptr;

static void probe76800(const char* tag) {
  uint32_t freeInt = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
  uint32_t largestInt = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);
  Serial.printf("[PROBE] %s freeInt=%u largestInt=%u\n", tag, freeInt, largestInt);

  void* p = heap_caps_malloc(76800, MALLOC_CAP_INTERNAL);  // 320*240*1
  Serial.printf("[PROBE] %s malloc76800=%s\n", tag, p ? "OK" : "FAIL");
  if (p) heap_caps_free(p);
}

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

    //vTaskDelay(pdMS_TO_TICKS(10));
    //vTaskDelay(1);   // 1tick
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
  probe76800("after M5.begin");

  M5.Log.setLogLevel(m5::log_target_display, ESP_LOG_NONE);
  M5.Log.setLogLevel(m5::log_target_serial, ESP_LOG_WARN);
  M5.Log.setEnableColor(m5::log_target_serial, false);

  // M5_LOGI("PCM16 MQTT SUB -> WavStreamPlayer");  // (disabled) verbose
  // M5_LOGI("[BUILD] %s %s", __DATE__, __TIME__);  // (disabled) verbose

  delay(1500);

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
  probe76800("after SD.begin");
  MARK("after FaceAvatar.begin");

  MARK("before preallocStream");
  probe76800("before preallocStream");

  // ★ストリーム用に 28KB FIFO を先取り（これで later largest が小さくても落ちない）
  if (!player.preallocStream(28672, 8192)) {
    Serial.println("[FATAL] preallocStream failed");
    for (;;) delay(1000);
  }
  probe76800("after preallocStream");
  MARK("after preallocStream");

  if (!servoXY.begin()) {
    Serial.println("servo begin error");
    servoXY.begin(0, 0);
  }

  MARK("before wifiConnect.connectToWiFi");
  if (!wifiConnect.connectToWiFi()) {
    M5_LOGE("[ERROR] WiFiの接続に失敗しました");
    while (true) delay(1);
  }
  // M5_LOGI("ip = %s", wifiConnect.ipAddress.c_str());  // (disabled) verbose
  MARK("after connectToWiFi");

  MARK("before discoverMqttBroker");
  if (!wifiConnect.discoverMqttBroker("DISCOVER_MQTT_V1", gBrokerIp, gBrokerPort)) {
    M5_LOGW("[WARN] discoverMqttBroker failed");
    while (true) delay(1);
  }
  // M5_LOGI("broker = %s:%u", gBrokerIp.toString().c_str(), (unsigned)gBrokerPort);  // (disabled) verbose
  MARK("after discoverMqttBroker");

  auto spk = M5.Speaker.config();
  M5.Speaker.config(spk);
  M5.Speaker.begin();
  gVolumeHandler.apply();

  MARK("before speaker/player beginAsync");
  player.beginAsync(4096, 3, 0, 1);  // prioを上げる（音声を優先）
  MARK("after beginAsync");

  // MQTT
  gRouter.enableAsyncDispatch(8);  // ★まず有効化（キュー深さは適当に）
  gRouter.begin(gBrokerIp, gBrokerPort, 5120);

  //Subの追加セット
  gRouter.addSubscription("pcm16/+/ctrl", [&](const char* t, uint8_t* p, unsigned int n) {
    gPcm16StreamHandler.handle(t, p, n);
  });
  gRouter.addSubscription("pcm16/+/pcm", [&](const char* t, uint8_t* p, unsigned int n) {
    gPcm16StreamHandler.handle(t, p, n);
  });
  // Volume control (plain integer payload): device/+/audio/vol
  gRouter.addSubscription("device/+/audio/vol", [&](const char* t, uint8_t* p, unsigned int n) {
    gVolumeHandler.handle(t, p, n);
  });

  // ServoXY control (topic command): device/+/servoxy/<cmd>
  //   move  payload: "x y ms"
  //   stop  payload: ""
  //   home  payload: "ms"
  //   speed payload: "degps_x degps_y"
  //   pulse payload: "minX maxX minY maxY"
  gRouter.addSubscription("device/+/servoxy/#", [&](const char* t, uint8_t* p, unsigned int n) {
    (void)gServoXYHandler.handle(t, p, n);
  });

  // タスク起動
  MARK("before tasks create");
  xTaskCreatePinnedToCore(mqttTask, "mqttTask", 3072, nullptr, 2, &hMqttTask, 1);
  xTaskCreatePinnedToCore(dispatchTask, "dispatchTask", 3072, nullptr, 1, &hDispatchTask, 1);
  MARK("after tasks create");

  MARK("setup end");

  //顔を出す（8bitを成立させたいので先にAvatar側を確保させる）
  dumpHeap("before faceAvatar");
  probe76800("before faceAvatar.begin");
  //faceAvatar.begin();
  dumpHeap("after faceAvatar");
  // {
  //   SpiGuard g;
  //   // 画面が描けるか（SPI/LCDが生きてるか）を確認
  //   M5.Display.fillScreen(TFT_BLUE);
  //   delay(300);
  //   M5.Display.fillScreen(TFT_RED);
  //   delay(300);
  //   M5.Display.fillScreen(TFT_GREEN);
  //   delay(300);
  //   M5.Display.fillScreen(TFT_BLACK);
  //   delay(300);
  // }
  // Serial.println("[MARK] LCD fill test done");

  // {
  //   SpiGuard g;
  //   M5.Display.clear();
  //   M5.Display.setTextSize(2);
  //   M5.Display.println("A=Stop B=Up C=Right");
  //   M5.Display.println("MQTT sub: device/+/servoxy/#");
  // }
}

static void MARK(const char* s) {
  Serial.printf("[MARK] %lu %s\n", (unsigned long)millis(), s);
}


uint8_t i = 0;

void loop() {
  M5.update();
  servoXY.update();
  //faceAvatar.update();

  if (M5.BtnA.wasPressed()) {
    servoXY.moveBlocking(0, 0, 1500);
    gPcm16StreamHandler.stop(true);

    //if (faceAvatar.Speaking) {
      //faceAvatar.stop();
    //} else {
      //faceAvatar.goSpeech(0.6, 200);
    //}
  }

  if (M5.BtnB.wasPressed()) {
    servoXY.moveBlocking(0, -45, 1500);
    //faceAvatar.setExpression(i);
    i++;
    if (i > 5) i = 0;
  }

  if (M5.BtnC.wasPressed()) {
    servoXY.moveBlocking(-90, 0, 1500);
  }

  delay(5);
  static uint32_t t = 0;
  if (millis() - t > 1000) {
    t = millis();
    Serial.printf("[HB] loop alive hw=%u\n", (unsigned)uxTaskGetStackHighWaterMark(nullptr));
  }
}
