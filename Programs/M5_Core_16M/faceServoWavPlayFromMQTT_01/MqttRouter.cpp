#include "MqttRouter.hpp"
#include <M5Unified.h>

extern WiFiClient gWiFiClient;

MqttRouter* MqttRouter::s_inst_ = nullptr;

MqttRouter::MqttRouter(PubSubClient& mqtt)
  : mqtt_(mqtt) {}


static void dumpHeap(const char* tag) {
  size_t free8 = heap_caps_get_free_size(MALLOC_CAP_8BIT);
  size_t large8 = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
  size_t freeInt = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
  size_t largeInt = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);
  Serial.printf("[HEAP] %s free8=%u largest8=%u freeInt=%u largestInt=%u\n",
                tag, (unsigned)free8, (unsigned)large8, (unsigned)freeInt, (unsigned)largeInt);
}


void MqttRouter::begin(const IPAddress& brokerIp, uint16_t brokerPort, uint16_t rxBufSize) {
  brokerIp_ = brokerIp;
  brokerPort_ = brokerPort;

  mqtt_.setBufferSize(rxBufSize);  // ★ /pcm 対応で必須になりがち

  dumpHeap("after setBufferSize");

  mqtt_.setServer(brokerIp_, brokerPort_);

  s_inst_ = this;
  mqtt_.setCallback(&MqttRouter::s_callback);

  ensureConnected_();
  resubscribeAll();
}

void MqttRouter::addSubscription(const char* topicFilter, HandlerFn fn, uint8_t qos) {
  Sub s;
  s.filter = topicFilter;
  s.qos = qos;
  s.fn = fn;
  subs_.push_back(std::move(s));

  // 既に接続済みならここで購読（再接続時はresubscribeAllで復帰）
  if (mqtt_.connected()) {
    bool ok = mqtt_.subscribe(topicFilter, qos);
    // M5_LOGI("sub %s qos=%u -> %d", topicFilter, (unsigned)qos, ok);  // (disabled) verbose
  }
}

void MqttRouter::loop() {
  ensureConnected_();
  mqtt_.loop();
}

void MqttRouter::resubscribeAll() {
  if (!mqtt_.connected()) return;
  for (auto& s : subs_) {
    bool ok = mqtt_.subscribe(s.filter.c_str(), s.qos);
    // M5_LOGI("sub %s qos=%u -> %d", s.filter.c_str(), (unsigned)s.qos, ok);  // (disabled) verbose
  }
}

void MqttRouter::s_callback(char* topic, uint8_t* payload, unsigned int length) {
  if (s_inst_) s_inst_->onMessage_(topic, payload, length);
}

void MqttRouter::onMessage_(char* topic, uint8_t* payload, unsigned int length) {
  if (!topic || !payload || length == 0) return;

  if (async_) {
    // ★ /pcm は fast-path（キュー/プールに溜めない）
    // PREBURST などで一気に来ても、pool枯渇による drop→seq jump を避ける。
    if (endsWith_(topic, "/pcm") || endsWith_(topic, "/ctrl")) {
      for (auto& s : subs_) {
        if (topicMatch_(s.filter.c_str(), topic)) {
          if (s.fn) s.fn(topic, payload, length);
        }
      }
      return;
    }

    enqueuePool_(topic, payload, length);
    return;
  }

  // async無効時は従来動作（その場でハンドラ実行）
  for (auto& s : subs_) {
    if (topicMatch_(s.filter.c_str(), topic)) {
      if (s.fn) s.fn(topic, payload, length);
    }
  }
}

// MQTT topic filter: + と # のみ対応（充分）
bool MqttRouter::topicMatch_(const char* filter, const char* topic) const {
  const char* f = filter;
  const char* t = topic;

  while (*f && *t) {
    if (*f == '#') return true;  // 末尾の#で全部マッチ
    if (*f == '+') {
      // 次の '/' まで飛ばす
      while (*t && *t != '/') t++;
      f++;
    } else {
      if (*f != *t) return false;
      f++;
      t++;
    }
    if (*f == '/' && *t == '/') {
      f++;
      t++;
    }
  }

  // 末尾処理
  if (*f == '\0' && *t == '\0') return true;
  if (*f == '#' && f[1] == '\0') return true;
  return false;
}

