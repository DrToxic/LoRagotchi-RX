#include <Arduino.h>   // needed for PlatformIO
#include <Mesh.h>
#ifdef LRX_TANK_WIFI
// Keep the framework NVS dependency visible to PlatformIO's chain LDF.
#include <Preferences.h>
#endif

#include "TankMesh.h"
#include "TankLinks.h"
#ifdef NRF52_PLATFORM
#include <CustomLFS.h>
#endif

#ifdef DISPLAY_CLASS
  #include "../simple_repeater/UITask.h"
  static UITask ui_task(board, display);
  static bool display_ready = false;
#endif

#ifdef ETHERNET_ENABLED
  #define ETHERNET_CLI_BANNER "MeshCore Repeater CLI"
  #include <helpers/nrf52/EthernetCLI.h>
#endif

StdRNG fast_rng;
SimpleMeshTables tables;

TankMesh the_mesh(board, radio_driver, *new ArduinoMillis(), fast_rng, rtc_clock, tables);

void halt() {
  while (1) ;
}

static TankLinks tank_links;
#ifdef ETHERNET_ENABLED
static char ethernet_command[160];
#endif


#if defined(PIN_USER_BTN) && defined(_SEEED_SENSECAP_SOLAR_H_)
static unsigned long userBtnDownAt = 0;
#define USER_BTN_HOLD_OFF_MILLIS 1500
#endif

void setup() {
  Serial.begin(115200);
  delay(1000);

  board.begin();

#ifdef HAS_EXTERNAL_WATCHDOG
  external_watchdog.begin();
#endif

#if defined(MESH_DEBUG) && defined(NRF52_PLATFORM)
  // give some extra time for serial to settle so
  // boot debug messages can be seen on terminal
  delay(5000);
#endif

#ifdef DISPLAY_CLASS
  if ((display_ready = display.begin())) {
    display.startFrame();
    display.setCursor(0, 0);
    display.print("Please wait...");
    display.endFrame();
  }
#endif

  if (!radio_init()) {
    MESH_DEBUG_PRINTLN("Radio init failed!");
    halt();
  }

  fast_rng.begin(radio_driver.getRngSeed());

  FILESYSTEM* fs;
#if defined(NRF52_PLATFORM) || defined(STM32_PLATFORM)
  if (!InternalFS.Adafruit_LittleFS::begin()) halt();
  fs = &InternalFS;
  IdentityStore store(InternalFS, "");
#elif defined(ESP32)
  if (!SPIFFS.begin(false)) halt();
  fs = &SPIFFS;
  IdentityStore store(SPIFFS, "/identity");
#elif defined(RP2040_PLATFORM)
  LittleFS.begin();
  fs = &LittleFS;
  IdentityStore store(LittleFS, "/identity");
  store.begin();
#else
  #error "need to define filesystem"
#endif
  if (!store.load("_main", the_mesh.self_id)) {
    MESH_DEBUG_PRINTLN("Generating new keypair");
    the_mesh.self_id = radio_new_identity();   // create new random identity
    int count = 0;
    while (count < 10 && (the_mesh.self_id.pub_key[0] == 0x00 || the_mesh.self_id.pub_key[0] == 0xFF)) {  // reserved id hashes
      the_mesh.self_id = radio_new_identity(); count++;
    }
    store.save("_main", the_mesh.self_id);
  }

  Serial.print("Repeater ID: ");
  mesh::Utils::printHex(Serial, the_mesh.self_id.pub_key, PUB_KEY_SIZE); Serial.println();


#ifdef ETHERNET_ENABLED
  ethernet_command[0] = 0;
#endif

  sensors.begin();

  the_mesh.begin(fs);
  FILESYSTEM* tank_fs = fs;
#if defined(NRF52_PLATFORM)
  static CustomLFS ExtraFS(0xD4000, 0x19000, 128);
  tank_fs = ExtraFS.Adafruit_LittleFS::begin() ? &ExtraFS : nullptr;
#endif
  the_mesh.beginTank(tank_fs);

#ifdef DISPLAY_CLASS
  if (display_ready) ui_task.begin(the_mesh.getNodePrefs(), FIRMWARE_BUILD_DATE, FIRMWARE_VERSION);
#endif

#ifdef ETHERNET_ENABLED
  ethernet_start_task();
#endif

  // send out initial zero hop Advertisement to the mesh
#if ENABLE_ADVERT_ON_BOOT == 1
  the_mesh.sendSelfAdvertisement(16000, false);
#endif

  tank_links.begin(the_mesh);
  board.onBootComplete();
}

void loop() {
  tank_links.loop(the_mesh);

  board.loop();   // let the board feed its watchdog, run periodic housekeeping

#ifdef ETHERNET_ENABLED
  ethernet_loop_maintain();
  if (ethernet_read_line(ethernet_command, sizeof(ethernet_command))) {
    char reply[160];
    reply[0] = 0;
    if (!ethernet_handle_command(ethernet_command, reply)) {
      the_mesh.handleCommand(0, ethernet_command, reply);
    }
    ethernet_send_reply(reply);
    ethernet_command[0] = 0;
  }
#endif

#if defined(PIN_USER_BTN) && defined(_SEEED_SENSECAP_SOLAR_H_) && !defined(DISPLAY_CLASS)
  // Hold the user button to power off the SenseCAP Solar repeater.
  int btnState = digitalRead(PIN_USER_BTN);
  if (btnState == LOW) {
    if (userBtnDownAt == 0) {
      userBtnDownAt = millis();
    } else if ((unsigned long)(millis() - userBtnDownAt) >= USER_BTN_HOLD_OFF_MILLIS) {
      Serial.println("Powering off...");
      board.powerOff();  // does not return
    }
  } else {
    userBtnDownAt = 0;
  }
#endif

  the_mesh.loop();
  the_mesh.tickTank();
  sensors.loop();
#ifdef DISPLAY_CLASS
  if (display_ready && !tank_links.showPin()) ui_task.loop();
#endif
  rtc_clock.tick();

#ifdef HAS_EXTERNAL_WATCHDOG
  external_watchdog.loop();
#endif
  // A tank endpoint must service connections and durable return deadlines even
  // while the host is absent. Preserve the stored preference, but do not enter
  // the stock 30-second idle sleep in these continuously available profiles.
}
