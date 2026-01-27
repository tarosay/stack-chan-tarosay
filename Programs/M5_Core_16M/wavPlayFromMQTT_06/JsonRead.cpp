#include <ArduinoJson.h>

#include "SpiBusLock.hpp"
#include "JsonRead.hpp"

// グローバルインスタンス
JsonRead jsonRead;

JsonRead::JsonRead() {}

bool JsonRead::loadDataByIndex(const char* path, const char* section, size_t index,
                               const char* keyA, String& outA,
                               const char* keyB, String* outB) {
  SpiGuard g;

  outA = "";
  if (outB) *outB = "";

  if (!path || !section || !keyA) return false;
  const bool wantB = (keyB && keyB[0] != '\0' && outB != nullptr);

  File f = SD.open(path, FILE_READ);
  if (!f) return false;

  JsonDocument doc;
  JsonDocument filter;

  // ★重要：Filter の [0] は「配列全要素に適用」用途で使う（あなたの旧版が正しい）
  filter[section][0][keyA] = true;
  if (wantB) filter[section][0][keyB] = true;

  DeserializationError err = deserializeJson(doc, f, DeserializationOption::Filter(filter));
  f.close();
  if (err) return false;

  JsonVariant vSection = doc[section];
  if (!vSection.is<JsonArray>()) return false;

  JsonArray arr = vSection.as<JsonArray>();
  if (index >= arr.size()) return false;

  JsonVariant vItem = arr[index];
  if (!vItem.is<JsonObject>()) return false;

  JsonObject obj = vItem.as<JsonObject>();

  // ★型に依存せず String 化して取得（キーが無ければ null 判定で落とす）
  JsonVariant a = obj[keyA];
  if (a.isNull()) return false;
  outA = a.as<String>();

  if (!wantB) return true;

  JsonVariant b = obj[keyB];
  if (b.isNull()) {
    outA = "";
    *outB = "";
    return false;
  }
  *outB = b.as<String>();

  return true;
}

size_t JsonRead::countEntries(const char* path, const char* section) {
  SpiGuard g;
  if (!path || !section) return 0;

  File f = SD.open(path, FILE_READ);
  if (!f) return 0;

  JsonDocument doc;
  JsonDocument filter;
  filter[section] = true;

  DeserializationError err = deserializeJson(doc, f, DeserializationOption::Filter(filter));
  f.close();
  if (err) return 0;

  JsonVariant v = doc[section];
  if (!v.is<JsonArray>()) return 0;
  return v.as<JsonArray>().size();
}
