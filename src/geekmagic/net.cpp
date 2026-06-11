#include "net.h"
#if defined(BUDDY_WIFI)

#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoOTA.h>
#include <string.h>
#include <esp_ota_ops.h>

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
  // If we arrived here via an OTA that has rollback armed (e.g. flashed from
  // ESPHome), confirm this image so the bootloader keeps it across reboots.
  // Harmless no-op when the partition isn't in the pending-verify state.
  esp_ota_mark_app_valid_cancel_rollback();

  if (strlen(BUDDY_WIFI_SSID) == 0) return;   // no creds -> remain BLE-only
  // IMPORTANT: associate with modem sleep OFF. With BLE already up on the
  // shared 2.4GHz radio, enabling WiFi modem-sleep *before* association makes
  // the auth/assoc handshake starve and silently fail. We turn coexistence
  // sleep back on only after the link is up (see netLoop).
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(true);
  WiFi.begin(BUDDY_WIFI_SSID, BUDDY_WIFI_PASS);   // non-blocking
  s_enabled = true;
  Serial.printf("net: connecting to \"%s\"...\n", BUDDY_WIFI_SSID);
}

void netLoop() {
  if (!s_enabled) return;

  if (WiFi.status() != WL_CONNECTED) {
    s_otaBegun = false;
    // Heartbeat so a serial log shows whether assoc is progressing or stuck.
    static uint32_t lastLog = 0;
    if (millis() - lastLog > 3000) {
      lastLog = millis();
      Serial.printf("net: WiFi status=%d (waiting)\n", (int)WiFi.status());
    }
    return;
  }

  if (!s_otaBegun) {
    // Now that we're associated, hand the radio back to BLE between beacons.
    WiFi.setSleep(true);
    // ArduinoOTA's listener / mDNS need the link up before begin().
    ArduinoOTA.setHostname(BUDDY_OTA_HOSTNAME);
    if (strlen(BUDDY_OTA_PASS) > 0) ArduinoOTA.setPassword(BUDDY_OTA_PASS);
    ArduinoOTA.begin();
    s_otaBegun = true;
    Serial.printf("net: OTA ready at %s / %s.local\n",
                  WiFi.localIP().toString().c_str(), BUDDY_OTA_HOSTNAME);
  }
  ArduinoOTA.handle();
}

#endif  // BUDDY_WIFI
