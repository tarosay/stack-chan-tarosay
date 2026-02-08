#ifndef WAVSTREAMPLAYER_HPP
#define WAVSTREAMPLAYER_HPP

#include <Arduino.h>
#include <M5Unified.h>
#include <SD.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

class WavStreamPlayer {
public:
  // chunkBytes: 先読み単位（8192〜16384推奨）
  // earlyMs   : 供給を少し早める（0〜5程度）
  // fadeMs    : チャンク境界のクリック除去（2ms推奨）
  //explicit WavStreamPlayer(size_t chunkBytes = 8192, uint32_t earlyMs = 2, uint32_t fadeMs = 2);
  explicit WavStreamPlayer(size_t chunkBytes = 8192, uint32_t earlyMs = 2, uint32_t fadeMs = 2, size_t fifoBytes = 65536);

  // 再生開始（PCM16 mono/stereo WAV対応）
  bool start(const char* wavPath);

  // 停止（即停止＋後始末）
  void stop();

  // クラス内で service を回すタスクを起動/停止
  //  - periodMs は 1ms 推奨（粗いと切替が遅れてクリック/欠けの原因になる）
  bool beginAsync(uint32_t stackBytes = 4096, UBaseType_t prio = 2, BaseType_t core = 0, uint32_t periodMs = 1);
  void endAsync();

  // 再生中か
  bool isPlaying() const {
    return playing_;
  }

  // loop()から頻繁に呼ぶ（再生を進める）
  // ※ beginAsync() を使う場合は外から呼ぶ必要なし（呼んでもOK）
  void service();

  // 任意：音量0..255
  void setVolume(uint8_t vol) {
    volume_ = vol;
  }

  // 任意：現在のWAV情報（デバッグ用）
  uint32_t sampleRate() const {
    return wi_.sampleRate;
  }

  // ===== PCM16ストリーム再生（MQTT等からpush）=====
  bool streamBegin(uint32_t sampleRate, uint16_t channels, uint16_t bitsPerSample,
                   size_t playChunkBytes = 4096);
  bool streamPush(const uint8_t* pcm, size_t bytes);
  void streamEnd();
  void streamClear();

private:
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

  // --- lock/unlock ---
  void ensureInternalMutex_();
  void lock_();
  void unlock_();

  // --- async task ---
  static void taskThunk_(void* arg);
  void taskLoop_();

  // --- unlocked core ---
  bool startUnlocked_(const char* wavPath);
  void stopUnlocked_();
  void serviceUnlocked_();

  // --- wav helpers ---
  bool parseWav_(File& f, WavInfo& wi);
  void speakerSetupFromWav_(const WavInfo& wi);
  size_t readChunk_(uint8_t* dst);
  void playChunk_(uint8_t* bytes, size_t len);
  uint32_t durMsFor_(size_t bytes) const;

  static bool readU16_(File& f, uint16_t& v);
  static bool readU32_(File& f, uint32_t& v);

  void fadeEdgesPcm16_(int16_t* s, size_t frames, uint32_t sampleRate, uint16_t channels);

private:
  // config
  size_t chunkBytes_;
  uint32_t earlyMs_;
  uint32_t fadeMs_;
  uint8_t volume_ = 200;

  // internal sync/task
  SemaphoreHandle_t mtx_ = nullptr;
  TaskHandle_t task_ = nullptr;
  uint32_t periodMs_ = 1;
  volatile bool taskRun_ = false;

  // state
  File file_;
  WavInfo wi_;
  uint32_t bytesPerSec_ = 0;
  uint32_t remain_ = 0;

  bool playing_ = false;
  bool stopping_ = false;

  uint8_t* buf0_ = nullptr;
  uint8_t* buf1_ = nullptr;
  bool curIsBuf0_ = true;

  size_t curLen_ = 0;
  size_t nextLen_ = 0;

  uint32_t waitUntil_ = 0;

  // --- stream fifo helpers ---
  void ensureStreamFifo_();
  size_t fifoFree_() const;
  bool fifoPush_(const uint8_t* src, size_t bytes);
  size_t fifoPop_(uint8_t* dst, size_t bytes);
  void streamServiceUnlocked_();

  // buf容量を持つ（realloc対応）
  size_t bufCap0_ = 0;
  size_t bufCap1_ = 0;

  // stream state
  bool streamMode_ = false;
  bool streamEof_ = false;
  bool streamStopArmed_ = false;
  size_t streamPlayChunkBytes_ = 4096;

  uint8_t* fifo_ = nullptr;
  size_t fifoHead_ = 0;
  size_t fifoTail_ = 0;
  size_t fifoUsed_ = 0;

  size_t fifoBytes_;  // ★configとして保持
};

#endif  // WAVSTREAMPLAYER_HPP
