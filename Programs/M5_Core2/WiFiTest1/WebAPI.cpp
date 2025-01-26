#include <M5Unified.h>
#include "WebAPI.hpp"

// コンストラクタ
WebAPI::WebAPI(int port)
  : server(port), serverStarted(false), fileUploaded(false) {}

// WiFi接続とサーバーの初期化
void WebAPI::begin(const char* ssid, const char* password) {
  connectToWiFi(ssid, password);

  // HTTPサーバールート設定
  server.on("/", HTTP_GET, [this]() {
    handleRoot();
  });
  server.on(
    "/upload", HTTP_POST, [this]() {}, [this]() {
      handleUpload();
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

// "/" エンドポイント処理
void WebAPI::handleRoot() {
  server.send(200, "text/plain", "M5Stack Core2 is ready!");
}

// "/upload" エンドポイント処理
void WebAPI::handleUpload() {
  if (server.uri() != "/upload" || server.method() != HTTP_POST) {
    server.send(404, "text/plain", "Not Found");
    return;
  }

  HTTPUpload& upload = server.upload();

  // ファイル保存先
  const char* filePath = "/upload.wav";
  static File file;

  if (upload.status == UPLOAD_FILE_START) {
    // 既存ファイル削除 & 新規作成
    if (SD.exists(filePath)) {
      SD.remove(filePath);
    }
    file = SD.open(filePath, FILE_WRITE);
    if (!file) {
      server.send(500, "text/plain", "Failed to open file for writing");
      return;
    }

  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (file) {
      file.write(upload.buf, upload.currentSize);
    }

  } else if (upload.status == UPLOAD_FILE_END) {
    if (file) {
      file.close();
      server.send(200, "text/plain", "File uploaded successfully.");

      // アップロードフラグをセット
      fileUploaded = true;
    } else {
      server.send(500, "text/plain", "Failed to save file.");
    }

  } else if (upload.status == UPLOAD_FILE_ABORTED) {
    if (file) {
      file.close();
      SD.remove(filePath);
    }
    server.send(500, "text/plain", "Upload aborted.");
  }
}

// ファイルアップロードの状態を確認
bool WebAPI::isFileUploaded() {
  return fileUploaded;
}

// アップロードフラグをリセット
void WebAPI::resetFileUploadedFlag() {
  fileUploaded = false;
}