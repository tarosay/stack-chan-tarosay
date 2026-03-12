#ifndef SERVOXYHANDLER_HPP
#define SERVOXYHANDLER_HPP

#include <Arduino.h>
#include "ServoXY.hpp"

// Minimal-heap MQTT text command handler for ServoXY.
//
// Topic format (subscribe): device/<deviceId>/servoxy/<cmd>
// Commands (cmd in topic tail):
//   .../move   payload: "<x> <y> <ms>"
//   .../stop   payload: ""
//   .../home   payload: "<ms>"
//   .../speed  payload: "<degps_x> <degps_y>"
//   .../pulse  payload: "<min_us_x> <max_us_x> <min_us_y> <max_us_y>"
//
// Design goals:
// - No JSON parsing (avoid dynamic allocations)
// - No payload copying (parses directly on (payload,len))

class ServoXYHandler {
public:
  explicit ServoXYHandler(ServoXY& servo)
    : servo_(servo) {}

  // Returns true if the command was recognized and applied.
  bool handle(const char* topic, uint8_t* payload, unsigned int len);

private:
  ServoXY& servo_;

  static bool isSpace_(char c);
  static bool topicTailEq_(const char* topic, const char* tail);
  static const char* skipSpaces_(const char* p, const char* end);
  static bool readInt_(const char*& p, const char* end, int32_t& out);
  static bool readUInt_(const char*& p, const char* end, uint32_t& out);
};

#endif
