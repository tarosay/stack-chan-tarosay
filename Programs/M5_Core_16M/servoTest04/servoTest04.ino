#include <Arduino.h>
#include <M5Unified.h>
#include <SD.h>

#include "SpiBusLock.hpp"
#include "ServoXY.hpp"

ServoXY servo;  // デフォルト設定（X=22,ch0 / Y=21,ch1）

void setup() {
  M5.begin();
  ensure_spi_mutex();

  // SD初期化（M5Stack Coreの一般的なCS=4）
  // ※環境により周波数は要調整
  const bool sdOk = SD.begin(4, SPI, 25000000);

  // JSONが読めたら設定を反映（centerで開始）。失敗なら(0,0)開始。
  if (!(sdOk && servo.begin())) {
    servo.begin(0, 0);
  }

  // 軽い移動は優先して完了させたい（ただし他タスクはdelayで動く）
  servo.moveBlocking(90, -90, 1000);

  // 長い移動は非ブロッキングで流す
  servo.moveBegin(-90, 90, 10000);
}

void loop() {
  // 非ブロッキング更新
  servo.update();

  // 他処理（MQTT等）をここに
}
