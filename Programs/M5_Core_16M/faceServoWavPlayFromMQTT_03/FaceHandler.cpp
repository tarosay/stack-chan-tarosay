#include "FaceHandler.hpp"
#include <string.h>
#include <stdlib.h>

const char* FaceHandler::afterFace_(const char* topic) {
  if (!topic) return nullptr;

  // "/" 区切りで "face" セグメントを探す
  const char* p = topic;
  while (*p) {
    // seg start
    const char* seg = p;
    while (*p && *p != '/') ++p;
    const size_t L = (size_t)(p - seg);

    if (L == 4 && strncmp(seg, "face", 4) == 0) {
      if (*p == '/') return p + 1;  // "face/" の直後
      return p;                     // "face" で終わり
    }
    if (*p == '/') ++p;  // skip '/'
  }
  return nullptr;
}

const char* FaceHandler::readSeg_(const char* s, char* buf, size_t bufSz) {
  if (!s || !*s) {
    if (bufSz) buf[0] = 0;
    return s;
  }
  size_t i = 0;
  while (*s && *s != '/') {
    if (i + 1 < bufSz) buf[i++] = *s;
    ++s;
  }
  if (bufSz) buf[i] = 0;
  if (*s == '/') ++s;
  return s;
}

bool FaceHandler::mapExpr_(const char* name, uint8_t& idxOut) {
  if (!name) return false;
  if (strcmp(name, "angry") == 0) {
    idxOut = 0;
    return true;
  }
  if (strcmp(name, "sleepy") == 0) {
    idxOut = 1;
    return true;
  }
  if (strcmp(name, "happy") == 0) {
    idxOut = 2;
    return true;
  }
  if (strcmp(name, "sad") == 0) {
    idxOut = 3;
    return true;
  }
  if (strcmp(name, "doubt") == 0) {
    idxOut = 4;
    return true;
  }
  if (strcmp(name, "neutral") == 0) {
    idxOut = 5;
    return true;
  }
  return false;
}

bool FaceHandler::parseSizeMs_(uint8_t* payload, unsigned int len, float& sizeOut, uint32_t& msOut) {
  // payload は null終端ではないのでスタックにコピー
  char buf[48];
  unsigned int m = (len >= sizeof(buf)) ? (sizeof(buf) - 1) : len;
  if (m > 0 && payload) memcpy(buf, payload, m);
  buf[m] = '\0';

  char* endp = nullptr;
  float size = strtof(buf, &endp);
  if (!endp) return false;

  while (*endp == ' ') ++endp;
  uint32_t ms = (*endp) ? (uint32_t)strtoul(endp, nullptr, 10) : 200;

  // size を 0..1 に丸め（ここは好みなら外してOK）
  if (size < 0.0f) size = 0.0f;
  if (size > 1.0f) size = 1.0f;

  sizeOut = size;
  msOut = ms;
  return true;
}

bool FaceHandler::handle(const char* topic, uint8_t* payload, unsigned int len) {
  const char* s = afterFace_(topic);
  if (!s || !*s) return false;

  char a[16], b[16];
  s = readSeg_(s, a, sizeof(a));  // a = first under face
  s = readSeg_(s, b, sizeof(b));  // b = second (if any)

  // face/expr/<name>
  if (strcmp(a, "expr") == 0) {
    uint8_t idx;
    if (mapExpr_(b, idx)) {
      // ここはあなたの設計方針に合わせて post 化している前提
      fa_.postExpression(idx);
    }
    return true;
  }

  // face/speech/start   payload: "size ms"
  // face/speech/stop
  if (strcmp(a, "speech") == 0) {
    if (strcmp(b, "stop") == 0) {
      fa_.postSpeechStop();
      return true;
    }
    if (strcmp(b, "start") == 0) {
      float size = 0.6f;
      uint32_t ms = 200;
      (void)parseSizeMs_(payload, len, size, ms);  // payload無くてもデフォルトで開始
      fa_.postSpeechStart(size, ms);
      return true;
    }
    return true;
  }

  return true;  // unknown は無視
}
