#include <M5Unified.h>
#include "Speech.hpp"

// コンストラクタ
Speech::Speech(WavPlayer& player, m5avatar::Avatar& avatarInstance)
  : wavPlayer(player), avatar(avatarInstance) {}

// 数字文字列を音声に変換して再生する
void Speech::playNumber(const String& input) {
  playNumber(input, 0.9);
}
void Speech::playNumber(const String& input, float volume) {
  for (size_t i = 0; i < input.length(); i++) {
    char c = input.charAt(i);

    String filename = "";
    // 数字または '.' のチェック
    if (c >= '0' && c <= '9') {
      filename = "/";
      filename += c;
      filename += ".wav";
    } else if (c == '.') {
      filename = "/t.wav";  // '.' に対応する "てん" の音声
    }

    if (filename.isEmpty()) {
      continue;
    }

    // WAVファイルを再生
    uint32_t end_mouth_millis = millis() + wavPlayer.play(filename.c_str(), volume) - 200;

    while (wavPlayer.isPlaying()) {
      //口をパクパクする
      delay(100);
      avatar.setMouthOpenRatio(0.7);
      delay(150);
      avatar.setMouthOpenRatio(0.0);
    }
  }
}
void Speech::playIP(const String& input) {
  playIP(input, 0.9);
}
void Speech::playIP(const String& input, float volume) {

  uint32_t end_mouth_millis = millis() + wavPlayer.play("/ip.wav", volume) - 200;
  //M5_LOGI("ip.wav: %d", dut);
  //口をパクパクする時間
  // while (end_mouth_millis >= millis()) {
  //   delay(200);
  //   avatar.setMouthOpenRatio(0.7);
  //   delay(150);
  //   avatar.setMouthOpenRatio(0.0);
  // }

  // 再生中のフラグが解除されるまで待機
  while (wavPlayer.isPlaying()) {
    //口をパクパクする
    delay(100);
    avatar.setMouthOpenRatio(0.7);
    delay(150);
    avatar.setMouthOpenRatio(0.0);
  }

  playNumber(input);

  end_mouth_millis = millis() + wavPlayer.play("/desu.wav", volume) - 200;
  //M5_LOGI("desu.wav: %d", dut);
  // //口をパクパクする時間
  // while (end_mouth_millis >= millis()) {
  //   delay(200);
  //   avatar.setMouthOpenRatio(0.7);
  //   delay(150);
  //   avatar.setMouthOpenRatio(0.0);
  // }

  // 再生中のフラグが解除されるまで待機
  while (wavPlayer.isPlaying()) {
    //口をパクパクする
    delay(100);
    avatar.setMouthOpenRatio(0.7);
    delay(150);
    avatar.setMouthOpenRatio(0.0);
  }
}