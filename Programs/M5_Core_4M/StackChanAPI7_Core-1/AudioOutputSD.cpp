#include "AudioOutputSD.hpp"

AudioOutputSD::AudioOutputSD(const char *filename) {
  this->filename = filename;
}

AudioOutputSD::~AudioOutputSD() {
  stop();
}

bool AudioOutputSD::begin() {
  //if (!SD.begin()) {
  //  return false;
  //}

  file = SD.open(filename, FILE_WRITE);
  if (!file) {
    return false;
  }

  int mp3Size = file.size();
  int sampleRate = 44100;
  int numChannels = 1;
  int bitsPerSample = 16;
  int byteRate = sampleRate * numChannels * (bitsPerSample / 8);
  int blockAlign = numChannels * (bitsPerSample / 8);
  int dataSize = 25 * mp3Size;
  int riffChunkSize = 36 + dataSize;

  // RIFF ヘッダ
  file.write(reinterpret_cast<const uint8_t *>("RIFF"), 4);
  file.write((uint8_t *)&riffChunkSize, 4);
  file.write(reinterpret_cast<const uint8_t *>("WAVE"), 4);
  // fmt チャンク
  file.write(reinterpret_cast<const uint8_t *>("fmt "), 4);
  int fmtChunkSize = 16;
  file.write((uint8_t *)&fmtChunkSize, 4);
  short audioFormat = 1;  // PCM
  file.write((uint8_t *)&audioFormat, 2);
  file.write((uint8_t *)&numChannels, 2);
  file.write((uint8_t *)&sampleRate, 4);
  file.write((uint8_t *)&byteRate, 4);
  file.write((uint8_t *)&blockAlign, 2);
  file.write((uint8_t *)&bitsPerSample, 2);
  // data チャンク
  file.write(reinterpret_cast<const uint8_t *>("data"), 4);
  file.write((uint8_t *)&dataSize, 4);

  return true;
}

bool AudioOutputSD::ConsumeSample(int16_t sample[2]) {
  return write(sample, sizeof(int16_t) * 2) > 0;
}

size_t AudioOutputSD::write(const void *data, size_t len) {
  if (!file) return 0;
  return file.write((const uint8_t *)data, len);
}

void AudioOutputSD::flush() {
  if (file) {
    file.flush();
  }
}

bool AudioOutputSD::stop() {
  if (file) {
    file.flush();
    file.close();
  }
  for (int i = 0; i < 100; i++) {
    delay(10);
  }
  return true;
}
