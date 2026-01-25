#include <Arduino.h>
#include <M5Unified.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <PubSubClient.h>

#include "SdCard.hpp"
#include "WifiConnect.hpp"
#include "SpiBusLock.hpp"

// -----------------------------
// MQTT settings
// -----------------------------
static WiFiClient gNet;
static PubSubClient gMqtt(gNet);

static IPAddress gBrokerIp;
static uint16_t gBrokerPort = 1883;

static const char* TOPIC_SUB = "stack/mp3/#";
static const char* TOPIC_BEGIN = "stack/mp3/begin";
static const char* TOPIC_RAW = "stack/mp3/raw";
static const char* TOPIC_END = "stack/mp3/end";

// PubSubClient needs a buffer big enough to hold topic + payload.
// Node-RED side is publishing 8192-byte chunks, so keep some headroom.
static const uint16_t MQTT_BUF_SIZE = 16384;

struct SpiLock {
  SpiLock() {
    xSemaphoreTake(gSpiMutex, portMAX_DELAY);
  }
  ~SpiLock() {
    xSemaphoreGive(gSpiMutex);
  }
};

// -----------------------------
// RX file state (SD)
// -----------------------------
static String gRxPath = "/rx.mp3";
static size_t gRxBytes = 0;
static uint32_t gLastRxMs = 0;

static volatile bool gReceiving = false;  // begin〜end間
static volatile bool gEndSeen = false;
static volatile bool gOverflow = false;   // rb溢れ（取りこぼし）
static volatile bool gBeginSeen = false;  // begin受信済み（raw-only fallback判定用）

static File gFile;


// -----------------------------
// Ring buffer (no PSRAM: keep modest)
// -----------------------------
static size_t gRbSize = 0;
static uint8_t* rb = nullptr;
static volatile size_t rb_head = 0, rb_tail = 0, rb_used = 0;
static portMUX_TYPE rbMux = portMUX_INITIALIZER_UNLOCKED;

static inline size_t rb_free_unsafe() {
  return gRbSize - rb_used;
}

static void rb_reset() {
  portENTER_CRITICAL(&rbMux);
  rb_head = rb_tail = rb_used = 0;
  portEXIT_CRITICAL(&rbMux);
}

static bool rb_push(const uint8_t* p, size_t n) {
  if (!rb || n == 0) return false;

  bool ok = true;
  portENTER_CRITICAL(&rbMux);
  if (n > rb_free_unsafe()) ok = false;
  portEXIT_CRITICAL(&rbMux);
  if (!ok) return false;

  // wrap対応2分割コピー
  size_t first = min(n, gRbSize - rb_head);
  memcpy(rb + rb_head, p, first);
  memcpy(rb, p + first, n - first);

  portENTER_CRITICAL(&rbMux);
  rb_head = (rb_head + n) % gRbSize;
  rb_used += n;
  portEXIT_CRITICAL(&rbMux);
  return true;
}

// sdTask側：最大2分割で取り出して書けるようにポインタ+長さを返す
static size_t rb_peek2(const uint8_t** p1, size_t* n1,
                       const uint8_t** p2, size_t* n2,
                       size_t want) {
  portENTER_CRITICAL(&rbMux);

  size_t used = (size_t)rb_used;  // ← volatile を外すスナップショット
  size_t take = (want < used) ? want : used;

  size_t first = take;
  size_t tail = (size_t)rb_tail;  // ← こっちも念のため
  size_t span = gRbSize - tail;
  if (first > span) first = span;

  *p1 = rb + tail;
  *n1 = first;
  *p2 = rb;
  *n2 = take - first;

  portEXIT_CRITICAL(&rbMux);
  return take;
}

static void rb_drop(size_t n) {
  portENTER_CRITICAL(&rbMux);
  rb_tail = (rb_tail + n) % gRbSize;
  rb_used -= n;
  portEXIT_CRITICAL(&rbMux);
}
// -----------------------------
// Very small “JSON-ish” extractor
// Node-RED begin payload is JSON string.
// -----------------------------
static String payloadToString(const uint8_t* payload, unsigned int len) {
  String s;
  s.reserve(len + 1);
  for (unsigned int i = 0; i < len; i++) s += (char)payload[i];
  return s;
}

static String extractJsonStringField(const uint8_t* payload, unsigned int len, const char* key) {
  if (!payload || len == 0 || !key) return "";
  String s = payloadToString(payload, len);

  String k = "\"";
  k += key;
  k += "\"";
  int p = s.indexOf(k);
  if (p < 0) return "";
  p = s.indexOf(':', p + k.length());
  if (p < 0) return "";
  while (p < (int)s.length() && (s[p] == ':' || s[p] == ' ' || s[p] == '\t')) p++;
  if (p >= (int)s.length() || s[p] != '"') return "";
  p++;
  int e = s.indexOf('"', p);
  if (e < 0) return "";
  return s.substring(p, e);
}

