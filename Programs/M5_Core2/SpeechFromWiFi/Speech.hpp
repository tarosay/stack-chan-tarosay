// Number2Speech.hpp
#ifndef SPEECH_HPP
#define SPEECH_HPP

#include <Avatar.h>       // https://github.com/meganetaaan/m5stack-avatar
#include "WavPlayer.hpp"  // 自作クラス WavPlayer のヘッダファイル

class Speech {
private:
  WavPlayer& wavPlayer;      // WavPlayerの参照
  m5avatar::Avatar& avatar;  // Avatarの参照

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
  void playWav(const String& input);
  void playWav(const String& input, float volume);
};

#endif  // SPEECH_HPP