#include "FaceAvatar.hpp"

FaceAvatar faceAvatar;
m5avatar::Avatar avatar;

const m5avatar::Expression expressions[] = { m5avatar::Expression::Angry, m5avatar::Expression::Sleepy,
                                             m5avatar::Expression::Happy, m5avatar::Expression::Sad,
                                             m5avatar::Expression::Doubt, m5avatar::Expression::Neutral };

FaceAvatar::FaceAvatar() {}

void FaceAvatar::begin(int colordepth) {
  avatar.init(colordepth);  // start drawing w/ 8bit color mode
  avatar.setFace(avatar.getFace());
  avatar.setMouthOpenRatio(0.0f);
  Speaking = false;
}

void FaceAvatar::update() {
  // ★ここが「抱え込み反映ポイント」
  bool doApply = false;
  uint8_t idx = 5;
  // 1) pending反映（Speaking=falseでも必ずここは通す）
  SpeechCmd cmd;
  float size;
  uint32_t pakuMs;

  portENTER_CRITICAL(&_mux);
  if (_exprPending) {
    doApply = true;
    idx = _exprIdx;
    _exprPending = false;
  }
  portEXIT_CRITICAL(&_mux);

  if (doApply) {
    setExpression(idx);  // ← avatar.setExpression() をここでのみ呼ぶ
  }

  portENTER_CRITICAL(&_mux);
  cmd = _speechCmd;
  size = _mouthMax;
  pakuMs = _pakuMs;
  _speechCmd = SpeechCmd::None;
  portEXIT_CRITICAL(&_mux);

  if (cmd == SpeechCmd::Start) goSpeech(size, pakuMs);
  else if (cmd == SpeechCmd::Stop) stop();

  if (!Speaking) return;

  // 経過時間ベース（goSpeech()を呼んだ瞬間から位相が始まる）
  const uint32_t now = millis();
  const uint32_t dt = now - _startMs;
  // 0..1
  const float phase = (dt % _pakuMs) / (float)_pakuMs;
  // サイン波で滑らか（0..1）
  const float s = 0.5f - 0.5f * cosf(2.0f * (float)M_PI * phase);

  avatar.setMouthOpenRatio(s * _mouthMax);
}

void FaceAvatar::goSpeech(float size, uint32_t pakuMs) {
  // 下限（速すぎると見た目が崩れるので適当な安全値）
  if (pakuMs < 40) pakuMs = 40;

  if (size < 0.0f) size = 0.0f;
  if (size > 1.0f) size = 1.0f;

  _pakuMs = pakuMs;
  _mouthMax = size;

  _startMs = millis();  // ★ここが重要：位相の基準点
  Speaking = true;
}

void FaceAvatar::stop() {
  Speaking = false;
  avatar.setMouthOpenRatio(0.0f);
}

void FaceAvatar::setExpression(uint8_t idx) {
  if (idx > 5) idx = 5;
  avatar.setExpression(expressions[idx]);
}

void FaceAvatar::postExpression(uint8_t idx) {
  if (idx > 5) idx = 5;
  portENTER_CRITICAL(&_mux);
  _exprIdx = idx;
  _exprPending = true;
  portEXIT_CRITICAL(&_mux);
}

void FaceAvatar::postSpeechStart(float size, uint32_t pakuMs) {
  portENTER_CRITICAL(&_mux);
  _mouthMax = size;
  _pakuMs = pakuMs;
  _speechCmd = SpeechCmd::Start;
  portEXIT_CRITICAL(&_mux);
}

void FaceAvatar::postSpeechStop() {
  portENTER_CRITICAL(&_mux);
  _speechCmd = SpeechCmd::Stop;
  portEXIT_CRITICAL(&_mux);
}