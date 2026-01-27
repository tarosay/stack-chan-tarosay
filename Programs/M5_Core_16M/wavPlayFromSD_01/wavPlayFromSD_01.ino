#include <Arduino.h>
#include <M5Unified.h>
#include <SD.h>

static const char* WAV_PATH = "/upload.wav";
#ifndef SDCARD_CSPIN
#define SDCARD_CSPIN 4
#endif

static constexpr size_t CHUNK_BYTES = 4096;  // 境目減らす
static constexpr uint32_t EARLY_MS = 2;      // 早め供給

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

class StreamPlayer {
public:
  bool start(const char* path) {
    stop();  // 念のため前回を止める

    file_ = SD.open(path, FILE_READ);
    if (!file_) return false;

    if (!parseWav(file_, wi_)) {
      file_.close();
      return false;
    }

    // このテストはPCM16 mono前提
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
    bi_ = 0;
    stopping_ = false;
    playing_ = true;

    // 先に1チャンク読む（サービス開始を軽くする）
    return true;
  }

  void stop() {
    stopping_ = true;
    playing_ = false;
    remain_ = 0;
    if (file_) file_.close();
    // 可能なら即停止（効く環境では音が止まる）
    M5.Speaker.stop();
  }

  bool isPlaying() const {
    return playing_;
  }

  // loopから頻繁に呼ぶ：再生を進める
  void service() {
    if (!playing_) return;
    if (stopping_) {
      stop();
      return;
    }
    if (!file_) {
      playing_ = false;
      return;
    }
    if (remain_ == 0) {
      finish_();
      return;
    }

    // 1チャンク読む
    uint32_t n = (remain_ > CHUNK_BYTES) ? CHUNK_BYTES : remain_;
    n = (n / wi_.blockAlign) * wi_.blockAlign;
    if (n == 0) {
      finish_();
      return;
    }

    int r = file_.read(bufs_[bi_], n);
    if (r <= 0) {
      finish_();
      return;
    }

    const size_t samples = (size_t)r / 2;  // PCM16 mono
    uint32_t durMs = (uint32_t)((1000.0 * (double)r) / (double)bytesPerSec_ + 0.5);
    if (durMs > EARLY_MS) durMs -= EARLY_MS;

    // 再生
    M5.Speaker.playRaw((const int16_t*)bufs_[bi_], samples, wi_.sampleRate, false);

    // 次へ
    remain_ -= (uint32_t)r;
    bi_ ^= 1;

    // ★ここが“ストリーミング風”の肝：このチャンク時間だけ待つ
    // ただし停止を効かせたいので、細かく刻んで戻る
    waitUntil_ = millis() + durMs;
  }

  // service() を呼ぶたびに wait を処理（ブロッキング回避）
  void pumpWait() {
    if (!playing_) return;
    if (waitUntil_ == 0) return;
    if ((int32_t)(millis() - waitUntil_) >= 0) {
      waitUntil_ = 0;
    }
  }

  bool canNext() const {
    return playing_ && (waitUntil_ == 0);
  }

private:
  void finish_() {
    playing_ = false;
    if (file_) file_.close();
  }

  File file_;
  WavInfo wi_;
  uint32_t bytesPerSec_ = 0;
  uint32_t remain_ = 0;

  bool playing_ = false;
  bool stopping_ = false;

  uint8_t buf0_[CHUNK_BYTES];
  uint8_t buf1_[CHUNK_BYTES];
  uint8_t* bufs_[2] = { buf0_, buf1_ };
  int bi_ = 0;

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
    player.stop();  // ★停止メソッド
  }

  if (!player.isPlaying() && M5.BtnA.wasPressed()) {
    if (!player.start(WAV_PATH)) {
      Serial.println("start failed");
    }
  }

  // serviceは「次を出せるときだけ」進める
  player.pumpWait();
  if (player.canNext()) {
    player.service();
  }

  delay(1);
}
