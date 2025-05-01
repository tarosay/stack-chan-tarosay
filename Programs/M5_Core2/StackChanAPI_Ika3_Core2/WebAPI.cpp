#include <M5Unified.h>
#include "WebAPI.hpp"

// コンストラクタ
WebAPI::WebAPI(int port)
  : server(port), serverStarted(false), fileUploaded(0), restart(false), wavng(false), ongen(0),
    spVolume(0.4f),
    isMove(false),
    isSpeech(false),
    isSing(false),
    intoroTime(0),
    rhythmTime(500) {}

// WiFi接続とサーバーの初期化
void WebAPI::begin(const char* ssid, const char* password) {
  connectToWiFi(ssid, password);

  // HTTPサーバールート設定
  server.on("/", HTTP_GET, [this]() {
    handleRoot();
  });

  server.on(
    "/upload", HTTP_POST,
    [this]() {
      // POSTリクエストが到達したら即座にレスポンスを返す
      server.send(200, "text/plain", "Ready to upload");
    },
    [this]() {
      // アップロード処理
      handleUpload();
    });

  // 再起動エンドポイント
  server.on("/restart", HTTP_GET, [this]() {
    server.send(200, "text/plain", "Restarting...");
    delay(1000);
    restart = true;  // レスポンス送信後に少し待機
    //esp_restart();  // M5Stackを再起動
  });

  server.on("/wavng", HTTP_GET, [this]() {
    server.send(200, "text/plain", "Wav upload NG");
    wavng = true;
  });

  // `ongen` パラメータを取得
  server.on("/ongen", HTTP_GET, [this]() {
    if (server.hasArg("type")) {
      ongen = server.arg("type").toInt();  // "type" の値を取得し、整数として `ongen` に代入
      server.send(200, "text/plain", "ongen set to: " + String(ongen));
    } else {
      server.send(400, "text/plain", "Missing parameter: type");
    }
  });

  // 音量設定
  server.on("/volume", HTTP_GET, [this]() {
    if (server.hasArg("value")) {
      spVolume = server.arg("value").toFloat();
      server.send(200, "text/plain", "volume set to: " + String(spVolume));
    } else {
      server.send(400, "text/plain", "Missing parameter: value");
    }
  });

  // X,Yサーボモータ回転
  server.on("/move", HTTP_GET, [this]() {
    int x = 0, y = 0;
    // クエリパラメータを取得
    if (server.hasArg("x")) {
      x = server.arg("x").toInt();
    }
    if (server.hasArg("y")) {
      y = server.arg("y").toInt();
    }
    moveXY.x = x;
    moveXY.y = y;
    isMove = true;
    server.send(200, "text/plain", "move set to: (" + String(x) + ", " + String(y) + ")");
  });

  // しゃべらせる
  server.on("/speech", HTTP_GET, [this]() {
    int x = 0, y = 0;
    // クエリパラメータを取得
    if (server.hasArg("x")) {
      x = server.arg("x").toInt();
    }
    if (server.hasArg("y")) {
      y = server.arg("y").toInt();
    }
    if (server.hasArg("wavFilename")) {
      String wavFilenameStr = server.arg("wavFilename");
      // 既存のメモリを解放
      if (wavFilename) {
        free(wavFilename);
        wavFilename = nullptr;
      }

      // 新しい値をセット
      wavFilename = strdup(wavFilenameStr.c_str());
    }

    moveXY.x = x;
    moveXY.y = y;
    isSpeech = true;

    // レスポンス送信
    String response = "speech set to: (" + String(x) + ", " + String(y) + ")" + ", wavFilename: " + String(wavFilename);
    server.send(200, "text/plain", response);
  });

  // 歌う
  server.on("/sing", HTTP_GET, [this]() {
    if (server.hasArg("wavFilename")) {
      String wavFilenameStr = server.arg("wavFilename");
      int intoro = 0;
      int rhythm = 500;
      // 既存のメモリを解放
      if (wavFilename) {
        free(wavFilename);
        wavFilename = nullptr;
      }

      if (server.hasArg("intoro")) {
        intoro = server.arg("intoro").toInt();
      }

      if (server.hasArg("rhythm")) {
        rhythm = server.arg("rhythm").toInt();
      }

      // 新しい値をセット
      wavFilename = strdup(wavFilenameStr.c_str());
      intoroTime = intoro;
      rhythmTime = rhythm;
      isSing = true;
    }

    // レスポンス送信
    String response = "sing this " + String(wavFilename);
    server.send(200, "text/plain", response);
  });

  // サーバースタート
  if (WiFi.status() == WL_CONNECTED) {
    server.begin();
    serverStarted = true;
  }
}

void WebAPI::handleClient() {
  if (serverStarted) {
    server.handleClient();
  }
}

