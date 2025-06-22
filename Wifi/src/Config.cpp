#include "Config.h"
#include <LittleFS.h>

static const char* CONFIG_PATH = "/config.json";

bool loadConfig(Config &cfg) {
  if (!LittleFS.begin()) return false;
  if (!LittleFS.exists(CONFIG_PATH)) return false;
  File f = LittleFS.open(CONFIG_PATH, "r");
  if (!f) return false;

  StaticJsonDocument<512> doc;
  DeserializationError err = deserializeJson(doc, f);
  f.close();
  if (err) return false;

  cfg.ssid       = doc["ssid"].as<String>();
  cfg.wifipass   = doc["wifipass"].as<String>();
  cfg.doubleAuth = doc["doubleAuth"].as<bool>();
  cfg.portalUrl  = doc["portalUrl"].as<String>();
  cfg.portalUser = doc["portalUser"].as<String>();
  cfg.portalPass = doc["portalPass"].as<String>();
  return true;
}

void saveConfig(const Config &cfg) {
  StaticJsonDocument<512> doc;
  doc["ssid"]       = cfg.ssid;
  doc["wifipass"]   = cfg.wifipass;
  doc["doubleAuth"] = cfg.doubleAuth;
  doc["portalUrl"]  = cfg.portalUrl;
  doc["portalUser"] = cfg.portalUser;
  doc["portalPass"] = cfg.portalPass;

  File f = LittleFS.open(CONFIG_PATH, "w");
  if (!f) return;
  serializeJson(doc, f);
  f.close();
}