void MqttRouter::ensureConnected_() {

  //dumpHeap("before mqtt_.connected");
  if (mqtt_.connected()) return;

  static bool tried = false;
  if (tried) return;  // ★同セッションは再試行しない
  tried = true;


  size_t largest8 = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
  size_t free8 = heap_caps_get_free_size(MALLOC_CAP_8BIT);

  // ★閾値未満なら “試さない”
  if (largest8 < 16 * 1024 || free8 < 20 * 1024) {
    Serial.printf("[MQTT] SKIP connect largest8=%u free8=%u\n",
                  (unsigned)largest8, (unsigned)free8);
    return;
  }

  String cid = "M5-" + String((uint32_t)ESP.getEfuseMac(), HEX);

  // ★ソケット掃除は一度だけ
  gWiFiClient.stop();
  vTaskDelay(pdMS_TO_TICKS(20));

  bool ok = mqtt_.connect(cid.c_str());
  Serial.printf("[MQTT] connect=%d rc=%d\n", ok, mqtt_.state());

  if (ok) resubscribeAll();

  // while (!mqtt_.connected()) {
  //   // M5_LOGI("MQTT connecting...");  // (disabled) verbose

  //   //dumpHeap("retry mqtt_.connected");

  //   // 前回失敗の半端なソケットを必ず潰す（lwIP側のリソース回収を促す）
  //   gWiFiClient.stop();
  //   vTaskDelay(pdMS_TO_TICKS(20));

  //   if (mqtt_.connect(cid.c_str())) {
  //     // M5_LOGI("MQTT connected");  // (disabled) verbose
  //     // 再接続した瞬間に購読復活
  //     resubscribeAll();
  //   } else {
  //     M5_LOGW("MQTT connect failed rc=%d", mqtt_.state());
  //     gWiFiClient.stop();
  //     dumpHeap("after stop");
  //     vTaskDelay(pdMS_TO_TICKS(500));
  //   }
  // }
}

void MqttRouter::enableAsyncDispatch(size_t queueDepth) {
  async_ = true;

  if (qRx_) vQueueDelete(qRx_);
  if (qSmallFree_) vQueueDelete(qSmallFree_);
  if (qPcmFree_) vQueueDelete(qPcmFree_);

  // rxQueue は「受信待ち」用：深さは任意だが、プール数以上は意味がない
  const size_t maxDepth = MQTTROUTER_SMALL_POOL_N + MQTTROUTER_PCM_POOL_N;
  if (queueDepth == 0) queueDepth = maxDepth;
  if (queueDepth > maxDepth) queueDepth = maxDepth;

  qRx_ = xQueueCreate(queueDepth, sizeof(void*));
  qSmallFree_ = xQueueCreate(MQTTROUTER_SMALL_POOL_N, sizeof(uint8_t));
  qPcmFree_ = xQueueCreate(MQTTROUTER_PCM_POOL_N, sizeof(uint8_t));

  // free キューに全ブロックを登録
  for (uint8_t i = 0; i < MQTTROUTER_SMALL_POOL_N; i++) {
    smallPool_[i].kind = MK_SMALL;
    smallPool_[i].poolIndex = i;
    xQueueSend(qSmallFree_, &i, 0);
  }
  for (uint8_t i = 0; i < MQTTROUTER_PCM_POOL_N; i++) {
    pcmPool_[i].kind = MK_PCM;
    pcmPool_[i].poolIndex = i;
    xQueueSend(qPcmFree_, &i, 0);
  }
}

// void MqttRouter::enqueue_(char* topic, uint8_t* payload, unsigned int length) {
//   if (!q_ || !topic || !payload || length == 0) return;

//   RxItem* m = (RxItem*)malloc(sizeof(RxItem));
//   if (!m) return;

//   m->tlen = (uint16_t)strlen(topic);
//   m->plen = (uint16_t)length;

