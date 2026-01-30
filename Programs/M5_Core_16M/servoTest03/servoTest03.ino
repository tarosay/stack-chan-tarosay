#include <Arduino.h>
#include "ServoXY.hpp"

ServoXY servo;  // デフォルト設定（X=22,ch0 / Y=21,ch1）

void setup() {
  servo.begin(0, 0);

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
