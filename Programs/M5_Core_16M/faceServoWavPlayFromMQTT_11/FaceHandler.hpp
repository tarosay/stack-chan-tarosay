#ifndef FACEHANDLER_HPP
#define FACEHANDLER_HPP

#include <Arduino.h>
#include "FaceAvatar.hpp"

class FaceHandler {
public:
  explicit FaceHandler(FaceAvatar& fa)
    : fa_(fa) {}
  bool handle(const char* topic, uint8_t* payload, unsigned int len);

private:
  FaceAvatar& fa_;

  // topic の "face/" の直後を返す（例: ".../face/expr/happy" -> "expr/happy"）
  static const char* afterFace_(const char* topic);

  // s から 1セグメント取り出して buf に入れ、次位置を返す
  static const char* readSeg_(const char* s, char* buf, size_t bufSz);

  // expr 名 -> idx
  static bool mapExpr_(const char* name, uint8_t& idxOut);

  // payload "size ms" を読む（size: float, ms: uint32）
  static bool parseSizeMs_(uint8_t* payload, unsigned int len, float& sizeOut, uint32_t& msOut);
};
#endif