#include <M5Unified.h>
#include <esp_system.h>  // または #include <Arduino.h> で足りることも多い
#include "Pcm16StreamHandler.hpp"
#include "FaceAvatar.hpp"

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
  StaticJsonDocument<512> doc;
  if (deserializeJson(doc, payload, len)) {
    M5_LOGW("[CTRL] JSON parse error");
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
    float mouth = doc["mouth"] | 0.6f;      // 0..1
    uint32_t pakuMs = doc["pakuMs"] | 200;  // ms/周期

    if (!sid[0] || sr == 0 || !(ch == 1 || ch == 2) || bits != 16) return;
    if (strcmp(codec, "pcm16") != 0) return;

    player_.streamClear();

    // ★ここ（clear直後）
    bool ok = player_.streamBegin(sr, ch, bits, chunkBytes);

    // ★ここ（begin直後）

    if (!ok) {
      M5_LOGE("[CTRL] streamBegin failed");
      streaming_ = false;
      activeSid_ = "";
      return;
    }

    // ★ここ（begin直後）に追加：口パク開始（実描画は faceAvatar.update が反映）
    faceAvatar.postSpeechStart(mouth, pakuMs);

    activeSid_ = sid;
    streaming_ = true;
    expectSeq_ = seq0;

    return;
  }

  if (strcmp(type, "end") == 0) {
    const char* sid = doc["sid"] | "";
    if (!sid[0]) return;
    if (!streaming_ || activeSid_ != sid) return;

    // ★追加：口パク停止
    faceAvatar.postSpeechStop();

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


  if (payloadBytes != 0 && payloadBytes != pcmLen) {
    M5_LOGW("[PCM] payloadBytes mismatch hdr=%u actual=%u",
            (unsigned)payloadBytes, (unsigned)pcmLen);
  }

  if (seq != expectSeq_) {
    M5_LOGW("[PCM] seq jump got=%u expect=%u", (unsigned)seq, (unsigned)expectSeq_);
    expectSeq_ = seq;
  }
  expectSeq_++;

  if (!player_.streamPush(pcm, pcmLen)) {
    // Rate-limit drop logs to avoid flooding / timing issues.
    static uint32_t dropCount = 0;
    dropCount++;
    if (dropCount <= 3 || (dropCount % 100) == 0) {
      M5_LOGW("[PCM] drop (fifo full?) seq=%u len=%u cnt=%u",
              (unsigned)seq, (unsigned)pcmLen, (unsigned)dropCount);
    }
  }

  (void)flags;
}

bool Pcm16StreamHandler::endsWith_(const char* s, const char* suf) const {
  size_t ls = strlen(s), l = strlen(suf);
  if (ls < l) return false;
  return memcmp(s + (ls - l), suf, l) == 0;
}

bool Pcm16StreamHandler::extractSid_(const char* topic, String& sidOut) const {
  const char* pfx = "device/";
  size_t lp = strlen(pfx);
  if (strncmp(topic, pfx, lp) != 0) return false;

  const char* p = topic + lp;  // <deviceId>/pcm16/<sid>/...
  const char* slash1 = strchr(p, '/');
  if (!slash1) return false;
  if (slash1 == p) return false;  // deviceId が空

  const char* p2 = slash1 + 1;  // pcm16/<sid>/...
  const char* mid = "pcm16/";
  size_t lm = strlen(mid);
  if (strncmp(p2, mid, lm) != 0) return false;

  const char* p3 = p2 + lm;  // <sid>/...
  const char* slash2 = strchr(p3, '/');
  if (!slash2) return false;
  if (slash2 == p3) return false;  // sid が空

  sidOut = String(p3).substring(0, (int)(slash2 - p3));
  return sidOut.length() > 0;
}

uint32_t Pcm16StreamHandler::u32le_(const uint8_t* p) const {
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
uint16_t Pcm16StreamHandler::u16le_(const uint8_t* p) const {
  return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}
