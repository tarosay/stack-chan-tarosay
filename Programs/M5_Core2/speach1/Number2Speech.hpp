// Number2Speech.hpp
#ifndef NUMBER2SPEECH_HPP
#define NUMBER2SPEECH_HPP

#include <Avatar.h>       // https://github.com/meganetaaan/m5stack-avatar
#include "WavPlayer.hpp"  // 自作クラス WavPlayer のヘッダファイル

class Number2Speech {
private:
  WavPlayer& wavPlayer;      // WavPlayerの参照
  m5avatar::Avatar& avatar;  // Avatarの参照

public:
  // コンストラクタ
  Number2Speech(WavPlayer& player, m5avatar::Avatar& avatarInstance);

  // 数字文字列を音声に変換して再生する
  void play(const String& input);
  void playIP(const String& input);
  void play(const String& input, float volume);
  void playIP(const String& input, float volume);
};

#endif  // NUMBER2SPEECH_HPP