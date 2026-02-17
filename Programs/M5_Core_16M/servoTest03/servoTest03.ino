#include <Arduino.h>
#include <M5Unified.h>
#include "ServoXY.hpp"

ServoXY servo;  // デフォルト設定（X=22,ch0 / Y=21,ch1）

void setup() {
  Serial.begin(115200);
  //M5.begin();
  delay(2000);

  servo.begin(0, 0);

  M5.Display.setTextSize(2);              // 文字サイズ
  M5.Display.setTextColor(WHITE, BLACK);  // (文字色, 背景色)
  M5.Display.setCursor(0, 0);             // 描画位置 (x,y)

  M5.Display.println("x = 90, y = -90");
  // 軽い移動は優先して完了させたい（ただし他タスクはdelayで動く）
  servo.moveBlocking(90, -90, 3000);

  delay(1000);

  M5.Display.println("x = 90, y = -90");
  // 長い移動は非ブロッキングで流す
  servo.moveBegin(-90, 90, 3000);
}

void loop() {
  // 非ブロッキング更新
  servo.update();

  // 他処理（MQTT等）をここに
}
