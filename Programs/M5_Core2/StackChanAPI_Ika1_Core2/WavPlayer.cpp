
#include "WavPlayer.hpp"

#include "AudioFileSourceSD.h"
#include "AudioGeneratorWAV.h"
#include "AudioOutputI2S.h"

AudioGeneratorWAV *wav;
AudioFileSourcePROGMEM *file = nullptr;  // 明示的にnullptrで初期化
AudioOutputI2S *out;
AudioFileSourceSD *fileSD = nullptr;

const uint8_t *playWavFile = nullptr;  // 明示的にnullptrで初期化
uint32_t playWavFileSize = 0;

// グローバルインスタンスの定義
WavPlayer wavPlayer;

//***********************
// コンストラクタ
// コンストラクタでメンバ変数の初期化を行います。
//***********************
WavPlayer::WavPlayer() {
}

void wavPlayerTask(void *pvParameters) {
  while (true) {
    if (wav->isRunning()) {
      if (!wav->loop()) {
        wav->stop();
      }
    }
    // // 現在のタスクのハイウォーターマークを取得
    // UBaseType_t highWaterMark = uxTaskGetStackHighWaterMark(NULL);
    // Serial.printf("Remaining stack: %d words\n", highWaterMark);

    vTaskDelay(5);  // 適度なディレイを入れてCPU負荷を軽減
  }
}

void WavPlayer::begin() {
  out = new AudioOutputI2S(0, 1);  // Output to builtInDAC
  out->SetOutputModeMono(true);
  out->SetGain(0.9);  // 初期音量を設定
  wav = new AudioGeneratorWAV();

  xTaskCreatePinnedToCore(
    wavPlayerTask,    // タスク関数
    "wavPlayerTask",  // タスク名
    2048,             // スタックサイズ（バイト単位）
    NULL,             // タスクパラメータ
    2,                // 優先度
    NULL,             // タスクハンドル
    0                 // タスクを実行するコア
  );
}

void WavPlayer::play(const uint8_t *wavFile, uint32_t fileSize) {
  if (file) {
    delete file;
    file = nullptr;
  }

  playWavFile = wavFile;
  playWavFileSize = fileSize;
  file = new AudioFileSourcePROGMEM(wavFile, fileSize);
  if (!wav->begin(file, out)) {
    //Serial.printf("Failed to restart playback\n");
    return;
  }
}

void WavPlayer::repeart() {
  if (wav->isRunning()) {
    return;
  }

  if (!playWavFile || playWavFileSize <= 0) {
    return;
  }

  if (file) {
    delete file;
    file = nullptr;
  }

  file = new AudioFileSourcePROGMEM(playWavFile, playWavFileSize);
  if (!wav->begin(file, out)) {
    return;
  }
}

void WavPlayer::stop() {
  wav->stop();
}

bool WavPlayer::isPlaying() {
  return wav->isRunning();
}

void WavPlayer::play(const String &wavFilename) {
  for (int i = 0; i < 8; i++) {
    if (fileSD) {
      delete fileSD;
      fileSD = nullptr;
    }

    fileSD = new AudioFileSourceSD(wavFilename.c_str());

    // 再生を開始
    if (wav->begin(fileSD, out)) {
      break;
    } else {
      Serial.printf("Failed to restart playback\n");
      delay(250);
      wav->stop();
      continue;
    }
  }
}

void WavPlayer::play(const String &wavFilename, float volume) {
  // 音量を設定 (AudioOutput 側で調整)
  if (volume < 0.0f) {
    volume = 0.0f;  // 最小音量
  }
  if (volume > 1.0f) {
    volume = 1.0f;  // 最大音量
  }
  out->SetGain(volume);
  play(wavFilename);
}