static int64_t extractJsonIntField(const uint8_t* payload, unsigned int len, const char* key) {
  if (!payload || len == 0 || !key) return -1;
  String s = payloadToString(payload, len);

  String k = "\"";
  k += key;
  k += "\"";
  int p = s.indexOf(k);
  if (p < 0) return -1;
  p = s.indexOf(':', p + k.length());
  if (p < 0) return -1;
  while (p < (int)s.length() && (s[p] == ':' || s[p] == ' ' || s[p] == '\t')) p++;
  if (p >= (int)s.length()) return -1;

  // 数字列（マイナスも一応）
  int e = p;
  while (e < (int)s.length() && (isDigit((unsigned char)s[e]) || s[e] == '-')) e++;
  if (e <= p) return -1;
  return s.substring(p, e).toInt();
}

// -----------------------------
// RX control (begin/raw/end)
// SD write is done in sdTask only.
// -----------------------------
static void rxAbortIfNeeded() {
  // 途中セッションがあれば閉じて破棄（必要ならファイル削除などもここで）
  if (gReceiving) {
    gEndSeen = true;  // sdTaskに吐き切り＆closeさせたいならtrue
    // ただし「即中止」したい場合は、ここでclose+rb_resetしてもOK
  }
}

static void rxBegin(const uint8_t* payload, unsigned int len) {
  // 前のセッションが残ってたら破棄（ここは方針次第）
  if (gReceiving) {
    // 強制中止：バッファ捨てて閉じる
    rb_reset();
    {
      SpiLock lock;
      if (gFile) gFile.close();
    }
    gReceiving = false;
    gEndSeen = false;
    gOverflow = false;
    gRxBytes = 0;
  }

  String name = extractJsonStringField(payload, len, "name");
  if (name.length() == 0) name = "rx_" + String(millis()) + ".mp3";

  // sanitize（超保守）
  name.replace("..", "_");
  name.replace("/", "_");
  name.replace("\\", "_");

  gRxPath = "/" + name;
  gRxBytes = 0;
  gLastRxMs = millis();
  gBeginSeen = true;
  gOverflow = false;
  gEndSeen = false;

  // sizeがあれば（最大2MB想定）簡易プリ確保して断片化を減らす
  int64_t sizeHint = extractJsonIntField(payload, len, "size");

  {
    SpiLock lock;

    SD.remove(gRxPath.c_str());
    gFile = SD.open(gRxPath.c_str(), FILE_WRITE);
    if (!gFile) {
      M5_LOGE("[RX] begin: failed to open %s", gRxPath.c_str());
      gReceiving = false;
      return;
    }

    if (sizeHint > 0) {
      // 事前確保（最後の1バイトを書いてサイズ確定→先頭に戻す）
      gFile.seek((uint32_t)(sizeHint - 1));
      uint8_t z = 0;
      gFile.write(&z, 1);
      gFile.flush();
      gFile.seek(0);
      M5_LOGI("[RX] prealloc size=%d", (int)sizeHint);
    }
  }

  rb_reset();
  gReceiving = true;

  M5_LOGI("[RX] begin -> %s", gRxPath.c_str());
}

static void rxEnd() {
  M5_LOGI("[RX] end seen (will flush+close) path=%s", gRxPath.c_str());
  gEndSeen = true;
  gBeginSeen = false;
}

// -----------------------------
// MQTT callback
// -----------------------------
static void mqttCallback(char* topic, uint8_t* payload, unsigned int length) {
  if (!topic) return;

  if (strcmp(topic, TOPIC_BEGIN) == 0) {
    rxBegin(payload, length);
    return;
  }

  if (strcmp(topic, TOPIC_RAW) == 0) {
    const uint32_t now = millis();

    // raw-only fallback（必要なら）
    if (!gBeginSeen || (now - gLastRxMs) > 1500) {
      const char* fallback = "{\"name\":\"rx_raw.mp3\",\"size\":0}";
      rxBegin((const uint8_t*)fallback, strlen(fallback));
    }

    gLastRxMs = now;

    // SDには書かない。リングへ入れるだけ。
    if (!rb_push(payload, length)) {
      gOverflow = true;  // 取りこぼし
    }
    return;
  }

  if (strcmp(topic, TOPIC_END) == 0) {
    rxEnd();
    return;
  }

  M5_LOGI("[MQTT] topic=%s len=%u", topic, (unsigned)length);
}

static bool mqttEnsureConnected() {
  if (gMqtt.connected()) return true;

  uint32_t r = (uint32_t)esp_random();
  String clientId = "m5stack-" + String((uint32_t)ESP.getEfuseMac(), HEX) + "-" + String(r, HEX);

  M5_LOGI("[MQTT] connecting to %s:%u ...", gBrokerIp.toString().c_str(), (unsigned)gBrokerPort);

  if (!gMqtt.connect(clientId.c_str())) {
    M5_LOGW("[MQTT] connect failed, state=%d", gMqtt.state());
    return false;
  }

  if (!gMqtt.subscribe(TOPIC_SUB, 0)) {
    M5_LOGE("[MQTT] subscribe failed: %s", TOPIC_SUB);
    return false;
  }

  M5_LOGI("[MQTT] subscribed: %s", TOPIC_SUB);
  return true;
}

