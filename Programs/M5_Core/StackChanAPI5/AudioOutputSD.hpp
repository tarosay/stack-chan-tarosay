//#pragma once
#ifndef AUDIO_OUTPUT_SD_HPP
#define AUDIO_OUTPUT_SD_HPP

#include "AudioOutput.h"
#include <SD.h>

class AudioOutputSD : public AudioOutput {
public:
  AudioOutputSD(const char* filename);
  virtual ~AudioOutputSD();

  virtual bool begin() override;
  virtual bool ConsumeSample(int16_t sample[2]) override;
  virtual void flush() override;
  virtual bool stop() override;

  size_t write(const void* data, size_t len);

private:
  fs::File file;
  const char* filename;
};
#endif  // AUDIO_OUTPUT_SD_HPP