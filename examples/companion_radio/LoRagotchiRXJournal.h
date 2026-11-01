#pragma once
#include "LoRagotchiRX.h"

namespace lrx {
// Filesystem adapter supplies whole-bank operations. Never overwrite the last
// validated bank, even after a short write, failed readback or reboot.
struct JournalFiles {
  virtual bool exists(unsigned bank) = 0;
  virtual bool read(unsigned bank, Journal& journal) = 0;
  virtual bool write(unsigned bank, const Journal& journal) = 0;
  virtual bool readV1(unsigned, JournalV1&) { return false; }
  virtual bool header(unsigned, uint32_t&, uint32_t&) { return false; }
  virtual ~JournalFiles() {}
};
class JournalStore {
  JournalFiles& files;
  int active = -1;
  bool readCompatible(unsigned bank,Journal& out) {
    if(files.read(bank,out) && valid(out))return true;
    JournalV1 old{};if(!files.readV1(bank,old) || !validV1(old))return false;
    out=Journal{};out.magic=old.magic;out.version=2;out.generation=old.generation;out.nextId=old.nextId;
    for(int i=0;i<CAPACITY;i++)memcpy(&out.slots[i],&old.slots[i],sizeof(SlotV1));
    memcpy(out.recent,old.recent,sizeof(old.recent));out.cursor=old.cursor;out.checksum=checksum(out);return true;
  }
public:
  explicit JournalStore(JournalFiles& backend) : files(backend) {}
  bool load(Journal& result, bool& fresh) {
    Journal a{}, b{};
    for(unsigned i=0;i<2;++i) { uint32_t magic=0,version=0;
      if(files.header(i,magic,version) && magic==0x3158524cUL && version>2) {fresh=false;active=-1;return false;}
    }
    const bool va = readCompatible(0, a);
    const bool vb = readCompatible(1, b);
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
