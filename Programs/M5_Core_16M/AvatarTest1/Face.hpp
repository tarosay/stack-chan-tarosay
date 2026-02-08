#ifndef FACE_HPP
#define FACE_HPP

#include <Arduino.h>
#include <Avatar.h>
#include <M5Unified.h>

class Face {
public:
  // ★デフォルト引数を使わない（古いgcc対策）
  Face();
  //explicit ServoXY(const Config& cfg);

  void begin();
  // 非ブロッキング移動開始（loopからupdate()を呼ぶ）
  void goKuchiPaku(float size,uint32_t durationMs);
  void stop();
  // 非ブロッキング更新（loopで高頻度に呼ぶ）
  void update();

private:
};

#endif