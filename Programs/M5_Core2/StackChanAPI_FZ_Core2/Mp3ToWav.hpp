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
  bool ConvertWav(const char* mp3Path, const char* wavPath);

private:
};
#endif  // MP3_TO_WAV_HPP