#ifndef _MP3PLAYER_HPP_
#define _MP3PLAYER_HPP_

#include <Arduino.h>
#include <AudioFileSourcePROGMEM.h>
#include <AudioFileSourceSD.h>
#include <AudioFileSourceID3.h>

class Mp3Player {
public:
  Mp3Player();

  void begin();
  void play(const uint8_t *mp3File, uint32_t fileSize);
  void play(const String &mp3Filename);
  void play(const String &mp3Filename, float volume);

  void repeart();  // repeat: 直前再生データを再度再生
  void stop();
  bool isPlaying();
private:
};

// グローバルインスタンスの宣言（必要に応じて）
extern Mp3Player mp3Player;

#endif  // _MP3PLAYER_HPP_