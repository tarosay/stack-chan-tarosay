#include "Mp3ToWav.hpp"

AudioFileSourceSD *mp3fileSD;
AudioFileSourceID3 *id3;
AudioGeneratorMP3 *mp3;
AudioOutputSD *sdOutput;

bool makeWAV(const char *mp3Path, const char *wavPath) {
  mp3fileSD = new AudioFileSourceSD(mp3Path);
  id3 = new AudioFileSourceID3(mp3fileSD);
  mp3 = new AudioGeneratorMP3();
  sdOutput = new AudioOutputSD(wavPath);
  if (!mp3->begin(id3, sdOutput)) {
    Serial.println("mp3->begin false");
    if (mp3) {
      if (mp3->isRunning())
        mp3->stop();
      delete mp3;
      mp3 = nullptr;
    }
    if (mp3fileSD) {
      delete mp3fileSD;
      mp3fileSD = nullptr;
    }
    if (id3) {
      delete id3;
      id3 = nullptr;
    }
    if (sdOutput) {
      delete sdOutput;
      sdOutput = nullptr;
    }
    return false;
  }

  while (true) {
    if (mp3 && mp3->isRunning()) {
      if (!mp3->loop()) {
        mp3->stop();
        break;
      }
    }
  }

  if (mp3) {
    if (mp3->isRunning())
      mp3->stop();
    delete mp3;
    mp3 = nullptr;
  }
  if (mp3fileSD) {
    delete mp3fileSD;
    mp3fileSD = nullptr;
  }
  if (id3) {
    delete id3;
    id3 = nullptr;
  }
  if (sdOutput) {
    delete sdOutput;
    sdOutput = nullptr;
  }
  return true;
}

bool Mp3ToWav::ConvertWav(const char *mp3Path, const char *wavPath) {
  bool convSuccess = false;
  fs::File file = SD.open(mp3Path, FILE_READ);
  if (!file) {
    return false;
  }
  int mp3Size = file.size();
  file.close();

  for (int i = 0; i < 16; i++) {
    //while (true) {
    delay(1000);
    if (!makeWAV(mp3Path, wavPath)) {
      Serial.println("WAVファイルの作成に失敗しました");
      continue;
    }

    file = SD.open(wavPath, FILE_READ);
    if (!file) {
      Serial.println("WAVファイルを開けませんでした");
      continue;
    }
    int fileSize = file.size();
    file.close();
    if (fileSize > mp3Size) {
      int riffChunkSize = fileSize - 8;
      int dataSize = fileSize - 44;
      file = SD.open(wavPath, FILE_WRITE);
      if (!file) {
        break;
      }
      file.seek(4);
      file.write((uint8_t *)&riffChunkSize, 4);
      file.seek(40);
      file.write((uint8_t *)&dataSize, 4);
      file.seek(fileSize);
      file.flush();
      file.close();
      //Serial.println(fileSize);
      convSuccess = true;
      break;
    }
    Serial.printf("WAVファイルサイズが %d バイト以下でした\r\n", mp3Size);
    for (int j = 0; j < 100; j++) {
      delay(10);
    }
  }
  return convSuccess;
}
