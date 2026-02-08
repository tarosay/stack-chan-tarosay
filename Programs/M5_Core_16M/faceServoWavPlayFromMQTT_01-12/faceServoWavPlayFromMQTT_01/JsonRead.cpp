#include <ArduinoJson.h>
#include <stdarg.h>
#include <SpiBusLock.hpp>

#include "JsonRead.hpp"

// グローバルインスタンス
JsonRead jsonRead;


JsonRead::JsonRead() {}

namespace {

static constexpr size_t TOKEN_MAX = 128; // 31文字は普通に超えるので現実的な長さ

// ---------- 小物： "servo/pin" を辿る ----------
// 戻り値:  1=OK, 0=END, -1=TOO_LONG
int splitTokenNext(const char*& p, char* token, size_t tokenSize) {
  while (*p == '/') ++p;
  if (*p == '\0') return 0;

  size_t i = 0;
  while (*p != '\0' && *p != '/') {
    if (i + 1 >= tokenSize) return -1; // too long
    token[i++] = *p++;
  }
  token[i] = '\0';
  return 1;
}

JsonVariantConst walkPrefix(JsonVariantConst v, const char* prefixPath) {
  if (!prefixPath || *prefixPath == '\0') return v;

  const char* p = prefixPath;
  char token[TOKEN_MAX];

  for (;;) {
    int r = splitTokenNext(p, token, sizeof(token));
    if (r == 0) break;
    if (r < 0) return JsonVariantConst();

    v = v[token];
    if (v.isNull()) return JsonVariantConst();
  }
  return v;
}

// filter側でも同じprefixのJsonObjectを作る
JsonObject ensurePrefixObject(JsonDocument& filter, const char* prefixPath) {
  JsonObject obj = filter.to<JsonObject>();  // root object
  if (!prefixPath || *prefixPath == '\0') return obj;

  const char* p = prefixPath;
  char token[TOKEN_MAX];

  for (;;) {
    int r = splitTokenNext(p, token, sizeof(token));
    if (r == 0) break;
    if (r < 0) return JsonObject();
    obj = obj.createNestedObject(token);
  }
  return obj;
}
}

bool JsonRead::loadDataMulti(const char* path, const char* prefixPath, ...) {
  if (!path) return false;

  // 1) 可変長引数をいったん取り出す（最大数は運用で調整）
  struct ItemLocal {
    const char* key;
    void* out;
    ValueType type;
  };

  static constexpr size_t MAX_ITEMS = 24;
  ItemLocal items[MAX_ITEMS];
  size_t n = 0;

  va_list ap;
  va_start(ap, prefixPath);
  while (true) {
    const char* key = va_arg(ap, const char*);
    if (key == nullptr) break;

    void* outPtr = va_arg(ap, void*);
    int typeInt = va_arg(ap, int);  // enumはintに昇格する
    ValueType vt = static_cast<ValueType>(typeInt);

    if (n < MAX_ITEMS) {
      items[n++] = { key, outPtr, vt };
    } else {
      // 多すぎたら無視（または false にしたければここでva_endしてreturn false）
    }
  }
  va_end(ap);

  // 2) Filterを作る（prefix配下の必要キーだけ true）
  JsonDocument filter;
  JsonObject fobj = ensurePrefixObject(filter, prefixPath);
  if (fobj.isNull()) return false; // prefixPathが異常（トークン長超過等）
  for (size_t i = 0; i < n; ++i) {
    fobj[items[i].key] = true;
  }

  JsonDocument doc;
  DeserializationError err;
  // 3) JSON読む
  {
    SpiGuard g;  // ★SDアクセスをロック（引継ぎ通り）

    File f = SD.open(path, FILE_READ);
    if (!f) return false;

    err = deserializeJson(doc, f, DeserializationOption::Filter(filter));
    f.close();
    if (err) return false;
  }

  // 4) prefix を辿って値を反映（キー欠落はデフォルト維持）
  JsonVariantConst base = walkPrefix(doc.as<JsonVariantConst>(), prefixPath);
  if (base.isNull()) return false;  // prefixそのものが無い

  for (size_t i = 0; i < n; ++i) {
    JsonVariantConst v = base[items[i].key];
    if (v.isNull()) continue;  // ★欠落はデフォルト維持

    switch (items[i].type) {
      case ValueType::I32:
        *reinterpret_cast<int32_t*>(items[i].out) = v.as<int32_t>();
        break;
      case ValueType::U32:
        *reinterpret_cast<uint32_t*>(items[i].out) = v.as<uint32_t>();
        break;
      case ValueType::F32:
        *reinterpret_cast<float*>(items[i].out) = v.as<float>();
        break;
      case ValueType::BOOL:
        *reinterpret_cast<bool*>(items[i].out) = v.as<bool>();
        break;
      default:
        break;
    }
  }
  return true;
}

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
