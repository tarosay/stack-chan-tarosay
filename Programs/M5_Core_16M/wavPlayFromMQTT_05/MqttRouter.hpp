#ifndef MQTTROUTER_HPP
#define MQTTROUTER_HPP

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <vector>
#include <functional>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>


class MqttRouter {
public:
  using HandlerFn = std::function<void(const char* topic, uint8_t* payload, unsigned int len)>;

  explicit MqttRouter(PubSubClient& mqtt);

  void begin(const IPAddress& brokerIp, uint16_t brokerPort,
             uint16_t rxBufSize = 8192);

  // 追加購読： topicFilter は "a/+/c" や "a/#" 可
  void addSubscription(const char* topicFilter, HandlerFn fn, uint8_t qos = 0);

  // loop()から呼ぶ
  void loop();

  // 任意：明示的に再購読したい時用
  void resubscribeAll();

  void enableAsyncDispatch(size_t queueDepth = 16);
  bool dispatchOne(TickType_t waitTicks = 0);  // キューから1件取り出してハンドラに配る

private:
  static void s_callback(char* topic, uint8_t* payload, unsigned int length);
  void onMessage_(char* topic, uint8_t* payload, unsigned int length);

  void ensureConnected_();
  bool topicMatch_(const char* filter, const char* topic) const;

  struct RxItem {
    char* topic;
    uint8_t* payload;
    uint16_t tlen;
    uint16_t plen;
  };

  bool async_ = false;
  QueueHandle_t q_ = nullptr;

  void enqueue_(char* topic, uint8_t* payload, unsigned int length);

private:
  PubSubClient& mqtt_;
  IPAddress brokerIp_;
  uint16_t brokerPort_ = 1883;

  struct Sub {
    String filter;
    uint8_t qos;
    HandlerFn fn;
  };
  std::vector<Sub> subs_;

  static MqttRouter* s_inst_;
};

#endif