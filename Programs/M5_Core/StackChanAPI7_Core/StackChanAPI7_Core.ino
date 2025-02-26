#include <Arduino.h>
#include <WiFi.h>
#include <SD.h>
#include <Update.h>
#include <Ticker.h>
#include <M5StackUpdater.h>
#include <M5Unified.h>
#include <Stackchan_system_config.h>
#include <Stackchan_servo.h>

#ifdef ARDUINO_M5STACK_CORES3
#include <gob_unifiedButton.hpp>
goblib::UnifiedButton unifiedButton;
#endif

#include <Avatar.h>  // https://github.com/meganetaaan/m5stack-avatar
#include <faces/FaceTemplates.hpp>

#include "WavPlayer.hpp"
#include "Speech.hpp"
#include "WebAPI.hpp"
#include "Mp3ToWav.hpp"

using namespace m5avatar;
Avatar* avatar = new Avatar();                   // ポインタに変更
Speech* speech = new Speech(wavPlayer, avatar);  // avatar を確保した後で Speech を作成

WebAPI webAPI;
Mp3ToWav mp3ToWav;  // mp3をwavに変換

#define START_DEGREE_VALUE_X 90
#define START_DEGREE_VALUE_Y 90

#define SDU_APP_PATH "/stackchan_tester.bin"
#define TFCARD_CS_PIN 4

StackchanSERVO servo;
StackchanSystemConfig system_config;

#ifdef ARDUINO_M5STACK_CORE
ColorPalette* color_palette;
#endif

bool core_port_a = false;  // Core1のPortAを使っているかどうか

