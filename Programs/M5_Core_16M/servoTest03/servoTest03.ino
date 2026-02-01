#include <Arduino.h>
#include <M5Unified.h>
#include "ServoXY.hpp"

ServoXY servo;  // デフォルト設定（X=22,ch0 / Y=21,ch1）

void setup() {
  Serial.begin(115200);
  //M5.begin();
  delay(2000);

  servo.begin(0, 0);

  // 軽い移動は優先して完了させたい（ただし他タスクはdelayで動く）
  // servo.moveBlocking(90, -90, 1000);

  // 長い移動は非ブロッキングで流す
  // servo.moveBegin(-90, 90, 4000);
}

void loop() {
  // 非ブロッキング更新
  servo.update();

  // 他処理（MQTT等）をここに
}
