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
private:
  uint32_t _startMs = 0;
  uint32_t _pakuMs = 200;
  float _mouthMax = 1.0f;
};

// グローバルインスタンスの宣言
extern FaceAvatar faceAvatar;

#endif