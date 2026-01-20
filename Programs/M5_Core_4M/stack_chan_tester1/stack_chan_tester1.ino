#pragma mark - Depend ESP8266Audio and ESP8266_Spiram libraries

#include <Arduino.h>

#include <WiFi.h>
#include "Wavs.hpp"
#include "AudioFileSourcePROGMEM.h"
#include "AudioFileSourceSD.h"
#include "AudioGeneratorWAV.h"
#include "AudioOutputI2S.h"

AudioGeneratorWAV *wav;
AudioFileSourcePROGMEM *file = nullptr;  // 明示的にnullptrで初期化
AudioOutputI2S *out;

float volume = 1.0;  // 初期音量 (最大1.0)

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

int servo_offset_x = 0;  // X軸サーボのオフセット（サーボの初期位置からの+-で設定）
int servo_offset_y = 0;  // Y軸サーボのオフセット（サーボの初期位置からの+-で設定）

#include <Avatar.h>          // https://github.com/meganetaaan/m5stack-avatar
#include "formatString.hpp"  // https://gist.github.com/GOB52/e158b689273569357b04736b78f050d6

using namespace m5avatar;
Avatar avatar;

#define START_DEGREE_VALUE_X 90
#define START_DEGREE_VALUE_Y 90

#define SDU_APP_PATH "/stackchan_tester.bin"
#define TFCARD_CS_PIN 4

StackchanSERVO servo;
StackchanSystemConfig system_config;

uint32_t mouth_wait = 2000;      // 通常時のセリフ入れ替え時間（msec）
uint32_t last_mouth_millis = 0;  // セリフを入れ替えた時間
bool core_port_a = false;        // Core1のPortAを使っているかどうか

const char *lyrics[] = { "BtnA:MoveTo90  ", "BtnB:ServoTest  ", "BtnC:RandomMode  ", "BtnALong:AdjustMode" };
const int lyrics_size = sizeof(lyrics) / sizeof(char *);
int lyrics_idx = 0;

void adjustOffset() {
  // サーボのオフセットを調整するモード
  servo_offset_x = 0;
  servo_offset_y = 0;
  servo.moveXY(system_config.getServoInfo(AXIS_X)->start_degree, system_config.getServoInfo(AXIS_Y)->start_degree, 2000);
  bool adjustX = true;
  for (;;) {
#ifdef ARDUINO_M5STACK_CORES3
    unifiedButton.update();  // M5.update() よりも前に呼ぶ事
#endif
    M5.update();
    if (M5.BtnA.wasPressed()) {
      // オフセットを減らす
      if (adjustX) {
        servo_offset_x--;
      } else {
        servo_offset_y--;
      }
    }
    if (M5.BtnB.pressedFor(2000)) {
      // 調整モードを終了
      break;
    }
    if (M5.BtnB.wasPressed()) {
      // 調整モードのXとYを切り替え
      adjustX = !adjustX;
    }
    if (M5.BtnC.wasPressed()) {
      // オフセットを増やす
      if (adjustX) {
        servo_offset_x++;
      } else {
        servo_offset_y++;
      }
    }
    servo.moveXY(system_config.getServoInfo(AXIS_X)->start_degree, system_config.getServoInfo(AXIS_Y)->start_degree, 2000);

    std::string s;

    if (adjustX) {
      s = formatString("%s:%d:BtnB:X/Y", "X", servo_offset_x);
    } else {
      s = formatString("%s:%d:BtnB:X/Y", "Y", servo_offset_y);
    }
    avatar.setSpeechText(s.c_str());
  }
}

void moveRandom() {
  for (;;) {
    // ランダムモード
    int x = random(system_config.getServoInfo(AXIS_X)->lower_limit + 45, system_config.getServoInfo(AXIS_X)->upper_limit - 45);  // 可動範囲の下限+45〜上限-45 でランダム
    int y = random(system_config.getServoInfo(AXIS_Y)->lower_limit, system_config.getServoInfo(AXIS_Y)->upper_limit);            // 可動範囲の下限〜上限 でランダム
#ifdef ARDUINO_M5STACK_CORES3
    unifiedButton.update();  // M5.update() よりも前に呼ぶ事
#endif
    M5.update();
    if (M5.BtnC.wasPressed()) {
      break;
    }
    int delay_time = random(10);
    servo.moveXY(x, y, 1000 + 100 * delay_time);
    delay(2000 + 500 * delay_time);
    if (!core_port_a) {
      // Basic/M5Stack Fireの場合はバッテリー情報が取得できないので表示しない
      avatar.setBatteryStatus(M5.Power.isCharging(), M5.Power.getBatteryLevel());
    }
    //avatar.setSpeechText("Stop BtnC");
    avatar.setSpeechText("");
  }
}
void testServo() {
  for (int i = 0; i < 2; i++) {
    avatar.setSpeechText("X center -> left  ");
    servo.moveX(system_config.getServoInfo(AXIS_X)->lower_limit, 1000);
    avatar.setSpeechText("X left -> right  ");
    servo.moveX(system_config.getServoInfo(AXIS_X)->upper_limit, 3000);
    avatar.setSpeechText("X right -> center  ");
    servo.moveX(system_config.getServoInfo(AXIS_X)->start_degree, 1000);
    avatar.setSpeechText("Y center -> lower  ");
    servo.moveY(system_config.getServoInfo(AXIS_Y)->lower_limit, 1000);
    avatar.setSpeechText("Y lower -> upper  ");
    servo.moveY(system_config.getServoInfo(AXIS_Y)->upper_limit, 1000);
    avatar.setSpeechText("Initial Pos.");
    servo.moveXY(system_config.getServoInfo(AXIS_X)->start_degree, system_config.getServoInfo(AXIS_Y)->start_degree, 1000);
  }
}

