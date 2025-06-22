#pragma once
#include <ArduinoJson.h>
#include <FS.h>

struct Config {
  String ssid;
  String wifipass;
  bool doubleAuth;
  String portalUrl;
  String portalUser;
  String portalPass;
};

bool loadConfig(Config &cfg);
void saveConfig(const Config &cfg);
