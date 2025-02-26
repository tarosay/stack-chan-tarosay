
#include "WavPlayer.hpp"

//#include "AudioFileSourcePROGMEM.h"
#include "AudioFileSourceSD.h"
#include "AudioGeneratorWAV.h"
#include "AudioOutputI2S.h"

AudioGeneratorWAV *wav;
AudioFileSourcePROGMEM *file = nullptr;  // 明示的にnullptrで初期化
AudioOutputI2S *out;

const uint8_t *playWavFile = nullptr;  // 明示的にnullptrで初期化
uint32_t playWavFileSize = 0;

// グローバルインスタンスの定義
WavPlayer wavPlayer;

//***********************
// コンストラクタ
// コンストラクタでメンバ変数の初期化を行います。
//***********************
WavPlayer::WavPlayer()
//: numPixels(NUMPIXELS),
//  brightness(25)
{
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

    vTaskDelay(1);  // 適度なディレイを入れてCPU負荷を軽減
  }
}


void WavPlayer::begin() {
  out = new AudioOutputI2S(0, 1);  // Output to builtInDAC
  out->SetOutputModeMono(true);
  out->SetGain(1.0);  // 初期音量を設定
  wav = new AudioGeneratorWAV();

  xTaskCreatePinnedToCore(
    wavPlayerTask,    // タスク関数
    "wavPlayerTask",  // タスク名
    2048,             // スタックサイズ（バイト単位）
    NULL,             // タスクパラメータ
    1,                // 優先度
    NULL,             // タスクハンドル
    0                 // タスクを実行するコア
  );
}

uint32_t WavPlayer::play(const uint8_t *wavFile, uint32_t fileSize) {
  if (file) {
    delete file;
    file = nullptr;
  }

  playWavFile = wavFile;
  playWavFileSize = fileSize;
  file = new AudioFileSourcePROGMEM(wavFile, fileSize);
  if (!wav->begin(file, out)) {
    //Serial.printf("Failed to restart playback\n");
    return 0;
  }
  return getPlayDuration(wavFile, fileSize);
}

uint32_t WavPlayer::getPlayDuration(const uint8_t *wavFile, uint32_t fileSize) {
  // ヘッダーの解析
  uint32_t sampleRate = *(uint32_t *)(wavFile + 24);  // サンプリング周波数
  uint16_t channels = *(uint16_t *)(wavFile + 22);    // チャンネル数
  uint16_t bitDepth = *(uint16_t *)(wavFile + 34);    // ビット深度

  // "data"チャンクを検索
  int dataChunkOffset = -1;
  uint32_t dataSize = 0;
  for (int i = 0; i < fileSize - 8; i++) {
    if (wavFile[i] == 'd' && wavFile[i + 1] == 'a' && wavFile[i + 2] == 't' && wavFile[i + 3] == 'a') {
      dataChunkOffset = i + 8;  // "data"の次の4バイトにデータサイズがある
      dataSize = *(uint32_t *)(wavFile + i + 4);
      break;
    }
  }

  // "data"チャンクが見つからない場合
  if (dataChunkOffset == -1) {
    return 0;
  }

  // 再生時間を計算（ミリ秒単位）
  uint32_t bytesPerSecond = sampleRate * channels * (bitDepth / 8);  // 1秒間に処理されるバイト数
  uint32_t duration = (dataSize * 1000) / bytesPerSecond;            // ミリ秒単位の再生時間
  return duration;
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

// void calculateWavDuration(const uint8_t *wavFile, int fileSize) {
//   // ヘッダーの解析
//   uint32_t sampleRate = *(uint32_t *)(wavFile + 24);  // サンプリング周波数
//   uint16_t channels = *(uint16_t *)(wavFile + 22);    // チャンネル数
//   uint16_t bitDepth = *(uint16_t *)(wavFile + 34);    // ビット深度

//   // "data"チャンクを検索
//   int dataChunkOffset = -1;
//   uint32_t dataSize = 0;
//   for (int i = 0; i < fileSize - 8; i++) {
//     if (wavFile[i] == 'd' && wavFile[i + 1] == 'a' && wavFile[i + 2] == 't' && wavFile[i + 3] == 'a') {
//       dataChunkOffset = i + 8;  // "data"の次の4バイトにデータサイズがある
//       dataSize = *(uint32_t *)(wavFile + i + 4);
//       break;
//     }
//   }

//   // "data"チャンクが見つからない場合
//   if (dataChunkOffset == -1) {
//     M5_LOGI("Error: 'data' chunk not found!");
//     return;
//   }

//   // 再生時間を計算
//   float duration = (float)dataSize / (sampleRate * channels * (bitDepth / 8));

//   // 結果を出力
//   M5_LOGI("Sample Rate: %d Hz", sampleRate);
//   M5_LOGI("Channels: %d", channels);
//   M5_LOGI("Bit Depth: %d bits", bitDepth);
//   M5_LOGI("Data Offset: %d bytes", dataChunkOffset);
//   M5_LOGI("Data Size: %d bytes", dataSize);
//   M5_LOGI("Duration: %.2f seconds", duration);
// }
