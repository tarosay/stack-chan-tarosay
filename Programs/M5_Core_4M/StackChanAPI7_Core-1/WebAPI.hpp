#ifndef WEBAPI_HPP
#define WEBAPI_HPP

#include <WiFi.h>
#include <WebServer.h>
#include <SD.h>

class WebAPI {
private:
  WebServer server;
  String ipAddress;
  bool serverStarted;
  int fileUploaded;
  bool restart;
  bool wavng;
  int ongen;
  float spVolume;
  String uploadfilename;
  bool isMove;
  bool isSpeech;
  char* wavFilename;

  void handleRoot();
  void handleUpload();

public:
  struct MoveXY {
    int x;
    int y;
    // コンストラクタで初期値を設定
    MoveXY()
      : x(0), y(0) {}
  };
  MoveXY moveXY;  // 構造体の変数を用意

  WebAPI(int port = 80);

  void begin(const char* ssid, const char* password);
  void handleClient();
  void connectToWiFi(const char* ssid, const char* password);
  void disconnectWiFi();
  String getIPAddress() const;
  String getUploadFilename() const;
  int getFileUploaded();
  void resetFileUploadedType();
  bool isReStart();
  bool isWavNG();
  void resetWavNG();
  int getOngen();
  void resetOngen();
  float getVolume();
  MoveXY getMoveXY();
  bool getIsMove();
  bool getIsSpeech();
  const char* getWavFilename();
};

#endif  // WEBAPI_HPP