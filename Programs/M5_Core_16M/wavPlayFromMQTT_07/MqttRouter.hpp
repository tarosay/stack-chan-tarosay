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

// ---- Pool config（CHUNK_BYTESを変えるならPCM_MAXも変える）----
#ifndef MQTTROUTER_TOPIC_MAX
#define MQTTROUTER_TOPIC_MAX 96
#endif

#ifndef MQTTROUTER_SMALL_MAX
#define MQTTROUTER_SMALL_MAX 512
#endif

// いま pcm は 8 + 4096 = 4104 が基本。余裕を少し持たせる
#ifndef MQTTROUTER_PCM_MAX
#define MQTTROUTER_PCM_MAX 4128
#endif

#ifndef MQTTROUTER_SMALL_POOL_N
#define MQTTROUTER_SMALL_POOL_N 16
#endif

#ifndef MQTTROUTER_PCM_POOL_N
#define MQTTROUTER_PCM_POOL_N 12
#endif

  enum MsgKind : uint8_t { MK_SMALL = 1,
                           MK_PCM = 2 };

  struct SmallMsg {
    uint8_t kind;
    uint8_t poolIndex;
    uint16_t tlen;
    uint16_t plen;
    char topic[MQTTROUTER_TOPIC_MAX];
    alignas(4) uint8_t payload[MQTTROUTER_SMALL_MAX];  // ★追加
  };

  struct PcmMsg {
    uint8_t kind;
    uint8_t poolIndex;
    uint16_t tlen;
    uint16_t plen;
    char topic[MQTTROUTER_TOPIC_MAX];
    alignas(4) uint8_t payload[MQTTROUTER_PCM_MAX];  // ★追加
  };

  bool async_ = false;

  // rxキュー：SmallMsg* / PcmMsg* を混在で流す
  QueueHandle_t qRx_ = nullptr;

  // freeキュー：空きブロックの index を配る
  QueueHandle_t qSmallFree_ = nullptr;
  QueueHandle_t qPcmFree_ = nullptr;

  SmallMsg smallPool_[MQTTROUTER_SMALL_POOL_N];
  PcmMsg pcmPool_[MQTTROUTER_PCM_POOL_N];

  static bool endsWith_(const char* s, const char* suf);

  void enqueuePool_(char* topic, uint8_t* payload, unsigned int length);

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