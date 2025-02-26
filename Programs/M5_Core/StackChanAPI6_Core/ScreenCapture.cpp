#include <M5Unified.h>
#include <SD.h>

#include "ScreenCapture.hpp"

ScreenCapture ScCaptur;

// **画面をSDカードに保存**
bool ScreenCapture::save() {
  File file = SD.open(FILENAME, FILE_WRITE);
  if (!file) {
    Serial.println("[ERROR] SD Card not write open.");
    return false;
  }

  uint16_t lineBuffer[SCREEN_WIDTH];

  for (int y = 0; y < SCREEN_HEIGHT; y++) {
    M5.Lcd.readRect(0, y, SCREEN_WIDTH, 1, lineBuffer);
    file.write((uint8_t*)lineBuffer, SCREEN_WIDTH * sizeof(uint16_t));
  }

  file.flush();
  file.close();
  return true;
}

// **SDカードから画面データを復元**
bool ScreenCapture::load() {
  File file = SD.open(FILENAME, FILE_READ);
  if (!file) {
    Serial.println("[ERROR] SDCaed note read open.");
    return false;
  }

  uint16_t lineBuffer[SCREEN_WIDTH];

  for (int y = 0; y < SCREEN_HEIGHT; y++) {
    int bytesRead = file.read((uint8_t*)lineBuffer, SCREEN_WIDTH * sizeof(uint16_t));
    if (bytesRead != SCREEN_WIDTH * sizeof(uint16_t)) {
      Serial.printf("[ERROR] SD Card read error: row %d, byteRead: %d\n", y, bytesRead);
      file.close();
      return false;
    }
    M5.Lcd.pushImage(0, y, SCREEN_WIDTH, 1, lineBuffer);
  }

  file.close();
  return true;
}
