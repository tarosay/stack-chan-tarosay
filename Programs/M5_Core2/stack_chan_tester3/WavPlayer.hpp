//#pragma once
#ifndef _WAVPLAYER_HPP_
#define _WAVPLAYER_HPP_

#include <Arduino.h>
#include <AudioFileSourcePROGMEM.h>
#include <AudioFileSourceSD.h>

class WavPlayer {
public:
  WavPlayer();

  void begin();
  uint32_t play(const uint8_t *wavFile, uint32_t fileSize);
  uint32_t play(const String wavFilename);
  uint32_t getPlayDuration(const uint8_t *wavFile, uint32_t fileSize);
  void repeart();
  void stop();

  static const int COLORS = 16;
  int numPixels = 0;
  int brightness = 25;  // 1が一番明るくて、(階調値 / brightness)の明るさにセットされます

private:
  // int ledNum(int x, int y);
  // uint32_t Colors[COLORS];  //COLORS 個の色テーブル。色番号0は黒色固定
  // //void SetDots(int px, int py, uint32_t fcol, uint32_t bcol, TypeTB tb, int width, byte *data);
};

// グローバルインスタンスの宣言
extern WavPlayer wavPlayer;

#endif  // _WAVPLAYER_HPP_