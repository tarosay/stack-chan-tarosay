#include <M5Unified.h>
#include <SD.h>

static const char* kPath = "/wav/21.wav";
static uint8_t* wav_buf = nullptr;
static size_t   wav_len = 0;

void setup() {
  auto cfg = M5.config();
  cfg.serial_baudrate = 115200;
  M5.begin(cfg);

  M5.Speaker.begin();
  M5.Speaker.setVolume(128);

  if (!SD.begin(GPIO_NUM_4)) {   // BASIC系のSD CSは多くが4
    Serial.println("SD.begin failed");
    for(;;) delay(1000);
  }

  File f = SD.open(kPath, "r");
  if (!f) {
    Serial.printf("open failed: %s\n", kPath);
    for(;;) delay(1000);
  }

  wav_len = f.size();
  Serial.printf("WAV size = %u bytes\n", (unsigned)wav_len);

  // でか過ぎ防止（必要に応じて調整）
  if (wav_len == 0 || wav_len > 1024 * 1024) {
    Serial.println("WAV too large (or empty) for no-PSRAM board.");
    f.close();
    for(;;) delay(1000);
  }

  wav_buf = (uint8_t*)malloc(wav_len);
  if (!wav_buf) {
    Serial.println("malloc failed");
    f.close();
    for(;;) delay(1000);
  }

  size_t rd = f.read(wav_buf, wav_len);
  f.close();
  if (rd != wav_len) {
    Serial.printf("read failed: %u/%u\n", (unsigned)rd, (unsigned)wav_len);
    free(wav_buf);
    wav_buf = nullptr;
    for(;;) delay(1000);
  }

  Serial.println("playWav...");
  // M5Unifiedの版によってオーバーロードが違うので、まずは2引数を試す
  // もしここがコンパイルエラーになるなら、下の1引数版にしてください。
  M5.Speaker.playWav(wav_buf, wav_len);
  // M5.Speaker.playWav(wav_buf);  // ←2引数が無い場合はこちら
}

void loop() {
  delay(10);
}
