#include <Arduino.h>
#include <WiFi.h>

#include "WavPlayer.hpp"
#include "Speech.hpp"
#include "WebAPI.hpp"

#include <SD.h>
#include <Update.h>
#include <Ticker.h>
#include <M5StackUpdater.h>
#include <M5Unified.h>  //M5Unified と M5GFXのアップデートはしてはだめ M5GFX 0.2.3 M5Unifild 0.2.2
#include <Stackchan_system_config.h>
#include <Stackchan_servo.h>

#ifdef ARDUINO_M5STACK_CORES3
#include <gob_unifiedButton.hpp>
goblib::UnifiedButton unifiedButton;
#endif

#include <Avatar.h>  // https://github.com/meganetaaan/m5stack-avatar
#include <faces/FaceTemplates.hpp>

using namespace m5avatar;
Avatar avatar;

Speech speech(wavPlayer, avatar);
WebAPI webAPI;

#define START_DEGREE_VALUE_X 90
#define START_DEGREE_VALUE_Y 90

#define SDU_APP_PATH "/stackchan_tester.bin"
#define TFCARD_CS_PIN 4

StackchanSERVO servo;
StackchanSystemConfig system_config;

bool core_port_a = false;  // Core1のPortAを使っているかどうか

void setup() {
  //Serial.begin(115200);  // シリアル出力初期設定

  auto cfg = M5.config();  // 設定用の情報を抽出
  //cfg.output_power = true;    // Groveポートの5V出力をする／しない（TakaoBase用）
  M5.begin(cfg);  // M5Stackをcfgの設定で初期化
  wavPlayer.begin();

#ifdef ARDUINO_M5STACK_CORES3
  unifiedButton.begin(&M5.Display, goblib::UnifiedButton::appearance_t::transparent_all);
#endif
  M5.Log.setLogLevel(m5::log_target_display, ESP_LOG_NONE);
  M5.Log.setLogLevel(m5::log_target_serial, ESP_LOG_INFO);
  M5.Log.setEnableColor(m5::log_target_serial, false);
  M5_LOGI("Hello World");
  SD.begin(GPIO_NUM_4, SPI, 25000000);
  delay(2000);

  system_config.loadConfig(SD, "");
  if (M5.getBoard() == m5::board_t::board_M5Stack) {
    if (system_config.getServoInfo(AXIS_X)->pin == 22) {
      // M5Stack Coreの場合、Port.Aを使う場合は内部I2CをOffにする必要がある。バッテリー表示は不可。
      avatar.setBatteryIcon(false);
      M5.In_I2C.release();
      core_port_a = true;
    }
  } else {
    avatar.setBatteryIcon(true);
  }
  // servo
#ifdef ARDUINO_M5STACK_CORES3
  system_config.getServoInfo(AXIS_X)->pin = 1;  // AXIS_Xのピンを2に設定
  system_config.getServoInfo(AXIS_Y)->pin = 2;  // AXIS_Yのピンを1に設定
#elif defined(ARDUINO_M5STACK_CORE)
  system_config.getServoInfo(AXIS_X)->pin = 5;   // AXIS_Xのピンを2に設定
  system_config.getServoInfo(AXIS_Y)->pin = 21;  // AXIS_Yのピンを1に設定
#endif
  servo.begin(system_config.getServoInfo(AXIS_X)->pin, system_config.getServoInfo(AXIS_X)->start_degree,
              system_config.getServoInfo(AXIS_X)->offset,
              system_config.getServoInfo(AXIS_Y)->pin, system_config.getServoInfo(AXIS_Y)->start_degree,
              system_config.getServoInfo(AXIS_Y)->offset,
              (ServoType)system_config.getServoType());

  M5.Power.setExtOutput(!system_config.getUseTakaoBase());  // 設定ファイルのTakaoBaseがtrueの場合は、Groveポートの5V出力をONにする。

  M5_LOGI("ServoType: %d", system_config.getServoType());
  M5_LOGI("AXIS_X: %d", system_config.getServoInfo(AXIS_X)->pin);
  M5_LOGI("AXIS_Y: %d", system_config.getServoInfo(AXIS_Y)->pin);

  avatar.setFace(avatar.getFace());
  avatar.init(8);  // start drawing

  speech.playWav("/wav/wificonnect.wav", 0.5);
  // WebAPIの初期化
  wifi_s *wifi_info = system_config.getWiFiSetting();
  //M5_LOGI("SSID: %s", wifi_info->ssid.c_str());
  //M5_LOGI("Password: %s", wifi_info->password.c_str());
  webAPI.begin(wifi_info->ssid.c_str(), wifi_info->password.c_str());

  if (webAPI.getIPAddress() == "0.0.0.0") {
    speech.playWav("/wav/wififaild.wav", 0.5);
  } else {
    speech.playWav("/wav/wifiok.wav", 0.5);
    delay(500);
    speech.playIP(webAPI.getIPAddress(), 0.5);
    delay(1000);
    speech.playIP(webAPI.getIPAddress(), 0.5);
  }
}

