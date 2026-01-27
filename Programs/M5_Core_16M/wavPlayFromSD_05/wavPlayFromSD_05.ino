#include <Arduino.h>
#include <M5Unified.h>
#include <SD.h>
#include "WavStreamPlayer.hpp"
#include "SpiBusLock.hpp"

static const char* WAV_PATH = "/wav/UchuuKomichi.wav";
//static const char* WAV_PATH = "/upload.wav";
#ifndef SDCARD_CSPIN
#define SDCARD_CSPIN 4
#endif

//WavStreamPlayer player(32768, 2, 0);
WavStreamPlayer player(24576, 2, 0);


void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);
  Serial.begin(115200);
  delay(200);

  //画面とSDのSPI競合を避けるためのmutexセット
  ensure_spi_mutex();

  bool sdOK = false;
  {
    SpiGuard g;
    sdOK = SD.begin(SDCARD_CSPIN, SPI, 25000000);
  }
  if (!sdOK) {
    Serial.println("SD init failed");
    for (;;) delay(1000);
  }

  auto spk = M5.Speaker.config();
  M5.Speaker.config(spk);
  M5.Speaker.begin();
  M5.Speaker.setVolume(180);

  player.beginAsync(4096, 2, 1, 1);  // stack, prio, core, periodMs
  //player.setVolume(180);

  {
    SpiGuard g;
    M5.Display.clear();
    M5.Display.setTextSize(2);
    M5.Display.println("A=Play  B=Stop");
  }
}

void loop() {
  M5.update();

  if (M5.BtnA.wasPressed()) player.start(WAV_PATH);
  if (M5.BtnB.wasPressed()) player.stop();

  delay(5);
}
