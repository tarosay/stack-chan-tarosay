#ifndef JSONREAD_HPP
#define JSONREAD_HPP

#include <Arduino.h>
#include <M5Unified.h>
#include <SD.h>

class JsonRead {
public:
  // コンストラクタ
  JsonRead();

  enum class ValueType : uint8_t { I32,
                                   U32,
                                   F32,
                                   BOOL,
                                   STR };

  // 終端は nullptr。各項目は (key, outPtr, type) の繰り返し
  bool loadDataMulti(const char* path, const char* prefixPath, ...);

  // section[ index ][ keyA ] を outA に取り出す。
  // keyB/outB が指定されていれば section[index][keyB] も取り出す。
  // 戻り値: 1個モード=keyAが取れたらtrue / 2個モード=keyA,keyB両方取れたらtrue
  bool loadDataByIndex(const char* path, const char* section, size_t index,
                       const char* keyA, String& outA,
                       const char* keyB = nullptr, String* outB = nullptr);

  size_t countEntries(const char* path, const char* section);

private:
};

// グローバルインスタンスの宣言
extern JsonRead jsonRead;

#endif