void WebAPI::connectToWiFi(const char* ssid, const char* password) {
  WiFi.begin(ssid, password);

  int maxRetries = 20;  // 10秒間待機 (500ms × 20回)
  while (WiFi.status() != WL_CONNECTED && maxRetries > 0) {
    delay(500);
    maxRetries--;
  }

  if (WiFi.status() == WL_CONNECTED) {
    ipAddress = WiFi.localIP().toString();
    if (!serverStarted) {
      server.begin();
      serverStarted = true;
    }
  } else {
    ipAddress = "0.0.0.0";
  }

  M5_LOGI("IP: %s", ipAddress.c_str());
}

void WebAPI::disconnectWiFi() {
  if (WiFi.status() == WL_CONNECTED) {
    WiFi.disconnect();
    ipAddress = "0.0.0.0";
  }
}

String WebAPI::getIPAddress() const {
  return ipAddress;
}

String WebAPI::getUploadFilename() const {
  return uploadfilename;
}

// "/" エンドポイント処理
void WebAPI::handleRoot() {
  server.send(200, "text/plain", "Yes, I Speak.");
  // アップロードタイプをセット、これを1にすると/upload.wavをしゃべる
  fileUploaded = 100;
}

// "/upload" エンドポイント処理
void WebAPI::handleUpload() {
  //M5_LOGI("handleUpload called with URI: %s", server.uri().c_str());

  if (server.uri() != "/upload" || server.method() != HTTP_POST) {
    server.send(404, "text/plain", "Not Found");
    return;
  }

  HTTPUpload& upload = server.upload();
  //M5_LOGI("Upload status: %d", upload.status);

  // ファイル保存先
  static File file;
  static String filePath;

  //const char* filePath = "/upload.wav";
  //static File file;

  if (upload.status == UPLOAD_FILE_START) {
    uploadfilename = upload.filename;
    uploadfilename.toLowerCase();

    // 拡張子チェック
    if (uploadfilename.endsWith(".wav")) {
      filePath = "/upload.wav";
    } else if (uploadfilename.endsWith(".mp3")) {
      filePath = "/upload.mp3";
    } else {
      server.send(400, "text/plain", "Unsupported file type");
      fileUploaded = 2;
      return;
    }

    if (SD.exists(filePath)) {
      SD.remove(filePath);
    }
    file = SD.open(filePath, FILE_WRITE);
    if (!file) {
      server.send(500, "text/plain", "Failed to open file for writing");
      fileUploaded = 2;
      return;
    }

  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (file) {
      file.write(upload.buf, upload.currentSize);
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    //M5_LOGI("UPLOAD_FILE_END: Total %d bytes", upload.totalSize);
    if (file) {
      file.close();

      file = SD.open(filePath, FILE_READ);
      if (file) {
        size_t filesize = file.size();
        file.close();
        if (filesize == 0) {
          server.send(500, "text/plain", "Failed to save file.");
          fileUploaded = 2;
        } else {
          server.send(200, "text/plain", "File uploaded successfully.");
          fileUploaded = 1;
        }
      } else {
        server.send(500, "text/plain", "Failed to save file.");
        fileUploaded = 2;
      }
    } else {
      server.send(500, "text/plain", "Failed to save file.");
      fileUploaded = 2;
    }
  } else if (upload.status == UPLOAD_FILE_ABORTED) {
    M5_LOGI("UPLOAD_FILE_ABORTED");
    if (file) {
      file.close();
      SD.remove(filePath);
    }
    server.send(500, "text/plain", "Upload aborted.");
    fileUploaded = 2;
  }
}

// ファイルアップロードの状態を確認
int WebAPI::getFileUploaded() {
  return fileUploaded;
}

bool WebAPI::isReStart() {
  return restart;
}

bool WebAPI::isWavNG() {
  return wavng;
}

// アップロードフラグをリセット
void WebAPI::resetFileUploadedType() {
  fileUploaded = 0;
}

// アップロードフラグをリセット
void WebAPI::resetWavNG() {
  wavng = false;
}

int WebAPI::getOngen() {
  return ongen;
}
void WebAPI::resetOngen() {
  ongen = 0;
}

float WebAPI::getVolume() {
  return spVolume;
}

WebAPI::MoveXY WebAPI::getMoveXY() {
  isMove = false;  // 値を取得後、リセットする場合
  return moveXY;
}
bool WebAPI::getIsMove() {
  return isMove;
}

bool WebAPI::getIsSpeech() {
  return isSpeech;
}

const char* WebAPI::getWavFilename() {
  isSpeech = false;
  isSing = false;
  return wavFilename;
}

bool WebAPI::getIsSing() {
  return isSing;
}
int WebAPI::getIntoroTime() {
  return intoroTime;
}
int WebAPI::getRhythmTime() {
  return rhythmTime;
}