//   m->topic = (char*)malloc(m->tlen + 1);
//   m->payload = (uint8_t*)malloc(m->plen);

//   if (!m->topic || !m->payload) {
//     if (m->topic) free(m->topic);
//     if (m->payload) free(m->payload);
//     free(m);
//     return;
//   }

//   memcpy(m->topic, topic, m->tlen + 1);  // '\0'含む
//   memcpy(m->payload, payload, m->plen);

//   // キュー満杯なら捨てる（ここは方針次第）
//   if (xQueueSend(q_, &m, 0) != pdTRUE) {
//     free(m->topic);
//     free(m->payload);
//     free(m);
//     // M5_LOGI("MQTT rx queue full (drop)");
//   }
// }

bool MqttRouter::endsWith_(const char* s, const char* suf) {
  size_t ls = strlen(s), l = strlen(suf);
  if (ls < l) return false;
  return memcmp(s + (ls - l), suf, l) == 0;
}

void MqttRouter::enqueuePool_(char* topic, uint8_t* payload, unsigned int length) {
  if (!qRx_ || !topic || !payload || length == 0) return;

  const size_t tlen = strlen(topic);
  if (tlen >= MQTTROUTER_TOPIC_MAX) {
    // topic が長すぎるものは捨てる（切り詰めでも良いが後々事故る）
    return;
  }

  const bool isPcm = endsWith_(topic, "/pcm");

  // PCMは固定ブロック、他はsmallへ
  if (isPcm) {
    if (length > MQTTROUTER_PCM_MAX) return;

    uint8_t idx;
    if (xQueueReceive(qPcmFree_, &idx, 0) != pdTRUE) {
      // 空き無し → drop
      return;
    }
    PcmMsg* m = &pcmPool_[idx];
    m->tlen = (uint16_t)tlen;
    m->plen = (uint16_t)length;
    memcpy(m->topic, topic, tlen + 1);
    memcpy(m->payload, payload, length);

    void* vp = (void*)m;
    if (xQueueSend(qRx_, &vp, 0) != pdTRUE) {
      // rxQueue満杯 → 返却してdrop
      xQueueSend(qPcmFree_, &idx, 0);
    }
    return;
  }

  // small
  if (length > MQTTROUTER_SMALL_MAX) {
    // 長い非PCMは今は捨てる（必要なら別プール追加）
    return;
  }

  uint8_t idx;
  if (xQueueReceive(qSmallFree_, &idx, 0) != pdTRUE) return;

  SmallMsg* m = &smallPool_[idx];
  m->tlen = (uint16_t)tlen;
  m->plen = (uint16_t)length;
  memcpy(m->topic, topic, tlen + 1);
  memcpy(m->payload, payload, length);

  void* vp = (void*)m;
  if (xQueueSend(qRx_, &vp, 0) != pdTRUE) {
    xQueueSend(qSmallFree_, &idx, 0);
  }
}

bool MqttRouter::dispatchOne(TickType_t waitTicks) {
  if (!qRx_) return false;

  void* vp = nullptr;
  if (xQueueReceive(qRx_, &vp, waitTicks) != pdTRUE) return false;
  if (!vp) return false;

  const uint8_t kind = *(uint8_t*)vp;

  if (kind == MK_PCM) {
    PcmMsg* m = (PcmMsg*)vp;

    for (auto& s : subs_) {
      if (topicMatch_(s.filter.c_str(), m->topic)) {
        if (s.fn) s.fn(m->topic, m->payload, m->plen);
      }
    }

    uint8_t idx = m->poolIndex;
    xQueueSend(qPcmFree_, &idx, 0);
    return true;
  }

  if (kind == MK_SMALL) {
    SmallMsg* m = (SmallMsg*)vp;

    for (auto& s : subs_) {
      if (topicMatch_(s.filter.c_str(), m->topic)) {
        if (s.fn) s.fn(m->topic, m->payload, m->plen);
      }
    }

    uint8_t idx = m->poolIndex;
    xQueueSend(qSmallFree_, &idx, 0);
    return true;
  }

  // 不明 → 捨てる
  return true;
}
