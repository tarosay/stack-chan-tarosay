#include <Arduino.h>
#include <M5Unified.h>
#include <SD.h>

static const char* WAV_PATH = "/upload.wav";
#ifndef SDCARD_CSPIN
#define SDCARD_CSPIN 4
#endif

void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);

  Serial.begin(115200);
  delay(200);

  M5.Display.clear();
  M5.Display.setTextSize(2);
  M5.Display.println("WAV playWav(RAM)");

  if (!SD.begin(SDCARD_CSPIN, SPI, 25000000)) {
    Serial.println("SD init failed");
    for (;;) delay(1000);
  }

  File f = SD.open(WAV_PATH, FILE_READ);
  if (!f) {
    Serial.println("open failed");
    for (;;) delay(1000);
  }

  size_t len = f.size();
  Serial.printf("file len=%u\n", (unsigned)len);

  uint8_t* wav = (uint8_t*)malloc(len);
  if (!wav) {
    Serial.println("malloc failed");
    for (;;) delay(1000);
  }

  size_t r = f.read(wav, len);
  f.close();
  Serial.printf("read=%u\n", (unsigned)r);

  M5.Speaker.begin();
  M5.Speaker.setVolume(160);

  // WAVヘッダ込みのメモリをそのまま再生（これがM5Unifiedの想定）
  bool ok = M5.Speaker.playWav(wav, len);
  Serial.printf("playWav=%d\n", (int)ok);

  // 再生終了を待つ（終わったら解放）
  while (M5.Speaker.isPlaying()) {
    M5.update();
    delay(1);
  }

  free(wav);
  Serial.println("done");
}

void loop() {}
