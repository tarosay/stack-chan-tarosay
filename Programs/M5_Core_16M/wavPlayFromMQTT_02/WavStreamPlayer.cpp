#include "WavStreamPlayer.hpp"
#include "SpiBusLock.hpp"

//コンストラクタ
WavStreamPlayer::WavStreamPlayer(size_t chunkBytes, uint32_t earlyMs, uint32_t fadeMs)
  : chunkBytes_(chunkBytes), earlyMs_(earlyMs), fadeMs_(fadeMs) {}
void WavStreamPlayer::ensureInternalMutex_() {
  if (mtx_ == nullptr) {
    mtx_ = xSemaphoreCreateMutex();
  }
}

void WavStreamPlayer::lock_() {
  if (mtx_) xSemaphoreTake(mtx_, portMAX_DELAY);
}

void WavStreamPlayer::unlock_() {
  if (mtx_) xSemaphoreGive(mtx_);
}

bool WavStreamPlayer::beginAsync(uint32_t stackBytes, UBaseType_t prio, BaseType_t core, uint32_t periodMs) {
  ensureInternalMutex_();
  lock_();
  if (task_ != nullptr) {  // already running
    unlock_();
    return true;
  }
  periodMs_ = (periodMs == 0) ? 1 : periodMs;
  taskRun_ = true;
  unlock_();

  // stack size in words (FreeRTOS)
  uint32_t stackWords = stackBytes / sizeof(StackType_t);
  if (stackWords < 2048 / sizeof(StackType_t)) stackWords = 2048 / sizeof(StackType_t);

  TaskHandle_t h = nullptr;
  BaseType_t ok = xTaskCreatePinnedToCore(
    taskThunk_,
    "WavStreamSvc",
    stackWords,
    this,
    prio,
    &h,
    core);
  if (ok != pdPASS) {
    lock_();
    taskRun_ = false;
    task_ = nullptr;
    unlock_();
    return false;
  }

  lock_();
  task_ = h;
  unlock_();
  return true;
}

void WavStreamPlayer::endAsync() {
  ensureInternalMutex_();
  lock_();
  if (task_ == nullptr) {
    unlock_();
    return;
  }
  taskRun_ = false;
  unlock_();

  // taskLoop_() が自分で task_ を nullptr にして終了するのを待つ
  for (int i = 0; i < 200; ++i) {  // ~200ms max
    lock_();
    bool done = (task_ == nullptr);
    unlock_();
    if (done) break;
    vTaskDelay(pdMS_TO_TICKS(1));
  }
}

void WavStreamPlayer::taskThunk_(void* arg) {
  static_cast<WavStreamPlayer*>(arg)->taskLoop_();
  vTaskDelete(nullptr);
}

void WavStreamPlayer::taskLoop_() {
  while (taskRun_) {
    ensureInternalMutex_();
    lock_();
    serviceUnlocked_();
    unlock_();
    vTaskDelay(pdMS_TO_TICKS(periodMs_));
  }

  // 終了通知
  lock_();
  task_ = nullptr;
  unlock_();
}

bool WavStreamPlayer::readU16_(File& f, uint16_t& v) {
  uint8_t b[2];
  if (f.read(b, 2) != 2) return false;
  v = (uint16_t)(b[0] | (b[1] << 8));
  return true;
}

bool WavStreamPlayer::readU32_(File& f, uint32_t& v) {
  uint8_t b[4];
  if (f.read(b, 4) != 4) return false;
  v = (uint32_t)(b[0] | (b[1] << 8) | (b[2] << 16) | (b[3] << 24));
  return true;
}

