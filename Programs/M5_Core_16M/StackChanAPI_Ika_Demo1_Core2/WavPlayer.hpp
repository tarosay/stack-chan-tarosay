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
  void play(const uint8_t *wavFile, uint32_t fileSize);
  void play(const String &wavFilename);
  void play(const String &wavFilename, float volume);

  void repeart();
  void stop();
  bool isPlaying();
private:
};

// グローバルインスタンスの宣言
extern WavPlayer wavPlayer;

#endif  // _WAVPLAYER_HPP_