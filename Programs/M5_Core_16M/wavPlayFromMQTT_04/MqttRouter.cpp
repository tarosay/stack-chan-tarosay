#include "MqttRouter.hpp"
#include <M5Unified.h>

MqttRouter* MqttRouter::s_inst_ = nullptr;

MqttRouter::MqttRouter(PubSubClient& mqtt)
  : mqtt_(mqtt) {}

void MqttRouter::begin(const IPAddress& brokerIp, uint16_t brokerPort, uint16_t rxBufSize) {
  brokerIp_ = brokerIp;
  brokerPort_ = brokerPort;

  mqtt_.setBufferSize(rxBufSize);  // ★ /pcm 対応で必須になりがち
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
    M5_LOGI("sub %s qos=%u -> %d", topicFilter, (unsigned)qos, ok);
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
    M5_LOGI("sub %s qos=%u -> %d", s.filter.c_str(), (unsigned)s.qos, ok);
  }
}

void MqttRouter::s_callback(char* topic, uint8_t* payload, unsigned int length) {
  if (s_inst_) s_inst_->onMessage_(topic, payload, length);
}

void MqttRouter::onMessage_(char* topic, uint8_t* payload, unsigned int length) {
  if (!topic || !payload || length == 0) return;

  // filter マッチしたもの全部に配る（必要なら最初の1件でbreakでもOK）
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
  if (mqtt_.connected()) return;

  String cid = "M5-" + String((uint32_t)ESP.getEfuseMac(), HEX);

  while (!mqtt_.connected()) {
    M5_LOGI("MQTT connecting...");
    if (mqtt_.connect(cid.c_str())) {
      M5_LOGI("MQTT connected");
      // 再接続した瞬間に購読復活
      resubscribeAll();
    } else {
      M5_LOGI("MQTT connect failed rc=%d", mqtt_.state());
      delay(500);
    }
  }
}
