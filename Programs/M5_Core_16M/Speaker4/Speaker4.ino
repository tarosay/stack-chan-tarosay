#include <M5Unified.h>
#include <SPI.h>
#include <SD.h>
#include <MP3DecoderHelix.h>
using namespace libhelix;

static File g_mp3;
static MP3DecoderHelix g_dec;

// ==== PCMリング（mono 16-bit）====
static constexpr size_t RB_SAMPLES = 32768;   // 64KB
static int16_t rb[RB_SAMPLES];
static volatile size_t rb_w = 0, rb_r = 0, rb_used = 0;

static volatile uint32_t g_sr = 44100;

// forward
static void feedSpeaker();

static inline void rb_push_blocking(int16_t s) {
  // 満杯なら空くまで待つ（捨てない）
  while (rb_used >= RB_SAMPLES) {
    feedSpeaker();
    delay(1);
  }
  rb[rb_w] = s;
  rb_w = (rb_w + 1) % RB_SAMPLES;
  rb_used++;
}

static size_t rb_peek(int16_t* out, size_t n) {
  size_t k = 0;
  size_t idx = rb_r;
  size_t avail = rb_used;
  while (k < n && avail) {
    out[k++] = rb[idx];
    idx = (idx + 1) % RB_SAMPLES;
    avail--;
  }
  return k;
}

static void rb_drop(size_t n) {
  if (n > rb_used) n = rb_used;
  rb_r = (rb_r + n) % RB_SAMPLES;
  rb_used -= n;
}

// Helix callback
static void pcmCallback(MP3FrameInfo& info, short* pcm, size_t len, void*) {
  g_sr = info.samprate;

  if (info.nChans == 1) {
    for (size_t i = 0; i < len; ++i) rb_push_blocking((int16_t)pcm[i]);
  } else {
    // interleaved LRLR... → mono
    for (size_t i = 0; i + 1 < len; i += 2) {
      int32_t m = (int32_t)pcm[i] + (int32_t)pcm[i + 1];
      rb_push_blocking((int16_t)(m / 2));
    }
  }
}

static constexpr size_t CHUNK = 1024;              // 512〜2048で調整
static constexpr size_t PREBUF = 16384;            // 44.1kHzで約0.37秒
static bool started = false;

static void feedSpeaker() {
  static int16_t chunk[CHUNK];

  if (!started) {
    if (rb_used < PREBUF) return;                  // 先に貯める
    started = true;
  }

  while (rb_used && M5.Speaker.isPlaying(0) != 2) {
    size_t req = (rb_used < CHUNK) ? rb_used : CHUNK;
    size_t n = rb_peek(chunk, req);
    if (!n) break;

    if (M5.Speaker.playRaw(chunk, n, g_sr, false, 1, 0, false)) {
      rb_drop(n);
    } else {
      delay(1);
      break;
    }
  }
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
  spk.dma_buf_len = 1024;     // 途切れ耐性アップ
  spk.dma_buf_count = 16;
  M5.Speaker.config(spk);

  M5.Speaker.begin();
  M5.Speaker.setVolume(200);

  SPI.begin(18, 19, 23, 4);
  if (!SD.begin(4, SPI, 20000000)) { // 20MHz（安定寄り）
    Serial.println("SD.begin failed");
    for(;;) delay(1000);
  }

  g_mp3 = SD.open("/mp3/file01.mp3", FILE_READ);
  if (!g_mp3) {
    Serial.println("open failed");
    for(;;) delay(1000);
  }
  Serial.printf("size=%u\n", (unsigned)g_mp3.size());

  g_dec.setDataCallback(pcmCallback);
  g_dec.begin();
}

void loop() {
  static uint8_t inbuf[2048];   // 小さく刻む（デコード暴走を防ぐ）
  int n = g_mp3.read(inbuf, sizeof(inbuf));
  if (n > 0) {
    // さらに刻んで feedSpeaker を挟む
    int pos = 0;
    while (pos < n) {
      int step = (n - pos > 512) ? 512 : (n - pos);
      g_dec.write(inbuf + pos, step);
      pos += step;
      feedSpeaker();
    }
  } else {
    // EOF
    g_dec.flush();
    while (rb_used) { feedSpeaker(); delay(1); }
    for(;;) delay(1000);
  }

  feedSpeaker();
}