void loop() {
  webAPI.handleClient();

  if (webAPI.getFileUploaded() == 1) {
    FaceUp();
    speech.playWav("/upload.wav", 0.5);
    webAPI.resetFileUploadedType();
    servo.moveXY(system_config.getServoInfo(AXIS_X)->start_degree, system_config.getServoInfo(AXIS_Y)->start_degree, 1000);
  }

  if (webAPI.isReStart()) {
    FaceUp();
    speech.playWav("/wav/restart.wav", 0.5);
    servo.moveXY(system_config.getServoInfo(AXIS_X)->start_degree, system_config.getServoInfo(AXIS_Y)->start_degree, 1000);
    delay(1000);
    esp_restart();  // M5Stackを再起動
  }

  if (webAPI.isWavNG()) {
    FaceUp();
    speech.playWav("/wav/wavng.wav", 0.5);
    webAPI.resetWavNG();
    servo.moveXY(system_config.getServoInfo(AXIS_X)->start_degree, system_config.getServoInfo(AXIS_Y)->start_degree, 1000);
  }

  int ongen = webAPI.getOngen();
  if (ongen > 0) {
    FaceUp();
    if (ongen == 1) {
      speech.playWav("/wav/motoko1.wav", 0.5);
    } else if (ongen == 2) {
      speech.playWav("/wav/motoko2.wav", 0.5);
    } else if (ongen == 3) {
      speech.playWav("/wav/jorin1.wav", 0.5);
    } else if (ongen == 4) {
      speech.playWav("/wav/jorin2.wav", 0.5);
    }
    webAPI.resetOngen();
    servo.moveXY(system_config.getServoInfo(AXIS_X)->start_degree, system_config.getServoInfo(AXIS_Y)->start_degree, 1000);
  }

#ifdef ARDUINO_M5STACK_CORES3
  unifiedButton.update();  // M5.update() よりも前に呼ぶ事
#endif
  M5.update();

  if (M5.BtnA.wasPressed()) {
    speech.playWav("/upload.wav", 0.5);
  }

  if (M5.BtnB.wasSingleClicked()) {
    speech.playIP(webAPI.getIPAddress(), 0.5);
  }

  if (M5.BtnC.wasPressed()) {
  }

  // delayを50msec程度入れないとCoreS3でバッテリーレベルと充電状態がおかしくなる。
  delay(25);
}

void FaceUp() {
  //ランダムに左を向く
  int x = random(system_config.getServoInfo(AXIS_X)->lower_limit + 100, system_config.getServoInfo(AXIS_X)->upper_limit - 45);  // 可動範囲の下限+45〜上限-45 でランダム
  //ランダムに上を向く
  int y = random(system_config.getServoInfo(AXIS_Y)->lower_limit + 35, system_config.getServoInfo(AXIS_Y)->upper_limit);  // 可動範囲の下限〜上限 でランダム
  int delay_time = random(10);
  servo.moveXY(x, y, 800 + 100 * delay_time);
  //delay(3000);
  //M5_LOGI("x: %d", x);
}