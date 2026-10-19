#pragma once
#include "LoRagotchiRX.h"

namespace lrx {
// Filesystem adapter supplies whole-bank operations. Never overwrite the last
// validated bank, even after a short write, failed readback or reboot.
struct JournalFiles {
  virtual bool exists(unsigned bank) = 0;
  virtual bool read(unsigned bank, Journal& journal) = 0;
  virtual bool write(unsigned bank, const Journal& journal) = 0;
  virtual ~JournalFiles() {}
};
class JournalStore {
  JournalFiles& files;
  int active = -1;
public:
  explicit JournalStore(JournalFiles& backend) : files(backend) {}
  bool load(Journal& result, bool& fresh) {
    Journal a{}, b{};
    const bool va = files.read(0, a) && valid(a);
    const bool vb = files.read(1, b) && valid(b);
    fresh = !files.exists(0) && !files.exists(1);
    active = -1;
    if (va && (!vb || int32_t(a.generation - b.generation) > 0)) {
      result = a;
      active = 0;
    } else if (vb) {
      result = b;
      active = 1;
    }
    return active >= 0;
  }
  bool save(const Journal& journal) {
    const unsigned bank = active == 0 ? 1 : 0;
    if (!valid(journal) || !files.write(bank, journal)) return false;
    Journal verify{};
    if (!files.read(bank, verify) || !valid(verify) ||
        memcmp(&journal, &verify, sizeof(journal))) return false;
    active = bank;
    return true;
  }
};
} // namespace lrx
