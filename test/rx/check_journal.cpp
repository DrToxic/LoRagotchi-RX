#include "examples/companion_radio/LoRagotchiRXJournal.h"
#include <cassert>
#include <iostream>
#include <vector>
struct Files : lrx::JournalFiles {
  std::vector<uint8_t> banks[2];
  bool torn=false, badReadback=false, badWrite=false;
  unsigned last=9;
  bool exists(unsigned bank) override { return !banks[bank].empty(); }
  bool read(unsigned bank, lrx::Journal& journal) override {
    if (banks[bank].size()!=sizeof(journal)) return false;
    memcpy(&journal,banks[bank].data(),sizeof(journal));
    if (badReadback && bank==last) journal.magic^=1;
    return true;
  }
  bool write(unsigned bank,const lrx::Journal& journal) override {
    last=bank;
    if (badWrite) return false;
    const auto* bytes=reinterpret_cast<const uint8_t*>(&journal);
    banks[bank].assign(bytes,bytes+(torn?sizeof(journal)/2:sizeof(journal)));
    return !torn;
  }
};
lrx::Journal make(uint32_t generation) {
  lrx::Journal journal{};journal.magic=0x3158524cUL;journal.version=1;
  journal.nextId=generation+1;journal.generation=generation;
  journal.checksum=lrx::checksum(journal);return journal;
}
int main() {
  Files files;lrx::JournalStore store(files);lrx::Journal loaded{};bool fresh=false;
  assert(!store.load(loaded,fresh) && fresh);
  auto a=make(1),b=make(2),c=make(3);
  assert(store.save(a) && files.last==0);
  files.torn=true;assert(!store.save(b) && files.last==1);
  lrx::JournalStore reboot(files);
  assert(reboot.load(loaded,fresh) && !fresh && loaded.generation==1);
  files.torn=false;assert(reboot.save(b) && files.last==1);
  files.badReadback=true;assert(!reboot.save(c) && files.last==0);
  files.badReadback=false;files.badWrite=true;
  assert(!reboot.save(c) && files.last==0); // never touch last verified bank 1
  files.badWrite=false;assert(reboot.save(c) && files.last==0);
  std::cout<<"PASS torn writes/readback failures preserve the last verified bank\n";
  files.banks[0][0]^=1;lrx::JournalStore fallback(files);
  assert(fallback.load(loaded,fresh) && loaded.generation==2 && !fresh);
  files.banks[1][0]^=1;lrx::JournalStore broken(files);
  assert(!broken.load(loaded,fresh) && !fresh);
  std::cout<<"PASS corrupt latest falls back; both corrupt fail closed, never fresh\n";
  files=Files{};auto old=make(UINT32_MAX-1),wrapped=make(0);
  files.write(0,old);files.write(1,wrapped);lrx::JournalStore wrap(files);
  assert(wrap.load(loaded,fresh) && loaded.generation==0);
  std::cout<<"PASS bank generations compare across uint32 rollover\n";
  wrapped.checksum^=1;assert(!wrap.save(wrapped));
  std::cout<<"PASS invalid journal is rejected before writing\n";
}
