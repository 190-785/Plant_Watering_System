#include "PortalLogin.h"
#include <ESP8266HTTPClient.h>

bool needsPortalLogin(WiFiClient &client) {
  HTTPClient http;
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.begin(client, "http://clients3.google.com/generate_204");
  int code = http.GET(); http.end();
  return code != 204;
}

PortalParams fetchPortalForm(WiFiClient &client, const char* portalUrl) {
  HTTPClient http; PortalParams p;
  Serial.printf("Fetching portal form from: %s\n", portalUrl);
  
  http.begin(client, portalUrl);
  int code = http.GET();
  Serial.printf("Portal GET response: %d\n", code);
  
  if (code == 200) {
    p.cookie = http.header("Set-Cookie");
    Serial.printf("Cookie: %s\n", p.cookie.c_str());
    
    String body = http.getString();
    Serial.printf("Body length: %d\n", body.length());
    
    // Look for various token patterns
    int i1 = body.indexOf("name=\"token\" value=\"");
    if (i1 > 0) {
      i1 += 20; // length of "name=\"token\" value=\""
      int i2 = body.indexOf('"', i1);
      p.hiddenToken = body.substring(i1, i2);
      Serial.printf("Found token: %s\n", p.hiddenToken.c_str());
    } else {
      // Try other common token patterns
      i1 = body.indexOf("name='token' value='");
      if (i1 > 0) {
        i1 += 20;
        int i2 = body.indexOf("'", i1);
        p.hiddenToken = body.substring(i1, i2);
        Serial.printf("Found token (single quotes): %s\n", p.hiddenToken.c_str());
      }
    }
    
    p.actionUrl = portalUrl;
    
    // Debug: Print first 200 chars of body
    Serial.printf("Body preview: %s\n", body.substring(0, 200).c_str());
  }
  http.end(); 
  return p;
}

bool loginPortal(WiFiClient &client, const PortalParams &p, const char* user, const char* pass) {
  HTTPClient http;
  Serial.printf("Attempting portal login to: %s\n", p.actionUrl.c_str());
  Serial.printf("Username: %s\n", user);
  
  http.begin(client, p.actionUrl);
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  if (p.cookie.length() > 0) {
    http.addHeader("Cookie", p.cookie);
    Serial.printf("Using cookie: %s\n", p.cookie.c_str());
  }
  
  // Try different common field name combinations
  String data1 = "username=" + String(user) + "&password=" + String(pass);
  String data2 = "user=" + String(user) + "&pass=" + String(pass);
  String data3 = "login=" + String(user) + "&password=" + String(pass);
  
  if (p.hiddenToken.length() > 0) {
    data1 += "&token=" + p.hiddenToken;
    data2 += "&token=" + p.hiddenToken;
    data3 += "&token=" + p.hiddenToken;
  }
  
  Serial.printf("POST data: %s\n", data1.c_str());
  
  // Try first combination
  int code = http.POST(data1);
  Serial.printf("Portal POST response: %d\n", code);
  
  if (code != 302 && code != 200) {
    // Try second combination
    Serial.println("Trying alternative field names...");
    code = http.POST(data2);
    Serial.printf("Alternative POST response: %d\n", code);
    
    if (code != 302 && code != 200) {
      // Try third combination
      Serial.println("Trying third field combination...");
      code = http.POST(data3);
      Serial.printf("Third POST response: %d\n", code);
    }
  }
  
  // Print response body for debugging
  if (code == 200) {
    String response = http.getString();
    Serial.printf("Response body: %s\n", response.substring(0, 100).c_str());
  }
  
  http.end();
  bool success = (code == 302 || code == 200);
  Serial.printf("Login %s\n", success ? "SUCCESS" : "FAILED");
  return success;
}
