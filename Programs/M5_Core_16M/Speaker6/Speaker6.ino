#include <M5Unified.h>
#include <SPI.h>
#include <SD.h>
#include <MP3DecoderHelix.h>

using namespace libhelix;

static File g_mp3;
static MP3DecoderHelix g_dec;

// ===== Ring buffer (mono 16-bit) =====
static constexpr size_t RB_SAMPLES = 32768;   // 64KB
static int16_t rb[RB_SAMPLES];
static volatile size_t rb_w = 0, rb_r = 0, rb_used = 0;

static portMUX_TYPE rbMux = portMUX_INITIALIZER_UNLOCKED;

static volatile uint32_t g_sr = 44100;
static volatile bool     g_sr_ready = false;
static volatile bool     g_eof = false;

// ===== Tunables =====
static constexpr size_t   CHUNK_SAMPLES      = 2048;
static constexpr size_t   PREBUF_SAMPLES     = 30000;
static constexpr uint32_t SILENCE_WARMUP_MS  = 800;

static constexpr int ENQUEUE_BURST = 12;

static constexpr uint8_t  START_VOL   = 120;
static constexpr uint8_t  TARGET_VOL  = 200;
static constexpr uint32_t VOL_RAMP_MS = 300;
static constexpr uint32_t VOL_STEP_MS = 50;

// ===== start-up 2s stats (MP3) =====
static volatile uint32_t g_fail_play = 0;   // playRaw false count (during playback)
static volatile uint32_t g_rb_zero   = 0;   // rb empty count (during playback)
static volatile size_t   g_rb_min2s  = RB_SAMPLES;

static uint32_t g_t_play_start_ms = 0;
static bool     g_printed_2s = false;
static uint32_t g_prebuf_wait_ms = 0;

// ---- ring ops (thread-safe) ----
static inline bool rb_push_one(int16_t s) {
  bool ok = false;
  portENTER_CRITICAL(&rbMux);
  if (rb_used < RB_SAMPLES) {
    rb[rb_w] = s;
    rb_w++;
    if (rb_w >= RB_SAMPLES) rb_w = 0;
    rb_used++;
    ok = true;
  }
  portEXIT_CRITICAL(&rbMux);
  return ok;
}

static inline size_t rb_peek_many(int16_t* out, size_t want) {
  size_t n = 0;
  portENTER_CRITICAL(&rbMux);
  size_t avail = rb_used;
  size_t r = rb_r;
  size_t w = (want < avail) ? want : avail;
  for (size_t i = 0; i < w; ++i) {
    out[i] = rb[r];
    r++;
    if (r >= RB_SAMPLES) r = 0;
  }
  n = w;
  portEXIT_CRITICAL(&rbMux);
  return n;
}

static inline void rb_drop_many(size_t n) {
  portENTER_CRITICAL(&rbMux);
  if (n > rb_used) n = rb_used;
  rb_r = (rb_r + n) % RB_SAMPLES;
  rb_used -= n;
  portEXIT_CRITICAL(&rbMux);
}

static inline size_t rb_used_now() {
  size_t u;
  portENTER_CRITICAL(&rbMux);
  u = rb_used;
  portEXIT_CRITICAL(&rbMux);
  return u;
}

// ---- SD warmup ----
static void sd_warmup(File& f, size_t bytes) {
  uint8_t tmp[4096];
  size_t warm = 0;
  while (warm < bytes) {
    int r = f.read(tmp, sizeof(tmp));
    if (r <= 0) break;
    warm += (size_t)r;
  }
  f.seek(0);
}

// ---- Helix PCM callback (producer) ----
static void pcmCallback(MP3FrameInfo& info, short* pcm, size_t len, void*) {
  g_sr = info.samprate;
  g_sr_ready = true;

  if (info.nChans == 1) {
    for (size_t i = 0; i < len; ++i) {
      while (!rb_push_one((int16_t)pcm[i])) vTaskDelay(1);
    }
  } else {
    for (size_t i = 0; i + 1 < len; i += 2) {
      int32_t m = (int32_t)pcm[i] + (int32_t)pcm[i + 1];
      int16_t mono = (int16_t)(m / 2);
      while (!rb_push_one(mono)) vTaskDelay(1);
    }
  }
}

