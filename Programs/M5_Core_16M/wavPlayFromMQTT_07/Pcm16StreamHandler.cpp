#include <M5Unified.h>
#include <esp_system.h>  // または #include <Arduino.h> で足りることも多い
#include "Pcm16StreamHandler.hpp"

Pcm16StreamHandler::Pcm16StreamHandler(WavStreamPlayer& player)
  : player_(player) {}

void Pcm16StreamHandler::handle(const char* topic, uint8_t* payload, unsigned int len) {
  if (endsWith_(topic, "/ctrl")) onCtrl_(topic, payload, len);
  else if (endsWith_(topic, "/pcm")) onPcm_(topic, payload, len);
}

void Pcm16StreamHandler::stop(bool clearFifo) {
  player_.streamEnd();
  if (clearFifo) player_.streamClear();
  streaming_ = false;
  activeSid_ = "";
  expectSeq_ = 0;
}

void Pcm16StreamHandler::onCtrl_(const char* topic, uint8_t* payload, unsigned int len) {
  JsonDocument doc;
  if (deserializeJson(doc, payload, len)) {
    M5_LOGI("[CTRL] JSON parse error");
    return;
  }

  const char* type = doc["type"] | "";
  if (!type[0]) return;

  if (strcmp(type, "begin") == 0) {
    const char* sid = doc["sid"] | "";
    uint32_t sr = doc["sr"] | 0;
    uint16_t ch = doc["ch"] | 0;
    uint16_t bits = doc["bits"] | 0;
    const char* codec = doc["codec"] | "";
    uint32_t seq0 = doc["seq0"] | 0;
    uint32_t chunkBytes = doc["chunkBytes"] | 4096;

    if (!sid[0] || sr == 0 || !(ch == 1 || ch == 2) || bits != 16) return;
    if (strcmp(codec, "pcm16") != 0) return;

    // ★ここ（begin直前）
    M5_LOGI("[MEM] before clear free=%u min=%u",
            (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMinFreeHeap());


    player_.streamClear();


    // ★ここ（clear直後）
    M5_LOGI("[MEM] after clear free=%u min=%u",
            (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMinFreeHeap());
    bool ok = player_.streamBegin(sr, ch, bits, chunkBytes);

    // ★ここ（begin直後）
    M5_LOGI("[MEM] after begin free=%u min=%u ok=%d",
            (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMinFreeHeap(), (int)ok);

    if (!ok) {
      M5_LOGI("[CTRL] streamBegin failed");
      streaming_ = false;
      activeSid_ = "";
      return;
    }

    activeSid_ = sid;
    streaming_ = true;
    expectSeq_ = seq0;

    M5_LOGI("[CTRL] begin sid=%s sr=%u ch=%u bits=%u chunk=%u seq0=%u",
            activeSid_.c_str(), (unsigned)sr, (unsigned)ch, (unsigned)bits,
            (unsigned)chunkBytes, (unsigned)seq0);
    return;
  }

  if (strcmp(type, "end") == 0) {
    const char* sid = doc["sid"] | "";
    if (!sid[0]) return;
    if (!streaming_ || activeSid_ != sid) return;

    M5_LOGI("[CTRL] end sid=%s lastSeq=%u chunks=%u pcmBytes=%u",
            sid,
            (unsigned)(doc["lastSeq"] | 0),
            (unsigned)(doc["chunks"] | 0),
            (unsigned)(doc["pcmBytes"] | 0));

    player_.streamEnd();
    streaming_ = false;
    activeSid_ = "";
    return;
  }
}

void Pcm16StreamHandler::onPcm_(const char* topic, uint8_t* payload, unsigned int len) {
  String sid;
  if (!extractSid_(topic, sid)) return;
  if (!streaming_ || sid != activeSid_) return;
  if (len < 8) return;

  uint32_t seq = u32le_(payload + 0);
  uint16_t payloadBytes = u16le_(payload + 4);
  uint16_t flags = u16le_(payload + 6);

  const uint8_t* pcm = payload + 8;
  size_t pcmLen = (size_t)len - 8;

  M5_LOGI("[PCM] ptr payload=%p pcm=%p", payload, payload + 8);

  if (payloadBytes != 0 && payloadBytes != pcmLen) {
    M5_LOGI("[PCM] payloadBytes mismatch hdr=%u actual=%u",
            (unsigned)payloadBytes, (unsigned)pcmLen);
  }

  if (seq != expectSeq_) {
    M5_LOGI("[PCM] seq jump got=%u expect=%u", (unsigned)seq, (unsigned)expectSeq_);
    expectSeq_ = seq;
  }
  expectSeq_++;

  if (!player_.streamPush(pcm, pcmLen)) {
    M5_LOGI("[PCM] drop (fifo full?) seq=%u len=%u", (unsigned)seq, (unsigned)pcmLen);
  }

  (void)flags;
}

bool Pcm16StreamHandler::endsWith_(const char* s, const char* suf) const {
  size_t ls = strlen(s), l = strlen(suf);
  if (ls < l) return false;
  return memcmp(s + (ls - l), suf, l) == 0;
}

bool Pcm16StreamHandler::extractSid_(const char* topic, String& sidOut) const {
  const char* pfx = "pcm16/";
  size_t lp = strlen(pfx);
  if (strncmp(topic, pfx, lp) != 0) return false;

  const char* p = topic + lp;
  const char* slash = strchr(p, '/');
  if (!slash) return false;

  sidOut = String(p).substring(0, (int)(slash - p));
  return sidOut.length() > 0;
}

uint32_t Pcm16StreamHandler::u32le_(const uint8_t* p) const {
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
uint16_t Pcm16StreamHandler::u16le_(const uint8_t* p) const {
  return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}
