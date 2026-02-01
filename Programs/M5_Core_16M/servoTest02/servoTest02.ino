struct XYPos {
  int x;
  int y;
};

#include <Arduino.h>
#include <M5Unified.h>
#include <Wire.h>
#include <driver/gpio.h>   // gpio_reset_pin を使う場合

static constexpr uint32_t SERVO_HZ = 50;
static constexpr uint8_t RES_BITS = 16;
static constexpr uint32_t PERIOD_US = 1000000UL / SERVO_HZ;
static constexpr uint32_t DUTY_MAX = (1UL << RES_BITS) - 1;

static constexpr uint8_t SERVO_X_PIN = 22;
static constexpr uint8_t SERVO_Y_PIN = 21;
static constexpr uint8_t CH_X = 0;
static constexpr uint8_t CH_Y = 1;

static constexpr int MIN_US_X = 600;
static constexpr int MAX_US_X = 2300;
static constexpr int MIN_US_Y = 600;
static constexpr int MAX_US_Y = 2300;

// 速度上限（deg/sec）…実機で詰める
static constexpr uint32_t MAX_DEG_PER_SEC_X = 300;
static constexpr uint32_t MAX_DEG_PER_SEC_Y = 300;

// 更新周期（50Hzなら20msが筋）
static constexpr uint32_t SERVO_TICK_MS = 20;

static inline int clampDeg(int deg) {
  if (deg < -90) deg = -90;
  if (deg > 90) deg = 90;
  return deg;
}

static inline uint32_t us_to_duty(uint32_t us) {
  return us * DUTY_MAX / PERIOD_US;
}

static inline void ledcWriteUs(uint8_t ch, int us) {
  if (ch == CH_X) {
    if (us < MIN_US_X) us = MIN_US_X;
    if (us > MAX_US_X) us = MAX_US_X;
  } else {
    if (us < MIN_US_Y) us = MIN_US_Y;
    if (us > MAX_US_Y) us = MAX_US_Y;
  }
  ledcWriteChannel(ch, us_to_duty((uint32_t)us));
}

static inline void servoWriteDeg(uint8_t ch, int deg) {
  deg = clampDeg(deg);
  int us;
  if (ch == CH_X) us = MIN_US_X + (deg + 90) * (MAX_US_X - MIN_US_X) / 180;
  else us = MIN_US_Y + (deg + 90) * (MAX_US_Y - MIN_US_Y) / 180;
  ledcWriteUs(ch, us);
}

static inline void applyXY(const XYPos& p) {
  servoWriteDeg(CH_X, p.x);
  servoWriteDeg(CH_Y, p.y);
}

// --- 目的地セット（-90..+90） ---
static XYPos g_cur = { 0, 0 };
static XYPos g_next = { 0, 0 };

static inline void setNextXY(int x1, int y1) {
  g_next.x = clampDeg(x1);
  g_next.y = clampDeg(y1);
}

// --- 移動状態（非ブロッキング） ---
struct MoveState {
  bool active = false;
  XYPos start{ 0, 0 };
  XYPos target{ 0, 0 };
  uint32_t t0 = 0;
  uint32_t duration = 0;
  uint32_t nextTick = 0;
} g_mv;

static inline uint32_t ceil_div_u32(uint32_t a, uint32_t b) {
  return (a + b - 1) / b;
}

// 指定msを「速度上限」＋「tick境界」にクランプ
static uint32_t clampDurationMs(const XYPos& s, const XYPos& t, uint32_t reqMs) {
  const uint32_t dx = (uint32_t)abs(t.x - s.x);
  const uint32_t dy = (uint32_t)abs(t.y - s.y);

  const uint32_t minMsX = (MAX_DEG_PER_SEC_X == 0) ? 0 : ceil_div_u32(dx * 1000UL, MAX_DEG_PER_SEC_X);
  const uint32_t minMsY = (MAX_DEG_PER_SEC_Y == 0) ? 0 : ceil_div_u32(dy * 1000UL, MAX_DEG_PER_SEC_Y);
  uint32_t minMs = (minMsX > minMsY) ? minMsX : minMsY;

  uint32_t ms = reqMs;
  if (ms < minMs) ms = minMs;

  // tick(20ms)に丸め（0は許可）
  if (ms != 0) {
    ms = ceil_div_u32(ms, SERVO_TICK_MS) * SERVO_TICK_MS;
  }
  return ms;
}

