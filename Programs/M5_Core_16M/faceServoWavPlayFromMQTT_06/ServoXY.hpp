#ifndef SERVOXY_HPP
#define SERVOXY_HPP

#include <Arduino.h>

// Default JSON path on SD
#define SERVOJSON "/json/SC_BasicConfig.json"

class ServoXY {
public:
  struct XYPos {
    int x;
    int y;
  };

  struct Config {
    // pins/ch
    uint8_t pinX = 22;
    uint8_t pinY = 21;
    uint8_t chX = 6;
    uint8_t chY = 5;

    // PWM
    uint32_t hz = 50;
    uint8_t resBits = 16;
    uint32_t tickMs = 20;  // 50Hzなら20ms推奨

    // pulse range (保守的初期値。実機で詰める)
    int minUsX = 600;
    int maxUsX = 2300;
    int minUsY = 600;
    int maxUsY = 2300;

    // speed limit (deg/sec) 0なら無制限
    uint32_t maxDegPerSecX = 300;
    uint32_t maxDegPerSecY = 300;
  };

  // ★デフォルト引数を使わない（古いgcc対策）
  ServoXY();
  explicit ServoXY(const Config& cfg);

  void begin(int cx, int cy);

  // SD上のJSON（{"servo":{...}}）から設定を読み込み、centerで初期化する。
  // 戻り値: 読めたらtrue（SD/JSON失敗ならfalse、既存設定は維持）
  bool begin(const char* jsonPath = SERVOJSON);

  // JSONを読み込んだ上で、指定初期値で開始（centerを上書き）
  bool begin(const char* jsonPath, int cx, int cy);

  // 目的地をセット（-90..+90）
  void setNext(int x1, int y1);

  XYPos cur() const;
  XYPos next() const;

  // 非ブロッキング移動開始（loopからupdate()を呼ぶ）
  void moveBegin(uint32_t durationMs);
  void moveBegin(int x1, int y1, uint32_t durationMs);

  bool moving() const;

  // 非ブロッキング更新（loopで高頻度に呼ぶ）
  void update();

  // 優先して完了させる（呼び出し元はブロックするが、delayで必ずyieldするので他タスクは動く）
  void moveBlocking(uint32_t durationMs);
  void moveBlocking(int x1, int y1, uint32_t durationMs);

  // 調整API
  void setSpeedLimit(uint32_t degPerSecX, uint32_t degPerSecY);
  void setPulseRangeX(int minUs, int maxUs);
  void setPulseRangeY(int minUs, int maxUs);

  void paramList();
private:
  struct MoveState {
    bool active = false;
    XYPos start{ 0, 0 };
    XYPos target{ 0, 0 };
    uint32_t t0 = 0;
    uint32_t duration = 0;
    uint32_t nextTick = 0;
  };

  Config cfg_;
  XYPos cur_{ 0, 0 };
  XYPos next_{ 0, 0 };

  // logical -> output adjustment
  XYPos offset_{ 0, 0 };

  // logical limits (inclusive)
  XYPos lower_{ -70, -45 };
  XYPos upper_{ 70, 0 };
  MoveState mv_;

  uint32_t periodUs_ = 20000;
  uint32_t dutyMax_ = 65535;

  static int clampDeg(int deg);
  static uint32_t ceil_div_u32(uint32_t a, uint32_t b);

  uint32_t usToDuty_(uint32_t us) const;
  void writeUs_(uint8_t ch, int us) const;
  void writeDeg_(uint8_t ch, int deg) const;
  void applyXY_(const XYPos& p) const;
  uint32_t clampDurationMs_(const XYPos& s, const XYPos& t, uint32_t reqMs) const;
};

#endif