// -----------------------------
// SD writer task: write in blocks (reduce SPI collisions)
// -----------------------------
static constexpr size_t WRITE_BLOCK = 64 * 1024;  // 64KB一括write

static void sdTask(void*) {
  for (;;) {
    if (gReceiving) {
      size_t used_snapshot;
      portENTER_CRITICAL(&rbMux);
      used_snapshot = rb_used;
      portEXIT_CRITICAL(&rbMux);

      // 64KB溜まったら書く / end後は残りも吐く
      bool do_write = (used_snapshot >= WRITE_BLOCK) || (gEndSeen && used_snapshot > 0);

      if (do_write) {
        size_t want = gEndSeen ? used_snapshot : WRITE_BLOCK;

        const uint8_t *p1, *p2;
        size_t n1, n2;
        size_t take = rb_peek2(&p1, &n1, &p2, &n2, want);

        if (take > 0) {
          SpiLock lock;
          if (gFile) {
            if (n1) gRxBytes += gFile.write(p1, n1);
            if (n2) gRxBytes += gFile.write(p2, n2);
          }
        }
        rb_drop(take);
      }

      // end後、バッファが空になったら close
      portENTER_CRITICAL(&rbMux);
      used_snapshot = rb_used;
      portEXIT_CRITICAL(&rbMux);

      if (gEndSeen && used_snapshot == 0) {
        {
          SpiLock lock;
          if (gFile) {
            gFile.flush();
            gFile.close();
          }
        }

        M5_LOGI("[RX] closed -> %s total=%u bytes overflow=%d",
                gRxPath.c_str(), (unsigned)gRxBytes, (int)gOverflow);

        // overflowなら失敗扱い（必要なら削除）
        if (gOverflow) {
          SpiLock lock;
          SD.remove(gRxPath.c_str());
          M5_LOGW("[RX] removed due to overflow: %s", gRxPath.c_str());
        }

        gReceiving = false;
        gEndSeen = false;
        gOverflow = false;
        gRxBytes = 0;
      }
    }

    vTaskDelay(1);
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);
  M5.begin();
  M5.Log.setLogLevel(m5::log_target_display, ESP_LOG_NONE);
  M5.Log.setLogLevel(m5::log_target_serial, ESP_LOG_INFO);
  M5.Log.setEnableColor(m5::log_target_serial, false);

  M5_LOGI("MQTT Test");

  gSpiMutex = xSemaphoreCreateMutex();

  M5_LOGI("MQTT MP3 RX (ringbuf -> SD block write)");

  // ring buffer alloc (try 256KB -> 128KB)
  {
    size_t trySizes[] = { 256 * 1024, 128 * 1024, 64 * 1024 };
    for (size_t s : trySizes) {
      rb = (uint8_t*)heap_caps_malloc(s, MALLOC_CAP_8BIT);
      if (rb) {
        gRbSize = s;
        break;
      }
    }
    if (!rb) {
      M5_LOGE("[FATAL] rb alloc failed");
      while (true) delay(1);
    }
    rb_reset();
    M5_LOGI("[RB] size=%u bytes", (unsigned)gRbSize);
  }

  // SD init（SdCard.hpp 経由）
  {
    SpiLock lock;
    if (!sdCard.begin(GPIO_NUM_4, SPI, 25000000)) {
      M5_LOGE("[FATAL] SD init failed");
      while (true) delay(1);
    }
  }

  if (!wifiConnect.connectToWiFi()) {
    M5_LOGI("[ERROR] WiFiの接続に失敗しました");
    while (true) delay(1);
  }

  M5_LOGI("ip = %s", wifiConnect.ipAddress.c_str());

  if (!wifiConnect.discoverMqttBroker("DISCOVER_MQTT_V1", gBrokerIp, gBrokerPort)) {
    M5_LOGI("[WARN] discoverMqttBroker failed");
    while (true) delay(1);
  }

  M5_LOGI("broker = %s:%u", gBrokerIp.toString().c_str(), (unsigned)gBrokerPort);

  gMqtt.setServer(gBrokerIp, gBrokerPort);
  gMqtt.setCallback(mqttCallback);
  gMqtt.setBufferSize(MQTT_BUF_SIZE);
  gMqtt.setKeepAlive(15);

  // SD writer task start
  xTaskCreatePinnedToCore(sdTask, "sdTask", 4096, nullptr, 2, nullptr, 0);

  mqttEnsureConnected();
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    delay(200);
    return;
  }

  if (!gMqtt.connected()) {
    static uint32_t lastTry = 0;
    uint32_t now = millis();
    if (now - lastTry > 1000) {
      lastTry = now;
      mqttEnsureConnected();
    }
  }

  gMqtt.loop();
  delay(1);
}