void mumumuServo() {
  for (int i = 0; i < 30; i++) {
    servo.moveX(120, 250);
    servo.moveX(240, 250);
  }
}

void setup() {
  //Serial.begin(115200);  // シリアル出力初期設定

  auto cfg = M5.config();  // 設定用の情報を抽出
  //cfg.output_power = true;    // Groveポートの5V出力をする／しない（TakaoBase用）
  M5.begin(cfg);  // M5Stackをcfgの設定で初期化

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

  avatar.init();

  calculateWavDuration(wavs, sizeof(wavs));
  //file = new AudioFileSourcePROGMEM(wavs, sizeof(wavs));

  out = new AudioOutputI2S(0, 1);  // Output to builtInDAC
  out->SetOutputModeMono(true);
  out->SetGain(volume);  // 初期音量を設定
  wav = new AudioGeneratorWAV();
  wav->begin(file, out);

  // while (wav->isRunning()) {
  //   if (!wav->loop()) {
  //     wav->stop();
  //     Serial.println("WAV playback finished");
  //   }
  // }


  last_mouth_millis = millis();
  //moveRandom();
  //testServo();
}

void loop() {
#ifdef ARDUINO_M5STACK_CORES3
  unifiedButton.update();  // M5.update() よりも前に呼ぶ事
#endif
  M5.update();
  if (M5.BtnA.pressedFor(2000)) {
    // サーボのオフセットを調整するモードへ
    adjustOffset();
  } else if (M5.BtnA.wasPressed()) {
    // 初期位置へ戻ります。
    servo.moveXY(system_config.getServoInfo(AXIS_X)->start_degree, system_config.getServoInfo(AXIS_Y)->start_degree, 2000);
  }

  if (M5.BtnB.wasSingleClicked()) {
    testServo();
  } else if (M5.BtnB.wasDoubleClicked()) {
    if (M5.Power.getExtOutput() == true) {
      M5.Power.setExtOutput(false);
      avatar.setSpeechText("ExtOutput Off");
    } else {
      M5.Power.setExtOutput(true);
      avatar.setSpeechText("ExtOutput On");
    }
    delay(2000);
    avatar.setSpeechText("");
  }
  if (M5.BtnC.pressedFor(5000)) {
    M5_LOGI("Will copy this sketch to filesystem");
    if (saveSketchToFS(SD, SDU_APP_PATH, TFCARD_CS_PIN)) {
      M5_LOGI("Copy Successful!");
    } else {
      M5_LOGI("Copy failed!");
    }
  } else if (M5.BtnC.wasPressed()) {

    if (file) {
      delete file;
      file = nullptr;
    }

    file = new AudioFileSourcePROGMEM(wavs, sizeof(wavs));
    if (!wav->begin(file, out)) {
      Serial.printf("Failed to restart playback\n");
    }
    wav->begin(file, out);
    while (wav->isRunning()) {
      if (!wav->loop()) {
        wav->stop();
        Serial.println("WAV playback finished");
      }
    }

    // ランダムモードへ
    //mumumuServo(); // 左右に高速で首を振ります。（サーボが壊れるのであまり使わないでください。）
    moveRandom();  // ランダムモードになります。
  }

  if ((millis() - last_mouth_millis) > mouth_wait) {
    const char *l = lyrics[lyrics_idx++ % lyrics_size];
    avatar.setSpeechText(l);
    avatar.setMouthOpenRatio(0.7);
    delay(200);
    avatar.setMouthOpenRatio(0.0);
    last_mouth_millis = millis();
    if (!core_port_a) {
      avatar.setBatteryStatus(M5.Power.isCharging(), M5.Power.getBatteryLevel());
    }
  }
  // delayを50msec程度入れないとCoreS3でバッテリーレベルと充電状態がおかしくなる。
  delay(50);
}


void calculateWavDuration(const uint8_t *wavFile, int fileSize) {
  // ヘッダーの解析
  uint32_t sampleRate = *(uint32_t *)(wavFile + 24);  // サンプリング周波数
  uint16_t channels = *(uint16_t *)(wavFile + 22);    // チャンネル数
  uint16_t bitDepth = *(uint16_t *)(wavFile + 34);    // ビット深度

  // "data"チャンクを検索
  int dataChunkOffset = -1;
  uint32_t dataSize = 0;
  for (int i = 0; i < fileSize - 8; i++) {
    if (wavFile[i] == 'd' && wavFile[i + 1] == 'a' && wavFile[i + 2] == 't' && wavFile[i + 3] == 'a') {
      dataChunkOffset = i + 8;  // "data"の次の4バイトにデータサイズがある
      dataSize = *(uint32_t *)(wavFile + i + 4);
      break;
    }
  }

  // "data"チャンクが見つからない場合
  if (dataChunkOffset == -1) {
    M5_LOGI("Error: 'data' chunk not found!");
    return;
  }

  // 再生時間を計算
  float duration = (float)dataSize / (sampleRate * channels * (bitDepth / 8));

  // 結果を出力
  M5_LOGI("Sample Rate: %d Hz", sampleRate);
  M5_LOGI("Channels: %d", channels);
  M5_LOGI("Bit Depth: %d bits", bitDepth);
  M5_LOGI("Data Offset: %d bytes", dataChunkOffset);
  M5_LOGI("Data Size: %d bytes", dataSize);
  M5_LOGI("Duration: %.2f seconds", duration);
}