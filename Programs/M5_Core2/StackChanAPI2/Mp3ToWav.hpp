//#pragma once
#ifndef MP3_TO_WAV_HPP
#define MP3_TO_WAV_HPP

#include <AudioFileSourceSD.h>
#include <AudioFileSourceID3.h>
#include <AudioGeneratorMP3.h>
#include <SD.h>
#include "AudioOutputSD.hpp"

class Mp3ToWav {
public:
  Mp3ToWav();
  Mp3ToWav(int numChannels, int sampleRate, int bitsPerSample);

  bool ConvertWav(const char* mp3Path, const char* wavPath);

private:
  int numChannels;
  int sampleRate;
  int bitsPerSample;
  int ToWav(const char* pcmPath, const char* wavPath);
};
#endif  // MP3_TO_WAV_HPP