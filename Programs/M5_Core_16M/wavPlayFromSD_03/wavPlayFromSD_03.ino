#include <Arduino.h>
#include <M5Unified.h>
#include <SD.h>

static const char* WAV_PATH = "/upload.wav";
#ifndef SDCARD_CSPIN
#define SDCARD_CSPIN 4
#endif

static constexpr size_t CHUNK_BYTES = 8192;  // ★まずここを大きく（16k/16bit/monoで約256ms）
static constexpr uint32_t EARLY_MS = 2;      // 供給を少し早める（任意）

struct WavInfo {
  uint16_t audioFormat = 0;  // 1=PCM
  uint16_t channels = 0;
  uint32_t sampleRate = 0;
  uint32_t byteRate = 0;
  uint16_t blockAlign = 0;
  uint16_t bitsPerSample = 0;
  uint32_t dataOffset = 0;
  uint32_t dataSize = 0;
};

static bool readU16(File& f, uint16_t& v) {
  uint8_t b[2];
  if (f.read(b, 2) != 2) return false;
  v = (uint16_t)(b[0] | (b[1] << 8));
  return true;
}
static bool readU32(File& f, uint32_t& v) {
  uint8_t b[4];
  if (f.read(b, 4) != 4) return false;
  v = (uint32_t)(b[0] | (b[1] << 8) | (b[2] << 16) | (b[3] << 24));
  return true;
}

static bool parseWav(File& f, WavInfo& wi) {
  if (!f.seek(0)) return false;
  char riff[4], wave[4];
  uint32_t riffSize;
  if (f.read((uint8_t*)riff, 4) != 4) return false;
  if (memcmp(riff, "RIFF", 4) != 0) return false;
  if (!readU32(f, riffSize)) return false;
  if (f.read((uint8_t*)wave, 4) != 4) return false;
  if (memcmp(wave, "WAVE", 4) != 0) return false;

  bool gotFmt = false, gotData = false;
  while (f.available()) {
    char id[4];
    uint32_t size;
    if (f.read((uint8_t*)id, 4) != 4) break;
    if (!readU32(f, size)) break;
    uint32_t chunkStart = f.position();

    if (memcmp(id, "fmt ", 4) == 0) {
      if (size < 16) return false;
      uint16_t af, ch, ba, bps;
      uint32_t sr, br;
      if (!readU16(f, af)) return false;
      if (!readU16(f, ch)) return false;
      if (!readU32(f, sr)) return false;
      if (!readU32(f, br)) return false;
      if (!readU16(f, ba)) return false;
      if (!readU16(f, bps)) return false;
      wi.audioFormat = af;
      wi.channels = ch;
      wi.sampleRate = sr;
      wi.byteRate = br;
      wi.blockAlign = ba;
      wi.bitsPerSample = bps;
      gotFmt = true;
    } else if (memcmp(id, "data", 4) == 0) {
      wi.dataOffset = chunkStart;
      wi.dataSize = size;
      gotData = true;
      break;
    }
    uint32_t next = chunkStart + size + (size & 1);
    if (!f.seek(next)) break;
  }
  return gotFmt && gotData;
}

static void speakerSetupFromWav(const WavInfo& wi) {
  auto spk = M5.Speaker.config();
  spk.sample_rate = wi.sampleRate;
  spk.stereo = (wi.channels == 2);
  M5.Speaker.config(spk);
  M5.Speaker.begin();
  M5.Speaker.setVolume(200);
}

static inline void fade_edges_pcm16(int16_t* s, size_t samples, uint32_t sampleRate) {
  const uint32_t fadeMs = 2;                              // 2ms
  size_t fadeN = (size_t)((sampleRate * fadeMs) / 1000);  // 16kHzなら32サンプル
  if (fadeN < 8) fadeN = 8;
  if (fadeN * 2 > samples) fadeN = samples / 2;

  // fade-in
  for (size_t i = 0; i < fadeN; i++) {
    float g = (float)i / (float)fadeN;
    s[i] = (int16_t)((float)s[i] * g);
  }
  // fade-out
  for (size_t i = 0; i < fadeN; i++) {
    float g = (float)(fadeN - i) / (float)fadeN;
    size_t idx = samples - fadeN + i;
    s[idx] = (int16_t)((float)s[idx] * g);
  }
}


