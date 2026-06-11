#pragma once
// Optional WiFi + over-the-air update layer.
//
// Compiled in only when BUDDY_WIFI is defined (the GeekMagic SmallTV Pro
// env sets it). For the upstream M5StickC Plus build these are inline
// no-ops, so behavior there is byte-for-byte unchanged — the WiFi support
// is purely additive.
//
// The point: a board with no USB-serial chip (SmallTV Pro) can only be
// reached over the network. Once this firmware runs, it keeps an ArduinoOTA
// listener up so every subsequent flash is a wireless `espota` upload — no
// serial adapter ever required.
#if defined(BUDDY_WIFI)
void netInit();   // call once from setup()
void netLoop();   // call every loop()
#else
static inline void netInit() {}
static inline void netLoop() {}
#endif
