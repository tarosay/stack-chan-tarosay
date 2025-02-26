// Number2Speech.hpp
#ifndef SPEECH_HPP
#define SPEECH_HPP

#include <Avatar.h>  // https://github.com/meganetaaan/m5stack-avatar
#include "WavPlayer.hpp"

class Speech {
private:
  WavPlayer& wavPlayer;      // WavPlayerの参照
  m5avatar::Avatar& avatar;  // Avatarの参照
  void soundStop();
  void pakupaku();

public:
  // コンストラクタ
  Speech(WavPlayer& player, m5avatar::Avatar& avatarInstance);

  // 数字文字列を音声に変換して再生する
  void playIPNumber(const String& input);
  void playIPNumber(const String& input, float volume);
  void playIP(const String& input);
  void playIP(const String& input, float volume);
  void playNumber(const String& input);
  void playNumber(const String& input, float volume);
  void playSound(const String& input);
  void playSound(const String& input, float volume);
};

#endif  // SPEECH_HPP