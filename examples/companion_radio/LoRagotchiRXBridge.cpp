#ifdef LORAGOTCHI_RX
#include "MyMesh.h"
#include "LoRagotchiRX.h"
#include "LoRagotchiRXJournal.h"
#include "LoRagotchiRXBridge.h"
#if !defined(ESP32) && !defined(NRF52_PLATFORM)
#error "RX persistence supports ESP32 SPIFFS and nRF52 LittleFS targets"
#endif
extern MyMesh the_mesh;
extern DataStore store;
namespace {
const char* paths[] = {"/lrx-a.dat", "/lrx-b.dat"};
FILESYSTEM* filesystem() {
  // nRF52 uses the existing extra filesystem alongside contacts, keeping the
  // smaller identity/settings region free. ESP32 uses its normal SPIFFS.
  auto* extra = store.getSecondaryFS();
  return extra ? extra : store.getPrimaryFS();
}
struct Files : lrx::JournalFiles {
  bool exists(unsigned bank) override { return filesystem()->exists(paths[bank]); }
  bool read(unsigned bank, lrx::Journal& journal) override {
    auto file = store.openRead(filesystem(), paths[bank]);
    if (!file) return false;
    const bool ok = file.size() == sizeof(journal) &&
        file.read(reinterpret_cast<uint8_t*>(&journal), sizeof(journal)) == sizeof(journal);
    file.close();
    return ok;
  }
  bool write(unsigned bank, const lrx::Journal& journal) override {
#if defined(NRF52_PLATFORM)
    // Adafruit FILE_O_WRITE does not truncate. Remove only the inactive bank.
    if (exists(bank) && !filesystem()->remove(paths[bank])) return false;
    auto file = filesystem()->open(paths[bank], FILE_O_WRITE);
#else
    auto file = filesystem()->open(paths[bank], "w");
#endif
    if (!file) return false;
    const size_t written = file.write(reinterpret_cast<const uint8_t*>(&journal), sizeof(journal));
    file.flush();
    file.close();
    return written == sizeof(journal);
  }
} files;
lrx::JournalStore journalStore(files);
struct Backend : lrx::IO {
  bool save(const lrx::Journal& journal) override { return journalStore.save(journal); }
  bool send(const uint8_t owner[32], const char* text, uint32_t& ack) override {
    auto* contact = the_mesh.lookupContactByPubKey(owner, 32);
    if (!contact) return false;
    uint32_t timeout = 0;
    return the_mesh.sendMessage(*contact, the_mesh.getRTCClock()->getCurrentTime(),
                               0, text, ack, timeout) >= 0;
  }
} backend;
lrx::Receiver receiver(backend);
}
void loragotchiRXBegin() {
  lrx::Journal saved{};
  bool fresh = false;
  const bool loaded = journalStore.load(saved, fresh);
  receiver.begin(loaded ? &saved : nullptr, millis(), fresh);
}
void loragotchiRXStatus(uint8_t& uploads, uint8_t& returns, uint8_t& failed, bool& healthy) {
  healthy = receiver.healthy(); uploads = returns = failed = 0;
  for (const auto& slot : receiver.data.slots) {
    if (slot.state >= lrx::Captured && !slot.uploaded) ++uploads;
    if (slot.state == lrx::Captured) ++returns;
    if (slot.state == lrx::Unresolved) ++failed;
  }
}
void loragotchiRXTick() { receiver.tick(millis()); }
bool loragotchiRXReceive(const uint8_t* owner, const char* text) {
  return receiver.receive(owner, text, millis());
}
void loragotchiRXAck(uint32_t hash) { receiver.acknowledgeRadio(hash); }
size_t loragotchiRXCommand(const uint8_t* in, size_t len, uint8_t* out, const uint8_t* identity) {
  uint8_t transports = 0;
#ifdef ENABLE_USB_INTERFACE
  transports |= 1;
#endif
#ifdef BLE_PIN_CODE
  transports |= 2;
#endif
  return receiver.command(in, len, out, millis(), identity, transports);
}
#endif
