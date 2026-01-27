#include "SdCard.hpp"

// グローバルインスタンスの定義
SdCard sdCard;

// コンストラクタ
SdCard::SdCard() {}

bool SdCard::begin(uint8_t ssPin, SPIClass& spi, uint32_t frequency, const char* mountpoint, uint8_t max_files, bool format_if_empty) {
  return SD.begin(ssPin, spi, frequency, mountpoint, max_files, format_if_empty);
}

bool SdCard::extractQuotedOrBareValue(const String& line, String& out) {
  int colon = line.indexOf(':');
  if (colon < 0) return false;

  String v = line.substring(colon + 1);
  v.trim();
  if (v.length() == 0) return false;

  // 行末コメント除去（簡易）
  int hash = v.indexOf('#');
  if (hash >= 0) {
    v = v.substring(0, hash);
    v.trim();
  }

  // "..." / '...'
  if (v.startsWith("\"")) {
    int end = v.indexOf('"', 1);
    if (end > 1) {
      out = v.substring(1, end);
      return true;
    }
  } else if (v.startsWith("'")) {
    int end = v.indexOf('\'', 1);
    if (end > 1) {
      out = v.substring(1, end);
      return true;
    }
  }

  // bare
  out = v;
  return true;
}

// wifi: セクション内の "- " の個数を数える（=エントリ数）
size_t SdCard::countWiFiEntriesInSD(const char* path) {
  File f = SD.open(path, FILE_READ);
  if (!f) return 0;

  bool inWifi = false;
  size_t count = 0;

  while (f.available()) {
    String line = f.readStringUntil('\n');
    line.trim();
    if (line.length() == 0) continue;
    if (line.startsWith("#")) continue;

    if (!inWifi) {
      if (line == "wifi:" || line.startsWith("wifi:")) inWifi = true;
      continue;
    }

    // 次のトップレベルキーへ移ったら終了（簡易）
    if (line.indexOf(':') > 0 && line[0] != '-' && line[0] != ' ' && line[0] != '\t'
        && !line.startsWith("ssid:") && !line.startsWith("password:")) {
      break;
    }

    if (line.startsWith("-")) count++;
  }

  f.close();
  return count;
}

// index(0始まり) で指定して 1件だけ取り出す
bool SdCard::loadWiFiByIndexFromSD(size_t index, String& ssid, String& password, const char* path) {
  ssid = "";
  password = "";

  File f = SD.open(path, FILE_READ);
  if (!f) return false;

  bool inWifi = false;
  bool inItem = false;
  size_t curIndex = 0;
  String curSsid, curPass;

  auto commit_if_target = [&]() -> bool {
    if (inItem && curIndex == index && curSsid.length() && curPass.length()) {
      ssid = curSsid;
      password = curPass;
      return true;
    }
    return false;
  };

  while (f.available()) {
    String line = f.readStringUntil('\n');
    line.trim();
    if (line.length() == 0) continue;
    if (line.startsWith("#")) continue;

    if (!inWifi) {
      if (line == "wifi:" || line.startsWith("wifi:")) inWifi = true;
      continue;
    }

    // wifiセクション終了（簡易）
    if (line.indexOf(':') > 0 && line[0] != '-' && line[0] != ' ' && line[0] != '\t'
        && !line.startsWith("ssid:") && !line.startsWith("password:")) {
      break;
    }

    // 新しい要素開始
    if (line.startsWith("-")) {
      if (commit_if_target()) {
        f.close();
        return true;
      }

      if (!inItem) {
        inItem = true;
        curIndex = 0;
      } else {
        curIndex++;
      }
      curSsid = "";
      curPass = "";

      // "- ssid: ..." 形式にも対応
      String rest = line.substring(1);
      rest.trim();
      if (rest.startsWith("ssid:")) extractQuotedOrBareValue(rest, curSsid);
      else if (rest.startsWith("password:")) extractQuotedOrBareValue(rest, curPass);
      continue;
    }

    if (!inItem) continue;  // "- ..." が来るまで無視

    if (line.startsWith("ssid:")) {
      extractQuotedOrBareValue(line, curSsid);
    } else if (line.startsWith("password:")) {
      extractQuotedOrBareValue(line, curPass);
    }

    // 目的のindexで揃ったら即返す
    if (curIndex == index && curSsid.length() && curPass.length()) {
      ssid = curSsid;
      password = curPass;
      f.close();
      return true;
    }
  }

  // EOFで最後の要素をチェック
  bool ok = commit_if_target();
  f.close();
  return ok;
}
