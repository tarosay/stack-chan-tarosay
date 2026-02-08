#include <M5Unified.h>
#include "VolumeHandler.hpp"

static inline uint8_t clampU8_(int v) {
  if (v < 0) return 0;
  if (v > 255) return 255;
  return (uint8_t)v;
}

void VolumeHandler::apply() {
  // Keep volume control in one place: the player.
  player_.setVolume(vol_);
}

void VolumeHandler::handle(const char* topic, uint8_t* payload, unsigned int len) {
  if (!payload || len == 0) return;

  // Copy to temporary null-terminated buffer (plain small payload expected)
  char buf[24];
  size_t m = (len < sizeof(buf) - 1) ? len : (sizeof(buf) - 1);
  memcpy(buf, payload, m);
  buf[m] = '\0';

  // Trim leading whitespace
  char* s = buf;
  while (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n') s++;

  // Plain integer only
  int v = atoi(s);
  uint8_t newVol = clampU8_(v);

  if (newVol == vol_) {
    return; // no-op
  }

  vol_ = newVol;
  player_.setVolume(vol_);

  // Event log only (serial level is WARN in this project)
  M5_LOGW("[VOL] %s -> %u", topic ? topic : "(null)", (unsigned)vol_);
}