// ---- Speaker task (consumer) ----
static void speakerTask(void*) {
  static int16_t chunk[CHUNK_SAMPLES];
  static int16_t zeros[CHUNK_SAMPLES] = {0};

  while (!g_sr_ready) vTaskDelay(1);

  // 1) 無音ウォームアップ中は音量0
  M5.Speaker.setVolume(0);

  uint32_t t_end = millis() + SILENCE_WARMUP_MS;
  while ((int32_t)(millis() - t_end) < 0) {
    uint32_t sr = g_sr;
    for (int k = 0; k < ENQUEUE_BURST; ++k) {
      if (!M5.Speaker.playRaw(zeros, CHUNK_SAMPLES, sr, false, 1, 0, false)) break;
    }
    vTaskDelay(1);
  }

  // 2) PCMが十分溜まるまで待つ（先頭は捨てない）
  {
    uint32_t t0 = millis();
    while (rb_used_now() < PREBUF_SAMPLES) vTaskDelay(1);
    g_prebuf_wait_ms = millis() - t0;
  }

  // ---- start-up 2s stats init (MP3 playback start) ----
  g_fail_play = 0;
  g_rb_zero   = 0;
  g_rb_min2s  = rb_used_now();
  g_t_play_start_ms = millis();
  g_printed_2s = false;

  // 3) ここから本再生：最初から聞こえる音量で開始→短時間でTARGETへ
  uint32_t start_ms  = millis();
  uint32_t next_step = start_ms;
  uint8_t  cur_vol   = START_VOL;
  M5.Speaker.setVolume(cur_vol);

  for (;;) {
    // ---- print once at +2s (one line) ----
    uint32_t now2 = millis();
    if (!g_printed_2s && (now2 - g_t_play_start_ms) >= 2000) {
      g_printed_2s = true;
      Serial.printf("[2s][MP3] prebuf_wait=%ums rb_min=%u fail_play=%u rb_zero=%u sr=%u\n",
                    (unsigned)g_prebuf_wait_ms,
                    (unsigned)g_rb_min2s,
                    (unsigned)g_fail_play,
                    (unsigned)g_rb_zero,
                    (unsigned)g_sr);
    }

    // ---- volume ramp (粗い更新) ----
    uint32_t now = millis();
    if ((int32_t)(now - next_step) >= 0) {
      next_step += VOL_STEP_MS;

      uint32_t dt = now - start_ms;
      uint32_t v;
      if (dt >= VOL_RAMP_MS) v = TARGET_VOL;
      else v = START_VOL + (uint32_t)(TARGET_VOL - START_VOL) * dt / VOL_RAMP_MS;

      uint8_t nv = (uint8_t)v;
      if (nv != cur_vol) {
        cur_vol = nv;
        M5.Speaker.setVolume(cur_vol);
      }
    }

    size_t used = rb_used_now();
    if (!g_printed_2s && used < g_rb_min2s) g_rb_min2s = used;

    if (used == 0) {
      if (!g_eof) g_rb_zero++;
      if (g_eof) break;
      vTaskDelay(1);
      continue;
    }

    bool pushed_any = false;
    uint32_t sr = g_sr;

    // ★burst enqueue
    for (int burst = 0; burst < ENQUEUE_BURST; ++burst) {
      used = rb_used_now();
      if (used == 0) break;

      size_t req = (used < CHUNK_SAMPLES) ? used : CHUNK_SAMPLES;
      size_t n = rb_peek_many(chunk, req);
      if (!n) break;

      if (M5.Speaker.playRaw(chunk, n, sr, false, 1, 0, false)) {
        rb_drop_many(n);
        pushed_any = true;
      } else {
        g_fail_play++; // playRawが拒否した回数
        break;
      }
    }

    if (!pushed_any) vTaskDelay(1);
    else vTaskDelay(0);
  }

  // もし2秒に到達せず終了した場合でも、最後に1回出す（保険）
  if (!g_printed_2s) {
    g_printed_2s = true;
    Serial.printf("[2s][MP3] (ended early) prebuf_wait=%ums rb_min=%u fail_play=%u rb_zero=%u sr=%u\n",
                  (unsigned)g_prebuf_wait_ms,
                  (unsigned)g_rb_min2s,
                  (unsigned)g_fail_play,
                  (unsigned)g_rb_zero,
                  (unsigned)g_sr);
  }

  M5.Speaker.setVolume(TARGET_VOL);
  while (rb_used_now()) vTaskDelay(1);

  vTaskDelete(nullptr);
}

// ---- Decode task ----
static void decodeTask(void*) {
  static uint8_t inbuf[4096];
  int yield_ctr = 0;

  for (;;) {
    int n = g_mp3.read(inbuf, sizeof(inbuf));
    if (n > 0) {
      int pos = 0;
      while (pos < n) {
        int step = (n - pos > 1024) ? 1024 : (n - pos);
        g_dec.write(inbuf + pos, step);
        pos += step;

        if (++yield_ctr >= 4) { yield_ctr = 0; vTaskDelay(0); }
      }
    } else {
      g_dec.flush();
      g_eof = true;
      break;
    }
  }

  g_mp3.close();
  vTaskDelete(nullptr);
}

void setup() {
  Serial.begin(115200);
  delay(200);

  auto cfg = M5.config();
  cfg.serial_baudrate = 115200;
  cfg.internal_mic = false;
  cfg.internal_imu = false;
  cfg.internal_rtc = false;
  M5.begin(cfg);

  // Speaker（DAC / GPIO25 / mono）
  auto spk = M5.Speaker.config();
  spk.use_dac = true;
  spk.pin_data_out = GPIO_NUM_25;
  spk.stereo = false;
  spk.sample_rate = 44100;
  spk.i2s_port = (i2s_port_t)0;
  spk.dma_buf_len = 2048;
  spk.dma_buf_count = 16;
  M5.Speaker.config(spk);
  M5.Speaker.begin();
  M5.Speaker.setVolume(0);

  // SD
  SPI.begin(18, 19, 23, 4);
  if (!SD.begin(4, SPI, 20000000)) {
    Serial.println("SD.begin failed");
    for (;;) delay(1000);
  }

  g_mp3 = SD.open("/mp3/file01.mp3", FILE_READ);
  if (!g_mp3) {
    Serial.println("open failed: /mp3/file01.mp3");
    for (;;) delay(1000);
  }

  sd_warmup(g_mp3, 131072);

  // reset ring + flags
  portENTER_CRITICAL(&rbMux);
  rb_w = rb_r = rb_used = 0;
  portEXIT_CRITICAL(&rbMux);

  g_eof = false;
  g_sr = 44100;
  g_sr_ready = false;

  g_dec.setDataCallback(pcmCallback);
  g_dec.begin();

  xTaskCreatePinnedToCore(speakerTask, "speaker", 4096, nullptr, 5, nullptr, 0);
  xTaskCreatePinnedToCore(decodeTask,  "decode",  8192, nullptr, 3, nullptr, 1);
}

void loop() {
  delay(1000);
}
