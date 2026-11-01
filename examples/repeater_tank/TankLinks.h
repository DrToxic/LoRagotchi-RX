#pragma once
#include "TankWire.h"
#include "TankWiFi.h"
#if (defined(LRX_TANK_USB) + defined(LRX_TANK_BLE) + defined(LRX_TANK_WIFI)) != 1
#error "Choose exactly one tank carrier; stock repeater CLI remains on USB"
#endif
#if defined(WITH_RS232_BRIDGE) || defined(WITH_ESPNOW_BRIDGE) || defined(ETHERNET_ENABLED)
#error "Tank profiles have not qualified simultaneous bridge hardware"
#endif
#ifdef LRX_TANK_BLE
#include "../companion_radio/LoRagotchiBLEPin.h"
#ifdef ESP32
#include <helpers/esp32/SerialBLEInterface.h>
#elif defined(NRF52_PLATFORM)
#include <helpers/nrf52/SerialBLEInterface.h>
#else
#error "Tank BLE requires an ESP32 or nRF52 target"
#endif
extern RADIO_CLASS radio;
#endif

class TankLinks {
  lrx::TankWire usb;
  lrx::TankReply usbReply;
#ifdef LRX_TANK_BLE
  SerialBLEInterface ble;
  lrx::TankReply bleReply;
  uint32_t pin = 0, pinShownAt = 0;
  bool pinPainted = false;
#endif
#ifdef LRX_TANK_WIFI
  TankWiFi wifi;
#endif
public:
  void begin(TankMesh& mesh) {
#ifdef LRX_TANK_BLE
    RadioNoiseListener random(radio);
#ifndef LRX_TANK_PIN
#define LRX_TANK_PIN 123456
#endif
    static_assert(LRX_TANK_PIN >= 100000 && LRX_TANK_PIN <= 999999, "Tank BLE PIN must be six digits");
    pin = LoRagotchiBLE::selectPin(0, LRX_TANK_PIN, random);
    char name[32]; strlcpy(name, mesh.getNodePrefs()->node_name, sizeof(name));
    ble.begin("LRX-", name, pin); ble.enable(); pinShownAt = millis();
    Serial.printf("Tank BLE PIN: %06lu\n", (unsigned long)pin);
#endif
#ifdef LRX_TANK_WIFI
    wifi.begin();
#endif
  }
  bool showPin() {
#if defined(LRX_TANK_BLE) && defined(DISPLAY_CLASS)
    if (uint32_t(millis() - pinShownAt) < 15000) {
      if (!pinPainted) {
        char label[32]; snprintf(label, sizeof(label), "PIN: %06lu", (unsigned long)pin);
        display.startFrame(); display.setTextSize(1);
        display.drawTextCentered(display.width()/2, 16, "Tank BLE pairing");
        display.drawTextCentered(display.width()/2, 32, label);
        display.drawTextCentered(display.width()/2, 48, "USB: tank pin"); display.endFrame();
        pinPainted = true;
      }
      return true;
    }
#endif
    return false;
  }
  void loop(TankMesh& mesh) {
    uint32_t now = millis();
    // Never wait for a host that has stopped draining USB. Binary replies are
    // retained with partial-write offsets, and no new request is consumed yet.
    if (usbReply.pending()) {
      int available = Serial.availableForWrite();
      size_t count = available > 0 ? size_t(available) : 0;
      if (count > usbReply.remaining()) count = usbReply.remaining();
      if (count) usbReply.sent(Serial.write(usbReply.bytes(), count));
    } else {
      usb.expire(now);
      for (unsigned budget = 0; Serial.available() && budget < 64; ++budget) {
        auto result = usb.feed(Serial.read(), now, true);
        if (result == lrx::TankWire::Frame) {
#ifdef LRX_TANK_USB
          uint8_t output[20]; size_t size = mesh.command(usb.frame, usb.size, output, 1);
          if (!size) { output[0] = 1; output[1] = 6; size = 2; }
          usbReply.set(output, size);
#endif
          break;
        }
        if (result == lrx::TankWire::Line) {
          char response[160] = {};
          bool handled = false;
#ifdef LRX_TANK_BLE
          if (!strcmp(usb.line, "tank pin")) {
            snprintf(response, sizeof(response), "Tank BLE PIN: %06lu", (unsigned long)pin);
            pinShownAt = now; pinPainted = false; handled = true;
          }
#endif
#ifdef LRX_TANK_WIFI
          handled = wifi.configure(usb.line, response);
#endif
          if (!handled) mesh.handleCommand(0, usb.line, response);
          if (response[0]) { Serial.print("  -> "); Serial.println(response); }
          // Do not retain provisioning credentials in the CLI scratch buffer.
          memset(usb.line, 0, sizeof(usb.line)); break;
        }
      }
    }
#ifdef LRX_TANK_BLE
    uint8_t input[MAX_FRAME_SIZE];
    size_t size = ble.checkRecvFrame(input); // also drains stock BLE send queue
    if (!bleReply.pending() && size) {
      uint8_t output[20]; size_t count = mesh.command(input, size, output, 2);
      if (!count) { output[0] = 1; output[1] = 6; count = 2; }
      bleReply.set(output, count, false);
    }
    if (bleReply.pending()) {
      if (!ble.isConnected()) bleReply.clear();
      else bleReply.sent(ble.writeFrame(bleReply.bytes(), bleReply.remaining()));
    }
#endif
#ifdef LRX_TANK_WIFI
    wifi.loop(mesh);
#endif
  }
};
