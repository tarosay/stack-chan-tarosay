#include <M5Unified.h>
#include "Speech.hpp"

//#include "esp_spiram.h"

// コンストラクタ
Speech::Speech(WavPlayer& player, m5avatar::Avatar* avatar)
  : wavPlayer(player), avatar(avatar),
    IsPakuPaku(true) {}

//確実な再生停止
void Speech::soundStop() {
  //wavPlayer.stop();
  //mp3Player.stop();
  while (wavPlayer.isPlaying()) {
    delay(1);
  }
}

//口パクパク
void Speech::pakupaku() {
  while (wavPlayer.isPlaying()) {
    //M5_LOGI("PaKu");
    //口をパクパクする
    if (IsPakuPaku) {
      avatar->setMouthOpenRatio(0.7);
      delay(100);
      avatar->setMouthOpenRatio(0.0);
      delay(150);
    } else {
      delay(100);
    }
  }
}

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
      filename = "/wav/";
      filename += c;
      filename += ".wav";
    } else if (c == '.') {
      filename = "/wav/t.wav";  // '.' に対応する "てん" の音声
    }

    if (filename.isEmpty()) {
      continue;
    }

    // WAVファイルを再生
    soundStop();
    wavPlayer.play(filename.c_str(), volume);

    //口パクパク
    pakupaku();
  }
}

// 数字文字列を音声に変換して再生する
void Speech::playIPNumber(const String& input) {
  playIPNumber(input, 0.9);
}
void Speech::playIPNumber(const String& input, float volume) {
  // ドットで文字列を分割
  size_t start = 0;
  size_t dotIndex;

  while (start < input.length()) {
    dotIndex = input.indexOf('.', start);  // 次のドットを探す

    // ドットが見つからなければ最後のセグメント
    if (dotIndex == -1) {
      dotIndex = input.length();
    }

    // セグメントを抽出
    String segment = input.substring(start, dotIndex);

    // 先頭の0を削除
    while (segment.length() > 1 && segment.charAt(0) == '0') {
      segment.remove(0, 1);
    }

    // 有効なセグメントの場合、ファイル名を生成して再生
    if (segment.length() > 0) {
      String filename = "/wav/" + segment + ".wav";
      //M5_LOGI("filename: %s", filename.c_str());
      soundStop();
      wavPlayer.play(filename.c_str(), volume);
    }

    //口パクパク
    pakupaku();

    // ドットの音声 "t.wav" を再生
    if (dotIndex < input.length()) {
      soundStop();
      wavPlayer.play("/wav/t.wav", volume);
    }

    //口パクパク
    pakupaku();
    // 次のセグメントの開始位置
    start = dotIndex + 1;
  }
}

void Speech::playIP(const String& input) {
  playIP(input, 0.9);
}
void Speech::playIP(const String& input, float volume) {

  soundStop();
  wavPlayer.play("/wav/ip.wav", volume);

  //口パクパク
  pakupaku();

  playIPNumber(input);

  soundStop();
  wavPlayer.play("/wav/desu.wav", volume);

  //口パクパク
  pakupaku();
}

void Speech::playSound(const String& input) {
  playSound(input, 0.9);
}

void Speech::playSound(const String& input, float volume) {
  if (input.endsWith(".wav")) {
    soundStop();
    wavPlayer.play(input, volume);
    //口パクパク
    pakupaku();

  } else {
    M5_LOGI("File Not open %s", input.c_str());
    return;
  }
}
void Speech::setPakuPaku(bool ispakupaku) {
  IsPakuPaku = ispakupaku;
}
