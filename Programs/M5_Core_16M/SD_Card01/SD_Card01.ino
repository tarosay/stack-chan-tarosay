// SD_Card_WiFiList.ino
// M5Stack Core (初期M5Stack) + M5Unified + SD.h
// SD root: /SC_SecConfig.yaml
//
// 推奨YAML形式（wifi配列）:
// wifi:
//   - ssid: "higashi-2f"
//     password: "hama1297noiti"
//   - ssid: "office"
//     password: "xxxx"

#include <Arduino.h>
#include <M5Unified.h>
#include <SPI.h>
#include <SD.h>
// ---------------------- util ----------------------
static bool extractQuotedOrBareValue(const String& line, String& out) {
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
static size_t countWiFiEntriesInSD(const char* path = "/SC_SecConfig.yaml") {
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
static bool loadWiFiByIndexFromSD(size_t index, String& ssid, String& password,
                                  const char* path = "/SC_SecConfig.yaml") {
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

// number(1始まり) で指定して取り出す
static bool loadWiFiByNumberFromSD(size_t number1based, String& ssid, String& password,
                                   const char* path = "/SC_SecConfig.yaml") {
  if (number1based == 0) return false;
  return loadWiFiByIndexFromSD(number1based - 1, ssid, password, path);
}

void setup() {
  Serial.begin(115200);
  delay(500);
  M5.begin();
  M5.Log.setLogLevel(m5::log_target_display, ESP_LOG_NONE);
  M5.Log.setLogLevel(m5::log_target_serial, ESP_LOG_INFO);
  M5.Log.setEnableColor(m5::log_target_serial, false);

  M5_LOGI("SD_Card WiFiList Test");

  if (!SD.begin(GPIO_NUM_4, SPI, 25000000)) {
    M5_LOGI("[ERROR] SDカードの初期化に失敗しました");
    while (true) delay(1);
  }

  const char* path = "/SC_SecConfig.yaml";

  size_t total = countWiFiEntriesInSD(path);
  M5_LOGI("wifi entries=%u", (unsigned)total);
  if (total == 0) {
    M5_LOGE("No wifi entries or file open failed: %s", path);
    return;
  }

  // ---- ここを好きに変える：取りたい番号（1つ目=1, 2つ目=2 ...） ----
  size_t wantNumber = 1;  // 例：2つ目
  // -----------------------------------------------------------------

  if (wantNumber > total) {
    M5_LOGW("wifi #%u not found (total=%u)", (unsigned)wantNumber, (unsigned)total);
    return;
  }

  String ssid, pass;
  if (!loadWiFiByNumberFromSD(wantNumber, ssid, pass, path)) {
    M5_LOGE("loadWiFiByNumberFromSD failed");
    return;
  }
  M5_LOGI("wifi #%u ssid=%s", (unsigned)wantNumber, ssid.c_str());
  M5_LOGI("password=%s", pass.c_str());  // パスワードは出さない

  wantNumber = 2;
  if (!loadWiFiByNumberFromSD(wantNumber, ssid, pass, path)) {
    M5_LOGE("loadWiFiByNumberFromSD failed");
    return;
  }
  M5_LOGI("wifi #%u ssid=%s", (unsigned)wantNumber, ssid.c_str());
  M5_LOGI("password=%s", pass.c_str());  // パスワードは出さない

  wantNumber = 3;
  if (!loadWiFiByNumberFromSD(wantNumber, ssid, pass, path)) {
    M5_LOGE("loadWiFiByNumberFromSD failed");
    return;
  }
  M5_LOGI("wifi #%u ssid=%s", (unsigned)wantNumber, ssid.c_str());
  M5_LOGI("password=%s", pass.c_str());  // パスワードは出さない

  wantNumber = 4;
  if (!loadWiFiByNumberFromSD(wantNumber, ssid, pass, path)) {
    M5_LOGE("loadWiFiByNumberFromSD failed");
    return;
  }
  M5_LOGI("wifi #%u ssid=%s", (unsigned)wantNumber, ssid.c_str());
  M5_LOGI("password=%s", pass.c_str());  // パスワードは出さない
}

void loop() {}
