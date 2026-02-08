#include "ServoXYHandler.hpp"

#include <string.h>

bool ServoXYHandler::isSpace_(char c) {
  return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

const char* ServoXYHandler::skipSpaces_(const char* p, const char* end) {
  while (p < end && isSpace_(*p)) ++p;
  return p;
}


bool ServoXYHandler::topicTailEq_(const char* topic, const char* tail) {
  if (!topic || !tail) return false;
  // compare last path segment (after last '/')
  const char* last = topic;
  for (const char* p = topic; *p; ++p) {
    if (*p == '/') last = p + 1;
  }
  const size_t tlen = strlen(tail);
  return (strncmp(last, tail, tlen) == 0) && (last[tlen] == '\0');
}

bool ServoXYHandler::readInt_(const char*& p, const char* end, int32_t& out) {
  p = skipSpaces_(p, end);
  if (p >= end) return false;

  bool neg = false;
  if (*p == '+') {
    ++p;
  } else if (*p == '-') {
    neg = true;
    ++p;
  }

  if (p >= end || *p < '0' || *p > '9') return false;
  int32_t v = 0;
  while (p < end && *p >= '0' && *p <= '9') {
    v = (v * 10) + (*p - '0');
    ++p;
  }
  out = neg ? -v : v;
  return true;
}

bool ServoXYHandler::readUInt_(const char*& p, const char* end, uint32_t& out) {
  p = skipSpaces_(p, end);
  if (p >= end) return false;
  if (*p == '+') ++p;
  if (p >= end || *p < '0' || *p > '9') return false;
  uint32_t v = 0;
  while (p < end && *p >= '0' && *p <= '9') {
    v = (v * 10u) + (uint32_t)(*p - '0');
    ++p;
  }
  out = v;
  return true;
}

bool ServoXYHandler::handle(const char* topic, uint8_t* payload, unsigned int len) {
  const char* p = (const char*)payload;
  const char* end = p + (payload ? len : 0);

  // Command is the last path segment of topic.
  if (topicTailEq_(topic, "move")) {
    int32_t x = 0, y = 0;
    uint32_t ms = 0;
    if (!readInt_(p, end, x)) return true;
    if (!readInt_(p, end, y)) return true;
    if (!readUInt_(p, end, ms)) return true;
    servo_.moveBegin((int)x, (int)y, ms);
    return true;
  }

  if (topicTailEq_(topic, "stop")) {
    const auto c = servo_.cur();
    servo_.moveBegin(c.x, c.y, 0);
    return true;
  }

  if (topicTailEq_(topic, "home")) {
    uint32_t ms = 0;
    (void)readUInt_(p, end, ms);  // missing -> 0
    servo_.moveBegin(0, 0, ms);
    return true;
  }

  if (topicTailEq_(topic, "speed")) {
    uint32_t sx = 0, sy = 0;
    if (!readUInt_(p, end, sx)) return true;
    if (!readUInt_(p, end, sy)) return true;
    servo_.setSpeedLimit(sx, sy);
    return true;
  }

  if (topicTailEq_(topic, "pulse")) {
    int32_t minX = 0, maxX = 0, minY = 0, maxY = 0;
    if (!readInt_(p, end, minX)) return true;
    if (!readInt_(p, end, maxX)) return true;
    if (!readInt_(p, end, minY)) return true;
    if (!readInt_(p, end, maxY)) return true;
    servo_.setPulseRangeX((int)minX, (int)maxX);
    servo_.setPulseRangeY((int)minY, (int)maxY);
    return true;
  }

  return false;
}
