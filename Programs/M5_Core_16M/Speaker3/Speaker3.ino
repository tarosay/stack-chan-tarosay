#include <M5Unified.h>
#include <SPI.h>
#include <SD.h>

#include <MP3DecoderHelix.h>  // 追加したHelixデコーダ側のヘッダ
using namespace libhelix;

static File g_mp3;
static MP3DecoderHelix g_dec;

static void pcmCallback(MP3FrameInfo& info, short* pcm, size_t len, void* /*ref*/)
{
  const bool stereo = (info.nChans == 2);   // channels じゃなく nChans :contentReference[oaicite:3]{index=3}
  const uint32_t sr = info.samprate;

  while (!M5.Speaker.playRaw((const int16_t*)pcm, len, sr, stereo, 1, 0, false)) {
    delay(1);
  }
}

void setup() {
  auto cfg = M5.config();
  cfg.internal_mic = false;  // 不要なら切る
  M5.begin(cfg);

  // Speaker 初期化（sample_rate は playRaw でも渡せるが、初期値を置く）
  auto spk = M5.Speaker.config();
  spk.sample_rate = 44100;
  spk.stereo = true;
  M5.Speaker.config(spk);
  M5.Speaker.setVolume(200);
  M5.Speaker.begin();

  // SD (M5Stack Basic系: CS=4 / SCK=18 / MISO=19 / MOSI=23 が一般的)
  SPI.begin(18, 19, 23, 4);
  if (!SD.begin(4, SPI, 25000000)) {
    M5.Display.println("SD.begin failed");
    for (;;) { delay(1000); }
  }

  // デコーダ開始（コールバック登録）
  g_dec.setDataCallback(pcmCallback);  // ←これ
  g_dec.begin();                       // ライブラリのAPIに合わせる

  g_mp3 = SD.open("/mp3/file01.mp3", FILE_READ);
  if (!g_mp3) {
    M5.Display.println("open failed: /mp3/file01.mp3");
    for (;;) { delay(1000); }
  }

  M5.Display.println("MP3 playing...");
}

void loop() {
  static uint8_t buf[2048];

  int n = g_mp3.read(buf, sizeof(buf));
  if (n > 0) {
    g_dec.write(buf, n);
  } else {
    g_mp3.close();
    M5.Display.println("done");
    for (;;) { delay(1000); }
  }

  M5.update();
}
