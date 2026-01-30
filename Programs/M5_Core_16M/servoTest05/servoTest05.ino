#include <Arduino.h>
#include <M5Unified.h>
#include <SD.h>

#include "SpiBusLock.hpp"
#include "ServoXY.hpp"

ServoXY servo;  // デフォルト設定（X=22,ch0 / Y=21,ch1）

void setup() {
  Serial.begin(115200);
  M5.begin();
  ensure_spi_mutex();
  delay(1000);

  // SD初期化（M5Stack Coreの一般的なCS=4）
  // ※環境により周波数は要調整
  const bool sdOk = SD.begin(4, SPI, 25000000);
  if (!sdOk) {
    Serial.println("SD begin error");
    //for (;;) {}
  }

  // JSONが読めたら設定を反映（centerで開始）。失敗なら(0,0)開始。
  if (!servo.begin()) {
    Serial.println("servo begin error");
    //servo.begin(0, 0);
    //Serial.println("x:0, y:0");
  }
  servo.begin(0, 0);
  Serial.println("x:0, y:0");

  // 軽い移動は優先して完了させたい（ただし他タスクはdelayで動く）
  //servo.moveBlocking(90, -90, 1000);

  // 長い移動は非ブロッキングで流す
  //servo.moveBegin(-90, 90, 10000);
}

void loop() {
  M5.update();

  if (M5.BtnA.wasPressed()) {
    Serial.println("x:0, y:0");
    servo.moveBlocking(0, 0, 1000);
  }

  if (M5.BtnB.wasPressed()) {
    Serial.println("x:0, y:90");
    servo.moveBlocking(0, 90, 1000);
  }

  if (M5.BtnC.wasPressed()) {
    Serial.println("x:0, y:-90");
    servo.moveBlocking(0, -90, 1000);
  }
  //servo.update();
  delay(5);
}