static void moveBegin(uint32_t durationMs) {
  g_mv.start = g_cur;
  g_mv.target = g_next;

  uint32_t ms = clampDurationMs(g_mv.start, g_mv.target, durationMs);

  if (ms == 0 || (g_mv.start.x == g_mv.target.x && g_mv.start.y == g_mv.target.y)) {
    g_cur = g_mv.target;
    applyXY(g_cur);
    g_mv.active = false;
    return;
  }

  g_mv.duration = ms;
  g_mv.t0 = millis();
  g_mv.nextTick = g_mv.t0;  // 即時更新してからtickで進める
  g_mv.active = true;
}

static inline bool isMoving() {
  return g_mv.active;
}

// loop()から頻繁に呼ぶ（止まらない）
static void moveUpdate() {
  if (!g_mv.active) return;

  const uint32_t now = millis();

  // tickより早い呼び出しは無視（処理コスト削減）
  if ((int32_t)(now - g_mv.nextTick) < 0) return;
  g_mv.nextTick += SERVO_TICK_MS;

  const uint32_t elapsed = now - g_mv.t0;
  if (elapsed >= g_mv.duration) {
    g_cur = g_mv.target;
    applyXY(g_cur);
    g_mv.active = false;
    return;
  }

  // 補間（float無し）
  const int x = g_mv.start.x + (int32_t)(g_mv.target.x - g_mv.start.x) * (int32_t)elapsed / (int32_t)g_mv.duration;
  const int y = g_mv.start.y + (int32_t)(g_mv.target.y - g_mv.start.y) * (int32_t)elapsed / (int32_t)g_mv.duration;

  servoWriteDeg(CH_X, x);
  servoWriteDeg(CH_Y, y);
}

// ブロッキングで「必ず移動を完了」させる（ただしOSへは譲る）
static void moveNextBlocking(uint32_t durationMs) {
  moveBegin(durationMs);

  // moveUpdate()がtick管理している前提なので、ここは“待つだけ”
  while (isMoving()) {
    moveUpdate();

    // ★重要：ここで必ずCPUを譲る（MQTT xTaskも動く）
    // 1msで十分。20ms周期にしたいなら vTaskDelay(pdMS_TO_TICKS(1)) でもOK
    delay(1);
  }
}

void setup() {
  Serial.begin(115200);
  auto cfg = M5.config();

  // そもそもI2C系デバイスを使わないなら、I2Cを使う内蔵機能を切る（後述）
  cfg.internal_imu = false;
  cfg.internal_rtc = false;
  cfg.internal_mic = false;
  cfg.internal_spk = false;

  //M5.begin(cfg);

// ★重要：M5Stackは In/Ex が 21/22 を共有し得るので両方解放
  M5.In_I2C.release();
  M5.Ex_I2C.release();

  // （念のため）ピン機能をリセットしてからPWMへ
  gpio_reset_pin((gpio_num_t)21);
  gpio_reset_pin((gpio_num_t)22);

  delay(2000);

  ledcAttachChannel(22, SERVO_HZ, RES_BITS, CH_X);
  ledcAttachChannel(21, SERVO_HZ, RES_BITS, CH_Y);

  g_cur = { 0, 0 };
  g_next = g_cur;
  applyXY(g_cur);


  // 例：最初の移動を開始（この後も止まらない）
  setNextXY(90, -90);
  moveNextBlocking(1000);

  setNextXY(-90, 90);
  moveBegin(5000);  // 長くても他は止まらない
}

void loop() {
  // ★ここが肝：移動はここで少しずつ進むだけ
  moveUpdate();

  // --- ここに他の処理（MQTT/表示/センサ等）を書いてOK ---
  // 例：移動が終わったら次を予約
  static uint32_t lastAction = 0;
  if (!isMoving() && (millis() - lastAction) > 500) {
    lastAction = millis();
    static int s = 0;
    if (s == 0) {
      setNextXY(-90, -90);
      moveBegin(800);
    }
    if (s == 1) {
      setNextXY(0, 0);
      moveBegin(800);
    }
    if (s == 2) {
      setNextXY(90, 90);
      moveBegin(800);
    }
    if (s == 3) {
      setNextXY(0, 0);
      moveBegin(800);
    }
    s = (s + 1) & 3;
  }
}
