#include "Mp3ToWav.hpp"

AudioFileSourceSD *mp3fileSD;
AudioFileSourceID3 *id3;
AudioGeneratorMP3 *mp3;
AudioOutputSD *sdOutput;


Mp3ToWav::Mp3ToWav(int numChannels, int sampleRate, int bitsPerSample)
  : numChannels(numChannels), sampleRate(sampleRate), bitsPerSample(bitsPerSample) {}

Mp3ToWav::Mp3ToWav()
  : numChannels(1), sampleRate(44100), bitsPerSample(16) {}

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
  while (true) {
    delay(1000);
    if (!makeWav(mp3Path, wavPath)) {
      Serial.println("WAVファイルの作成に失敗しました");
      continue;
    }

    file = SD.open(wavPath, FILE_READ);
    if (!file) {
      Serial.println("WAVファイルを開けませんでした");
      continue;
    }
    dataSize = file.size();
    file.close();
    if (dataSize > 50) { break; }
    Serial.println("WAVファイルサイズが50バイト以下でした");
  }
  return true;
}
