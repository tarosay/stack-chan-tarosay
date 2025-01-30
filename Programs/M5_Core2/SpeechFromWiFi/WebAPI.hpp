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

  void handleRoot();
  void handleUpload();

public:
  WebAPI(int port = 80);

  void begin(const char* ssid, const char* password);
  void handleClient();
  void connectToWiFi(const char* ssid, const char* password);
  void disconnectWiFi();
  String getIPAddress() const;
  int getFileUploaded();
  void resetFileUploadedType();
  bool isReStart();
  bool isWavNG();
  void resetWavNG();
  int getOngen();
  void resetOngen();
};

#endif  // WEBAPI_HPP