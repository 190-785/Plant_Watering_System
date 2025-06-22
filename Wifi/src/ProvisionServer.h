#pragma once
#include <ESP8266WebServer.h>
#include "Config.h"

class ProvisionServer {
public:
  ProvisionServer(ESP8266WebServer &server);
  void begin();
  void handle();
private:
  ESP8266WebServer &server;
  void handleRoot();
  void handleSave();
};