bool WavStreamPlayer::parseWav_(File& f, WavInfo& wi) {
  SpiGuard g;  // ★復活：WAVヘッダ解析中のSD(SPI)アクセスを包括保護

  if (!f.seek(0)) return false;
  char riff[4], wave[4];
  uint32_t riffSize = 0;

  if (f.read((uint8_t*)riff, 4) != 4) return false;
  if (memcmp(riff, "RIFF", 4) != 0) return false;
  if (!readU32_(f, riffSize)) return false;
  if (f.read((uint8_t*)wave, 4) != 4) return false;
  if (memcmp(wave, "WAVE", 4) != 0) return false;

  bool gotFmt = false, gotData = false;

  while (f.available()) {
    char id[4];
    uint32_t size = 0;
    if (f.read((uint8_t*)id, 4) != 4) break;
    if (!readU32_(f, size)) break;
    uint32_t chunkStart = f.position();

    if (memcmp(id, "fmt ", 4) == 0) {
      if (size < 16) return false;

      uint16_t af, ch, ba, bps;
      uint32_t sr, br;

      if (!readU16_(f, af)) return false;
      if (!readU16_(f, ch)) return false;
      if (!readU32_(f, sr)) return false;
      if (!readU32_(f, br)) return false;
      if (!readU16_(f, ba)) return false;
      if (!readU16_(f, bps)) return false;

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
      // fmt がまだなら続行（変則WAV耐性）
      if (gotFmt) break;
    }

    uint32_t next = chunkStart + size + (size & 1);
    if (!f.seek(next)) break;
  }

  return gotFmt && gotData;
}

void WavStreamPlayer::speakerSetupFromWav_(const WavInfo& wi) {
  auto spk = M5.Speaker.config();
  spk.sample_rate = wi.sampleRate;
  spk.stereo = (wi.channels == 2);
  M5.Speaker.config(spk);
  // M5.Speaker.begin(); // setup()で1回だけ
  M5.Speaker.setVolume(volume_);
}

uint32_t WavStreamPlayer::durMsFor_(size_t bytes) const {
  // 2バッファで切替えるので、早回し(earlyMs_)で前倒しすると「再生中のバッファを上書き」する危険がある。
  // 特に44.1kHz stereo では冒頭が崩れやすいので、ここでは前倒しをしない。
  return (uint32_t)((1000.0 * (double)bytes) / (double)bytesPerSec_ + 0.5);
}


void WavStreamPlayer::fadeEdgesPcm16_(int16_t* s, size_t frames, uint32_t sampleRate, uint16_t channels) {
  if (fadeMs_ == 0 || frames < 32 || channels == 0) return;

  size_t fadeN = (size_t)((sampleRate * fadeMs_) / 1000);
  if (fadeN < 8) fadeN = 8;

  // 高サンプルレート/短チャンクでゲート感が出ないように上限をかける（必要なら 16/32 を調整）
  const size_t kMaxFadeFrames = 32;
  if (fadeN > kMaxFadeFrames) fadeN = kMaxFadeFrames;

  if (fadeN * 2 > frames) fadeN = frames / 2;
  if (fadeN == 0) return;

  // 先頭
  for (size_t i = 0; i < fadeN; i++) {
    const float g = (float)i / (float)fadeN;
    const size_t base = i * channels;
    for (uint16_t c = 0; c < channels; c++) {
      s[base + c] = (int16_t)((float)s[base + c] * g);
    }
  }
  // 末尾
  for (size_t i = 0; i < fadeN; i++) {
    const float g = (float)(fadeN - i) / (float)fadeN;
    const size_t base = (frames - fadeN + i) * channels;
    for (uint16_t c = 0; c < channels; c++) {
      s[base + c] = (int16_t)((float)s[base + c] * g);
    }
  }
}

size_t WavStreamPlayer::readChunk_(uint8_t* dst) {
  if (!file_ || remain_ == 0) return 0;

  uint32_t n = (remain_ > chunkBytes_) ? (uint32_t)chunkBytes_ : remain_;
  n = (n / wi_.blockAlign) * wi_.blockAlign;
  if (n == 0) return 0;

  int r;
  {
    SpiGuard g;  // ★SDアクセス区間だけ
    r = file_.read(dst, n);
  }
  if (r <= 0) return 0;

  remain_ -= (uint32_t)r;
  return (size_t)r;
}

