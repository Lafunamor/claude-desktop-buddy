#include "net.h"
#if defined(BUDDY_WIFI)

#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoOTA.h>
#include <string.h>

// Credentials live in an untracked header so they never land in git. Build
// works with or without it; with no SSID the firmware simply stays BLE-only.
#if __has_include("wifi_secrets.h")
  #include "wifi_secrets.h"
#endif
#ifndef BUDDY_WIFI_SSID
  #define BUDDY_WIFI_SSID ""
#endif
#ifndef BUDDY_WIFI_PASS
  #define BUDDY_WIFI_PASS ""
#endif
#ifndef BUDDY_OTA_HOSTNAME
  #define BUDDY_OTA_HOSTNAME "claude-buddy"
#endif
#ifndef BUDDY_OTA_PASS
  #define BUDDY_OTA_PASS ""   // set non-empty to require a password on upload
#endif

static bool s_enabled  = false;   // creds present, WiFi started
static bool s_otaBegun = false;   // ArduinoOTA listener up (needs WiFi first)

void netInit() {
  if (strlen(BUDDY_WIFI_SSID) == 0) return;   // no creds -> remain BLE-only
  // STA + modem sleep so WiFi and BLE can share the single 2.4GHz radio.
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(true);
  WiFi.setAutoReconnect(true);
  WiFi.begin(BUDDY_WIFI_SSID, BUDDY_WIFI_PASS);   // non-blocking
  s_enabled = true;
}

void netLoop() {
  if (!s_enabled) return;
  if (WiFi.status() != WL_CONNECTED) { s_otaBegun = false; return; }

  if (!s_otaBegun) {
    // Defer until associated: ArduinoOTA's listener / mDNS need the link up.
    ArduinoOTA.setHostname(BUDDY_OTA_HOSTNAME);
    if (strlen(BUDDY_OTA_PASS) > 0) ArduinoOTA.setPassword(BUDDY_OTA_PASS);
    ArduinoOTA.begin();
    s_otaBegun = true;
    Serial.printf("net: OTA ready at %s / %s\n",
                  WiFi.localIP().toString().c_str(), BUDDY_OTA_HOSTNAME);
  }
  ArduinoOTA.handle();
}

#endif  // BUDDY_WIFI
