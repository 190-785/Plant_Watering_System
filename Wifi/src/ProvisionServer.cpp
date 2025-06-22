#include "ProvisionServer.h"
#include <ESP8266WiFi.h>
#include <LittleFS.h>

ProvisionServer::ProvisionServer(ESP8266WebServer &srv): server(srv) {}

void ProvisionServer::begin() {
  WiFi.softAP("Config-AP");
  Serial.printf("Access Point started: Config-AP\n");
  Serial.printf("Connect to WiFi 'Config-AP' and go to: http://%s\n", WiFi.softAPIP().toString().c_str());
  server.on("/", HTTP_GET, [&](){ handleRoot(); });
  server.on("/save", HTTP_POST, [&](){ handleSave(); });
  server.begin();
}

void ProvisionServer::handle() {
  server.handleClient();
}

void ProvisionServer::handleRoot() {
  String page = "<h1>Network Configuration</h1>";
  int n = WiFi.scanNetworks();
  page += "<form action='/save' method='POST'>SSID:<select name='ssid'>";
  for (int i = 0; i < n; ++i) {
    page += "<option value='" + WiFi.SSID(i) + "'>" + WiFi.SSID(i) + "</option>";
  }
  page += "</select><br>";  page += "Wi-Fi Password: <input type='password' name='wifipass'><br>";
  page += "<label><input type='checkbox' name='doubleAuth'> Double Auth Portal</label><br>";
  page += "<div id='portal' style='display:none;'>";
  page += "Portal URL: <input name='portalurl' placeholder='http://172.16.1.1/' style='width:250px;'><br>";
  page += "Portal User: <input name='portaluser'><br>";
  page += "Portal Pass: <input type='password' name='portalpass'><br>";
  page += "</div>";
  page += R"(<script>document.querySelector('[name=doubleAuth]').onchange=e=>{document.getElementById('portal').style.display = e.target.checked?'block':'none';}</script>)";
  page += "<button type='submit'>Save & Reboot</button></form>";
  server.send(200, "text/html", page);
}

void ProvisionServer::handleSave() {
  Config cfg;  cfg.ssid       = server.arg("ssid");
  cfg.wifipass   = server.arg("wifipass");
  cfg.doubleAuth = server.hasArg("doubleAuth");
  cfg.portalUrl  = server.arg("portalurl");
  cfg.portalUser = server.arg("portaluser");
  cfg.portalPass = server.arg("portalpass");
  saveConfig(cfg);
  server.send(200, "text/html", "<h2>Saved! Rebooting…</h2>");
  delay(1500);
  ESP.restart();
}
