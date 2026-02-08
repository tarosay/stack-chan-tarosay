#include <M5Unified.h>

#include "Face.hpp"


void setup() {
  Serial.begin(115200);
  delay(1500);
  M5.begin();
  M5.Lcd.setBrightness(30);
  M5.Lcd.clear();

  face.begin();
}

uint8_t i = 0;
void loop() {
  M5.update();
  face.update();

  if (M5.BtnA.wasPressed()) {
    if (face.Speaking) {
      face.stop();
    } else {
      face.goSpeech(0.6, 200);
    }
  }
  if (M5.BtnB.wasPressed()) {
    face.setExpression(i);
    i++;
    if (i > 5) i = 0;
  }
  if (M5.BtnC.wasPressed()) {
  }
}
