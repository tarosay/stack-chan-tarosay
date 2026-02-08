#include <stdint.h>
#ifndef FACEAVATAR_HPP
#define FACEAVATAR_HPP

#include <Arduino.h>
#include <Avatar.h>
#include <M5Unified.h>

class FaceAvatar {
public:
  FaceAvatar();
  //explicit ServoXY(const Config& cfg);

  void begin(int colordepth);

  // size: 0.0〜1.0（最大開き）
  // pakuMs: 1周期の時間(ms)。小さいほど速い
  void goSpeech(float size, uint32_t durationMs);
  void stop();
  void update();

  void setExpression(uint8_t idx);

  bool Speaking = false;

  void postExpression(uint8_t idx);  // 受信側（MQTT側）から呼ぶ：要求を積むだけ
  void postSpeechStart(float size, uint32_t pakuMs); // MQTT側から：要求だけ
  void postSpeechStop();                             // MQTT側から：要求だけ

private:
  uint32_t _startMs = 0;
  uint32_t _pakuMs = 200;
  float _mouthMax = 1.0f;

  portMUX_TYPE _mux = portMUX_INITIALIZER_UNLOCKED;
  bool _exprPending = false;
  uint8_t _exprIdx = 5;              // neutral
    enum class SpeechCmd : uint8_t { None, Start, Stop };
  SpeechCmd _speechCmd = SpeechCmd::None;
  //float _speechSize = 1.0f;
  //uint32_t _speechPakuMs = 200;
};

// グローバルインスタンスの宣言
extern FaceAvatar faceAvatar;

#endif