#pragma once
#include "Config.h"
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>

struct PortalParams { String actionUrl, cookie, hiddenToken; };

bool needsPortalLogin(WiFiClient &client);
PortalParams fetchPortalForm(WiFiClient &client, const char* portalUrl);
bool loginPortal(WiFiClient &client, const PortalParams &p, const char* user, const char* pass);