void WavStreamPlayer::playChunk_(uint8_t* bytes, size_t len) {
  // PCM16 mono/stereo
  const uint16_t ch = wi_.channels;              // 1 or 2
  const size_t totalSamples = len / 2;           // int16_t 要素数（全ch合計）
  const size_t frames = (ch ? (totalSamples / ch) : 0);
  auto* pcm = (int16_t*)bytes;

  fadeEdgesPcm16_(pcm, frames, wi_.sampleRate, ch);

  const bool stereo = (ch == 2);
  M5.Speaker.playRaw(pcm, totalSamples, wi_.sampleRate, stereo);
}

bool WavStreamPlayer::start(const char* wavPath) {
  ensureInternalMutex_();
  lock_();
  bool ok = startUnlocked_(wavPath);
  unlock_();
  return ok;
}

bool WavStreamPlayer::startUnlocked_(const char* wavPath) {
  stopUnlocked_();

  // バッファ確保（初回のみ）
  if (!buf0_) buf0_ = (uint8_t*)malloc(chunkBytes_);
  if (!buf1_) buf1_ = (uint8_t*)malloc(chunkBytes_);
  if (!buf0_ || !buf1_) {
    stopUnlocked_();
    return false;
  }

  {
    SpiGuard g;  // SD.open はSPIアクセス
    file_ = SD.open(wavPath, FILE_READ);
  }
  if (!file_) return false;

  if (!parseWav_(file_, wi_)) {
    stopUnlocked_();
    return false;
  }

  // PCM16 mono/stereo のみ
  if (!(wi_.audioFormat == 1 && (wi_.channels == 1 || wi_.channels == 2) && wi_.bitsPerSample == 16)) {
    stopUnlocked_();
    return false;
  }

  bool ok_seek = false;
  {
    SpiGuard g;  // seek もSPIアクセス
    ok_seek = file_.seek(wi_.dataOffset);
  }
  if (!ok_seek) {
    stopUnlocked_();
    return false;
  }

  speakerSetupFromWav_(wi_);
  bytesPerSec_ = wi_.sampleRate * wi_.blockAlign;
  remain_ = wi_.dataSize;

  curLen_ = readChunk_(buf0_);
  nextLen_ = readChunk_(buf1_);
  if (curLen_ == 0) {
    stopUnlocked_();
    return false;
  }

  playChunk_(buf0_, curLen_);
  waitUntil_ = millis() + durMsFor_(curLen_);

  playing_ = true;
  stopping_ = false;
  curIsBuf0_ = true;
  return true;
}


void WavStreamPlayer::stop() {
  ensureInternalMutex_();
  lock_();
  stopUnlocked_();
  unlock_();
}

void WavStreamPlayer::stopUnlocked_() {
  stopping_ = true;
  playing_ = false;
  waitUntil_ = 0;
  curLen_ = nextLen_ = 0;
  remain_ = 0;

  if (file_) {
    SpiGuard g;  // close もSPIアクセス
    file_.close();
  }
  M5.Speaker.stop();

  // buf0_/buf1_ は保持（繰り返し再生でmalloc/freeしない）
  stopping_ = false;
}


void WavStreamPlayer::service() {
  ensureInternalMutex_();
  lock_();
  serviceUnlocked_();
  unlock_();
}

void WavStreamPlayer::serviceUnlocked_() {
  if (!playing_) return;

  // 次チャンクへ切替タイミング
  if (waitUntil_ != 0 && (int32_t)(millis() - waitUntil_) >= 0) {
    if (nextLen_ == 0) {
      stopUnlocked_();
      return;
    }

    if (curIsBuf0_) {
      playChunk_(buf1_, nextLen_);
    } else {
      playChunk_(buf0_, nextLen_);
    }
    waitUntil_ = millis() + durMsFor_(nextLen_);

    // 再生中に次の次を読む
    if (curIsBuf0_) {
      curIsBuf0_ = false;  // 今buf1再生中
      curLen_ = nextLen_;
      nextLen_ = readChunk_(buf0_);
    } else {
      curIsBuf0_ = true;  // 今buf0再生中
      curLen_ = nextLen_;
      nextLen_ = readChunk_(buf1_);
    }
  }
}