void setup() {
  Serial.begin(115200);  // シリアル出力初期設定

  auto cfg = M5.config();  // 設定用の情報を抽出
  //cfg.output_power = true;    // Groveポートの5V出力をする／しない（TakaoBase用）
  M5.begin(cfg);  // M5Stackをcfgの設定で初期化

#ifdef ARDUINO_M5STACK_CORES3
  unifiedButton.begin(&M5.Display, goblib::UnifiedButton::appearance_t::transparent_all);
#endif
  M5.Log.setLogLevel(m5::log_target_display, ESP_LOG_NONE);
  M5.Log.setLogLevel(m5::log_target_serial, ESP_LOG_INFO);
  M5.Log.setEnableColor(m5::log_target_serial, false);

  // **M5Unified を使用する場合、SD.begin() のみでOK**
  if (!SD.begin(GPIO_NUM_4, SPI, 25000000)) {
    M5_LOGI("[ERROR] SDカードの初期化に失敗しました");
    while (true)
      ;
  }

  delay(2000);

  system_config.loadConfig(SD, "");
  if (M5.getBoard() == m5::board_t::board_M5Stack) {
    if (system_config.getServoInfo(AXIS_X)->pin == 22) {
      // M5Stack Coreの場合、Port.Aを使う場合は内部I2CをOffにする必要がある。バッテリー表示は不可。
      avatar->setBatteryIcon(false);
      M5.In_I2C.release();
      core_port_a = true;
    }
  } else {
    avatar->setBatteryIcon(true);
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

#ifdef ARDUINO_M5STACK_CORE
  avatar->setFace(new OmegaFace());
  color_palette = new ColorPalette();
  color_palette->set(COLOR_PRIMARY, TFT_BLACK);
  color_palette->set(COLOR_SECONDARY, TFT_BLUE);
  color_palette->set(COLOR_BACKGROUND, TFT_WHITE);
  avatar->setColorPalette(*color_palette);
#else
  avatar->setFace(avatar->getFace());
#endif

  avatar->init(8);  // start drawing

  wavPlayer.begin();

  delay(1000);
  speech->playSound("/wav/wificonnect.wav", 0.5);

  // WebAPIの初期化
  wifi_s* wifi_info = system_config.getWiFiSetting();

  webAPI.begin(wifi_info->ssid.c_str(), wifi_info->password.c_str());

  if (webAPI.getIPAddress() == "0.0.0.0") {
    speech->playSound("/wav/wififaild.wav", 0.5);
  } else {
    speech->playSound("/wav/wifiok.wav", 0.5);
    delay(500);
    speech->playIP(webAPI.getIPAddress(), 0.5);
    delay(1000);
    speech->playIP(webAPI.getIPAddress(), 0.5);
  }

  while (wavPlayer.isPlaying()) {
    delay(1);
  }
}

void loop() {
  webAPI.handleClient();

  if (webAPI.getFileUploaded() == 1) {
    // 拡張子チェック
    String filename = webAPI.getUploadFilename();
    filename.toLowerCase();
    if (filename.endsWith(".mp3")) {
      // アバターを一時停止 アバターは出たまま
      avatar->stop();

      if (!mp3ToWav.ConvertWav("/upload.mp3", "/upload.wav")) {
        FaceUp();
        speech->playSound("/wav/wavfilefail.wav", webAPI.getVolume());
        delay(1000);
        speech->playSound("/wav/saikido.wav", webAPI.getVolume());
        servo.moveXY(system_config.getServoInfo(AXIS_X)->start_degree, system_config.getServoInfo(AXIS_Y)->start_degree, 1000);
        delay(1000);
        esp_restart();  // M5Stackを再起動    speech->playSound("/upload.wav", webAPI.getVolume());
      }

      // アバターを再開
      avatar->start(8);
    }
    FaceUp();
    speech->playSound("/upload.wav", webAPI.getVolume());
    webAPI.resetFileUploadedType();
    servo.moveXY(system_config.getServoInfo(AXIS_X)->start_degree, system_config.getServoInfo(AXIS_Y)->start_degree, 1000);
  }

  if (webAPI.getFileUploaded() == 100) {
    FaceUp();
    speech->playSound("/upload.wav", webAPI.getVolume());
    webAPI.resetFileUploadedType();
    servo.moveXY(system_config.getServoInfo(AXIS_X)->start_degree, system_config.getServoInfo(AXIS_Y)->start_degree, 1000);
  }

  if (webAPI.isReStart()) {
    FaceUp();
    speech->playSound("/wav/restart.wav", webAPI.getVolume());
    servo.moveXY(system_config.getServoInfo(AXIS_X)->start_degree, system_config.getServoInfo(AXIS_Y)->start_degree, 1000);
    delay(1000);
    esp_restart();  // M5Stackを再起動
  }

  if (webAPI.isWavNG()) {
    FaceUp();
    speech->playSound("/wav/wavng.wav", webAPI.getVolume());
    webAPI.resetWavNG();
    servo.moveXY(system_config.getServoInfo(AXIS_X)->start_degree, system_config.getServoInfo(AXIS_Y)->start_degree, 1000);
  }

  int ongen = webAPI.getOngen();
  if (ongen > 0) {
    FaceUp();

    if (ongen == 1) {
      speech->playSound("/wav/motoko1.wav", webAPI.getVolume());
    } else if (ongen == 2) {
      speech->playSound("/wav/motoko2.wav", webAPI.getVolume());
    } else if (ongen == 3) {
      speech->playSound("/wav/jorin1.wav", webAPI.getVolume());
    } else if (ongen == 4) {
      speech->playSound("/wav/jorin2.wav", webAPI.getVolume());
    }

    webAPI.resetOngen();
    servo.moveXY(system_config.getServoInfo(AXIS_X)->start_degree, system_config.getServoInfo(AXIS_Y)->start_degree, 1000);
  }

  if (webAPI.getIsMove()) {
    WebAPI::MoveXY move = webAPI.getMoveXY();
    int x = system_config.getServoInfo(AXIS_X)->start_degree + move.x;
    int y = system_config.getServoInfo(AXIS_Y)->start_degree + move.y;
    FaceUp(x, y);
  }

  if (webAPI.getIsSpeech()) {
    WebAPI::MoveXY move = webAPI.getMoveXY();
    int x = system_config.getServoInfo(AXIS_X)->start_degree + move.x;
    int y = system_config.getServoInfo(AXIS_Y)->start_degree + move.y;
    String wavFilenameStr = String(webAPI.getWavFilename());

    FaceUp(x, y);
    speech->playSound(wavFilenameStr, webAPI.getVolume());
    servo.moveXY(system_config.getServoInfo(AXIS_X)->start_degree, system_config.getServoInfo(AXIS_Y)->start_degree, 1000);
  }

#ifdef ARDUINO_M5STACK_CORES3
  unifiedButton.update();  // M5.update() よりも前に呼ぶ事
#endif
  M5.update();

  if (M5.BtnA.wasPressed()) {
    speech->playSound("/upload.wav", webAPI.getVolume());
  }

  if (M5.BtnB.wasSingleClicked()) {
    // アバターを一時停止
    avatar->suspend();
    dispIP();
    // 画面をクリアしてアバターを再開
    M5.Lcd.fillScreen(TFT_BLACK);
    avatar->resume();
    speech->playIP(webAPI.getIPAddress(), webAPI.getVolume());
  }

  if (M5.BtnC.wasPressed()) {
    listDir(SD, "/", 0);
  }

  // delayを50msec程度入れないとCoreS3でバッテリーレベルと充電状態がおかしくなる。
  delay(25);
}

void FaceUp() {
  //ランダムに左を向く
  int x = random(system_config.getServoInfo(AXIS_X)->start_degree + 10, system_config.getServoInfo(AXIS_X)->start_degree + 45);  // 可動範囲の真ん中+10〜上限-45 でランダム
  //ランダムに上を向く
  int y = random(system_config.getServoInfo(AXIS_Y)->start_degree - 25, system_config.getServoInfo(AXIS_Y)->start_degree);  // 可動範囲の下限+35〜真ん中 でランダム
  //M5_LOGI("x: %d, y: %d", x, y);
  FaceUp(x, y);
}
void FaceUp(int x, int y) {
  //return;
  x = x < system_config.getServoInfo(AXIS_X)->lower_limit ? system_config.getServoInfo(AXIS_X)->lower_limit : x;
  x = x > system_config.getServoInfo(AXIS_X)->upper_limit ? system_config.getServoInfo(AXIS_X)->upper_limit : x;
  y = y < system_config.getServoInfo(AXIS_Y)->lower_limit ? system_config.getServoInfo(AXIS_Y)->lower_limit : y;
  y = y > system_config.getServoInfo(AXIS_Y)->upper_limit ? system_config.getServoInfo(AXIS_Y)->upper_limit : y;

  int delay_time = random(10);
  servo.moveXY(x, y, 800 + 100 * delay_time);
}

void dispIP() {
  // 画面をクリアしてIPアドレスを表示
  M5.Lcd.fillScreen(TFT_BLACK);
  M5.Lcd.setTextSize(3);                      // 文字サイズを設定
  M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);  // 白文字、黒背景

  // IPアドレスを分割
  String ipAddress = webAPI.getIPAddress();
  int pos = 0, prevPos = 0;
  int line = 50;  // 表示開始位置
  for (int i = 0; i < 4; i++) {
    pos = ipAddress.indexOf('.', prevPos);    // '.' の位置を検索
    if (pos == -1) pos = ipAddress.length();  // 最後の要素

    String part = ipAddress.substring(prevPos, pos) + ".";  // 分割
    if (i == 3) part = ipAddress.substring(prevPos, pos);   // 最後は '.' を付けない

    M5.Lcd.setCursor(50, line);
    M5.Lcd.print(part);

    prevPos = pos + 1;
    line += 40;  // 行の間隔を調整
  }
  // 5秒間表示
  delay(3000);
}

void listDir(fs::FS& fs, const char* dirname, uint8_t levels) {
  M5_LOGI("Listing directory: %s", dirname);

  File root = fs.open(dirname);
  if (!root) {
    M5_LOGI("Failed to open directory");
    return;
  }
  if (!root.isDirectory()) {
    M5_LOGI("Not a directory");
    return;
  }

  File file = root.openNextFile();
  while (file) {
    if (file.isDirectory()) {
      M5_LOGI("DIR : %s", file.name());
      if (levels) {
        listDir(fs, file.name(), levels - 1);
      }
    } else {
      M5_LOGI("FILE: %s  SIZE: %d bytes", file.name(), file.size());
    }
    file = root.openNextFile();
  }
}