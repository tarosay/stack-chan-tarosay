#ifndef PCM16STREAMHANDLER_HPP
#define PCM16STREAMHANDLER_HPP

#include <Arduino.h>
#include <ArduinoJson.h>
#include "WavStreamPlayer.hpp"

class Pcm16StreamHandler {
public:
  explicit Pcm16StreamHandler(WavStreamPlayer& player);

  // topicは固定仕様：pcm16/+/ctrl と pcm16/+/pcm
  void handle(const char* topic, uint8_t* payload, unsigned int len);

  void stop(bool clearFifo = true);

private:
  void onCtrl_(const char* topic, uint8_t* payload, unsigned int len);
  void onPcm_(const char* topic, uint8_t* payload, unsigned int len);

  bool endsWith_(const char* s, const char* suf) const;
  bool extractSid_(const char* topic, String& sidOut) const;
  uint32_t u32le_(const uint8_t* p) const;
  uint16_t u16le_(const uint8_t* p) const;

private:
  WavStreamPlayer& player_;
  String activeSid_;
  bool streaming_ = false;
  uint32_t expectSeq_ = 0;
};

#endif