#ifndef VOLUMEHANDLER_HPP
#define VOLUMEHANDLER_HPP

#include <Arduino.h>
#include "WavStreamPlayer.hpp"

// Volume control handler for MQTT topic: device/<deviceId>/audio/vol
// Payload: plain integer "0..255" (whitespace allowed)
class VolumeHandler {
public:
  explicit VolumeHandler(WavStreamPlayer& player, uint8_t initialVol = 180)
    : player_(player), vol_(initialVol) {}

  void apply();  // apply current vol_ to player

  void handle(const char* topic, uint8_t* payload, unsigned int len);

  uint8_t volume() const { return vol_; }

private:
  WavStreamPlayer& player_;
  uint8_t vol_;
};

#endif
