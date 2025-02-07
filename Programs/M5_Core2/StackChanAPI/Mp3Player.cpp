#include "Mp3Player.hpp"

#include <AudioGeneratorMP3.h>
#include <AudioOutputI2S.h>
#include <SD.h>

// WavPlayer クラスと同様、グローバル変数で再生オブジェクトを管理します
AudioGeneratorMP3 *mp3 = nullptr;
AudioFileSourcePROGMEM *fileMem = nullptr;  // PROGMEM 上の MP3 データ用
AudioFileSourceSD *mp3fileSD = nullptr;     // SD カード上の MP3 ファイル用
AudioFileSourceID3 *id3 = nullptr;          // ID3 タグ読み出し用ラッパー
AudioOutputI2S *mp3out = nullptr;
const uint8_t *playMp3File = nullptr;  // repeat 用：直前に再生したバイナリの先頭ポインタ
uint32_t playMp3FileSize = 0;          // repeat 用：直前 MP3 データのサイズ

// グローバルインスタンスの定義
Mp3Player mp3Player;

//------------------------------------------------------
// タスク関数：再生中の mp3->loop() を定期的に呼び出す
//------------------------------------------------------
void mp3PlayerTask(void *pvParameters) {
  while (true) {
    if (mp3 && mp3->isRunning()) {
      if (!mp3->loop()) {
        mp3->stop();
      }
    }
    vTaskDelay(20);  // CPU負荷軽減のためのディレイ
    // または、タスクの残り処理がある場合は yield を挟む
    taskYIELD();
  }
}

//------------------------------------------------------
// コンストラクタ
//------------------------------------------------------
Mp3Player::Mp3Player() {
  // 初期化は begin() 内で行います
}

//------------------------------------------------------
// begin(): オーディオ出力の初期化とタスク生成
//------------------------------------------------------
void Mp3Player::begin() {
  mp3out = new AudioOutputI2S(0, 1);  // builtInDAC を使用
  mp3out->SetOutputModeMono(true);
  mp3out->SetGain(0.9);  // 初期音量を設定

  mp3 = new AudioGeneratorMP3();

  xTaskCreatePinnedToCore(
    mp3PlayerTask,    // タスク関数
    "mp3PlayerTask",  // タスク名
    2048,             // スタックサイズ（バイト単位）
    NULL,             // タスクパラメータ
    5,                // 優先度
    NULL,             // タスクハンドル
    0                 // 実行するコア
  );
}

//------------------------------------------------------
// play(const uint8_t*, uint32_t)
// PROGMEM 上の MP3 バイナリデータを再生
//------------------------------------------------------
void Mp3Player::play(const uint8_t *mp3File, uint32_t fileSize) {
  // 既に再生中なら停止・クリーンアップ
  if (fileMem) {
    delete fileMem;
    fileMem = nullptr;
  }
  if (id3) {
    delete id3;
    id3 = nullptr;
  }

  playMp3File = mp3File;
  playMp3FileSize = fileSize;
  fileMem = new AudioFileSourcePROGMEM(mp3File, fileSize);
  id3 = new AudioFileSourceID3(fileMem);
  if (!mp3->begin(id3, mp3out)) {
    // 再生開始に失敗
    return;
  }
}

//------------------------------------------------------
// play(const String &filename)
// SD カード上の MP3 ファイルを再生
//------------------------------------------------------
void Mp3Player::play(const String &mp3Filename) {
  // 既存の再生を停止・クリーンアップ
  if (mp3 && mp3->isRunning()) {
    mp3->stop();
  }
  if (mp3fileSD) {
    delete mp3fileSD;
    mp3fileSD = nullptr;
  }
  if (id3) {
    delete id3;
    id3 = nullptr;
  }

  mp3fileSD = new AudioFileSourceSD(mp3Filename.c_str());
  id3 = new AudioFileSourceID3(mp3fileSD);
  if (!mp3->begin(id3, mp3out)) {
    return;
  }
}

//------------------------------------------------------
// play(const String &filename, float volume)
// SD カード上のファイルを指定の音量で再生
//------------------------------------------------------
void Mp3Player::play(const String &mp3Filename, float volume) {
  // 音量は 0.0 ～ 1.0 に丸める
  if (volume < 0.0f) volume = 0.0f;
  if (volume > 1.0f) volume = 1.0f;
  mp3out->SetGain(volume);
  play(mp3Filename);
}

//------------------------------------------------------
// repeart(): repeat 再生
// 直前に再生した PROGMEM 上の MP3 データを再度再生します
//------------------------------------------------------
void Mp3Player::repeart() {
  if (mp3->isRunning()) {
    return;
  }
  if (!playMp3File || playMp3FileSize == 0) {
    return;
  }

  if (fileMem) {
    delete fileMem;
    fileMem = nullptr;
  }
  fileMem = new AudioFileSourcePROGMEM(playMp3File, playMp3FileSize);
  if (id3) {
    delete id3;
    id3 = nullptr;
  }
  id3 = new AudioFileSourceID3(fileMem);
  mp3->begin(id3, mp3out);
}

//------------------------------------------------------
// stop() / isPlaying()
//------------------------------------------------------
void Mp3Player::stop() {
  mp3->stop();
}

bool Mp3Player::isPlaying() {
  return mp3->isRunning();
}