class StreamPlayer {
public:
  bool start(const char* path) {
    stop();

    file_ = SD.open(path, FILE_READ);
    if (!file_) return false;

    if (!parseWav(file_, wi_)) {
      file_.close();
      return false;
    }
    if (!(wi_.audioFormat == 1 && wi_.channels == 1 && wi_.bitsPerSample == 16)) {
      file_.close();
      return false;
    }
    if (!file_.seek(wi_.dataOffset)) {
      file_.close();
      return false;
    }

    speakerSetupFromWav(wi_);
    bytesPerSec_ = wi_.sampleRate * wi_.blockAlign;  // 16000*2=32000
    remain_ = wi_.dataSize;

    // 先読み2段
    cur_len_ = readChunk_(buf0_);
    next_len_ = readChunk_(buf1_);
    if (cur_len_ == 0) {
      stop();
      return false;
    }

    // まずcurを再生開始
    playChunk_(buf0_, cur_len_);
    // 「次の切替時刻」を設定
    waitUntil_ = millis() + durMsFor_(cur_len_);

    playing_ = true;
    stopping_ = false;
    curIsBuf0_ = true;
    return true;
  }

  void stop() {
    stopping_ = true;
    playing_ = false;
    waitUntil_ = 0;
    cur_len_ = next_len_ = 0;
    remain_ = 0;
    if (file_) file_.close();
    M5.Speaker.stop();
  }

  bool isPlaying() const {
    return playing_;
  }

  void service() {
    if (!playing_) return;
    if (stopping_) {
      stop();
      return;
    }

    // 次チャンクへ切り替えるタイミングか？
    if (waitUntil_ != 0 && (int32_t)(millis() - waitUntil_) >= 0) {
      // nextが無い＝終わり
      if (next_len_ == 0) {
        stop();  // きれいに止める（必要ならfinishに分けてもOK）
        return;
      }

      // nextを即再生（境目でSD readしないのがポイント）
      if (curIsBuf0_) {
        playChunk_(buf1_, next_len_);
      } else {
        playChunk_(buf0_, next_len_);
      }
      waitUntil_ = millis() + durMsFor_(next_len_);

      // 再生中に「次の次」を読む（今鳴ってるバッファとは逆側へ）
      if (curIsBuf0_) {
        curIsBuf0_ = false;  // 今はbuf1再生中
        cur_len_ = next_len_;
        next_len_ = readChunk_(buf0_);  // buf0を次用に先読み
      } else {
        curIsBuf0_ = true;  // 今はbuf0再生中
        cur_len_ = next_len_;
        next_len_ = readChunk_(buf1_);  // buf1を次用に先読み
      }
    }
  }

private:
  uint32_t durMsFor_(size_t bytes) const {
    uint32_t d = (uint32_t)((1000.0 * (double)bytes) / (double)bytesPerSec_ + 0.5);
    if (d > EARLY_MS) d -= EARLY_MS;
    return d;
  }

  size_t readChunk_(uint8_t* dst) {
    if (!file_ || remain_ == 0) return 0;
    uint32_t n = (remain_ > CHUNK_BYTES) ? CHUNK_BYTES : remain_;
    n = (n / wi_.blockAlign) * wi_.blockAlign;
    if (n == 0) return 0;

    int r = file_.read(dst, n);
    if (r <= 0) return 0;
    remain_ -= (uint32_t)r;
    return (size_t)r;
  }

  void playChunk_(const uint8_t* bytes, size_t len) {
    const size_t samples = len / 2;  // PCM16 mono
    auto* pcm = (int16_t*)bytes;

    fade_edges_pcm16(pcm, samples, wi_.sampleRate);  // ★ここが追加点

    M5.Speaker.playRaw(pcm, samples, wi_.sampleRate, false);
  }

  File file_;
  WavInfo wi_;
  uint32_t bytesPerSec_ = 0;
  uint32_t remain_ = 0;

  bool playing_ = false;
  bool stopping_ = false;

  uint8_t buf0_[CHUNK_BYTES];
  uint8_t buf1_[CHUNK_BYTES];
  bool curIsBuf0_ = true;

  size_t cur_len_ = 0;
  size_t next_len_ = 0;

  uint32_t waitUntil_ = 0;
};

static StreamPlayer player;

void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);
  Serial.begin(115200);
  delay(200);

  if (!SD.begin(SDCARD_CSPIN, SPI, 25000000)) {
    Serial.println("SD init failed");
    for (;;) delay(1000);
  }

  M5.Display.clear();
  M5.Display.setTextSize(2);
  M5.Display.println("A=Play  B=Stop");
}

void loop() {
  M5.update();

  if (M5.BtnB.wasPressed()) {
    player.stop();
  }

  if (!player.isPlaying() && M5.BtnA.wasPressed()) {
    player.start(WAV_PATH);
  }

  player.service();
  delay(1);
}
