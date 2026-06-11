#pragma once
// Copy this file to wifi_secrets.h (which is gitignored) and fill in your
// network. The build also works without it — the firmware just stays
// BLE-only until credentials are provided.
#define BUDDY_WIFI_SSID  "your-ssid"
#define BUDDY_WIFI_PASS  "your-password"

// mDNS hostname the device advertises; flash later with:
//   pio run -e geekmagic-smalltv-pro-ota -t upload
#define BUDDY_OTA_HOSTNAME "claude-buddy"

// Optional: require this password on OTA uploads (leave empty for none).
#define BUDDY_OTA_PASS   ""
