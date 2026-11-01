#pragma once
#ifdef LRX_TANK_WIFI
#if !defined(ESP32)
#error "Onboard tank Wi-Fi currently requires an ESP32 target"
#endif
#include <WiFi.h>
#include <Preferences.h>
#include <esp_system.h>
#include <mbedtls/md.h>
#include <lwip/sockets.h>
#include <errno.h>
#include "TankAuth.h"
#include "TankWire.h"

class TankWiFi {
  WiFiServer server{8765};
  WiFiClient client;
  uint8_t token[32] = {};
  bool enabled = false;
  uint32_t reconnectAt = 0;
  lrx::TankAuth auth;
  lrx::TankWire wire;
  lrx::TankReply reply;
  bool transmit(const uint8_t* data, size_t length, size_t& sent) {
    // WiFiClient::write can wait repeatedly for socket space. Never let a
    // stalled aquarium block the radio scheduler: use a nonblocking send.
    int n = ::send(client.fd(), data, length, MSG_DONTWAIT);
    if (n > 0) { sent = n; return true; }
    sent = 0;
    return n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK);
  }
  void close() { client.stop(); wire.reset(); reply.clear(); }
public:
  bool active() const { return enabled; }
  void begin() {
    Preferences prefs;
    if (!prefs.begin("lrx-net", true)) return;
    String ssid = prefs.getString("ssid", ""), password = prefs.getString("password", "");
    enabled = prefs.getBool("enabled", false) && ssid.length() && password.length() >= 8 &&
              prefs.getBytesLength("token") == 32 && prefs.getBytes("token", token, 32) == 32;
    prefs.end();
    if (!enabled) return; // no AP, no default credential, no anonymous listener
    WiFi.mode(WIFI_STA); WiFi.setAutoReconnect(true);
    WiFi.begin(ssid.c_str(), password.c_str()); // ESP32 asynchronous association
    reconnectAt = millis() + 30000;
    server.begin();
  }
  // Physical USB only. Values are deliberately never echoed or returned.
  // Changes apply on reboot, so provisioning cannot reset an active return.
  bool configure(const char* command, char* response) {
    if (strncmp(command, "tank ", 5)) return false;
    Preferences prefs;
    if (!prefs.begin("lrx-net", false)) { strcpy(response, "Tank settings unavailable"); return true; }
    bool ok = false;
    if (!strncmp(command, "tank ssid ", 10)) {
      size_t n = strlen(command + 10);
      if (n && n <= 32) ok = prefs.putString("ssid", command + 10) == n;
    } else if (!strncmp(command, "tank password ", 14)) {
      size_t n = strlen(command + 14);
      if (n >= 8 && n <= 63) ok = prefs.putString("password", command + 14) == n;
    } else if (!strncmp(command, "tank token ", 11)) {
      const char* hex = command + 11; uint8_t key[32] = {}; bool valid = strlen(hex) == 64;
      for (unsigned i = 0; valid && i < 64; ++i) {
        int digit = hex[i] >= '0' && hex[i] <= '9' ? hex[i]-'0' :
                    hex[i] >= 'a' && hex[i] <= 'f' ? hex[i]-'a'+10 :
                    hex[i] >= 'A' && hex[i] <= 'F' ? hex[i]-'A'+10 : -1;
        if (digit < 0) valid = false; else key[i/2] |= digit << (i % 2 ? 0 : 4);
      }
      if (valid) ok = prefs.putBytes("token", key, 32) == 32;
      memset(key, 0, sizeof(key));
    } else if (!strcmp(command, "tank wifi on")) ok = prefs.putBool("enabled", true) == 1;
    else if (!strcmp(command, "tank wifi off")) ok = prefs.putBool("enabled", false) == 1;
    prefs.end(); strcpy(response, ok ? "Tank setting saved; reboot to apply" : "Invalid tank setting"); return true;
  }
  template<class Mesh> void loop(Mesh& mesh) {
    if (!enabled) return;
    uint32_t now = millis();
    if (WiFi.status() != WL_CONNECTED) {
      close();
      if (int32_t(now-reconnectAt) >= 0) { reconnectAt = now + 30000; WiFi.reconnect(); }
      return;
    }
    auto incoming = server.available();
    if (incoming) {
      if (client && client.connected()) incoming.stop(); // never replace owner
      else {
        close(); client = incoming; client.setNoDelay(true);
        uint8_t nonce[32], expected[32]; esp_fill_random(nonce, sizeof(nonce));
        const auto* md = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
        if (!md || mbedtls_md_hmac(md, token, sizeof(token), nonce, sizeof(nonce), expected)) { close(); return; }
        auth.begin(nonce, expected, now); memset(expected, 0, sizeof(expected));
      }
    }
    if (!client || !client.connected()) { close(); return; }
    if (!auth.ready()) {
      if (auth.timedOut(now) || auth.rejected()) { close(); return; }
      if (auth.remaining()) {
        size_t n = 0;
        if (!transmit(auth.bytes(), auth.remaining(), n)) { close(); return; }
        auth.sent(n);
      }
      for (unsigned budget = 0; auth.wantsProof() && client.available() && budget < 32; ++budget)
        auth.feed(client.read());
      return;
    }
    if (reply.pending()) {
      size_t n = 0;
      if (!transmit(reply.bytes(), reply.remaining(), n)) { close(); return; }
      reply.sent(n); return;
    }
    wire.expire(now);
    for (unsigned budget = 0; client.available() && budget < 64; ++budget) {
      if (wire.feed(client.read(), now, false) == lrx::TankWire::Frame) {
        uint8_t output[20]; size_t size = mesh.command(wire.frame, wire.size, output, 4);
        if (!size) { output[0] = 1; output[1] = 6; size = 2; }
        reply.set(output, size); break;
      }
    }
  }
};
#endif
