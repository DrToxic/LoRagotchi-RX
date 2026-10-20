#!/usr/bin/env python3
"""Exercise boot order + production channel accessors against persisted fixtures.

Only storage/default insertion and the SHA boundary are host substitutes. OpenSSL
provides real SHA256 (the general native test mocks deliberately do not).
"""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
source = (root / "examples/companion_radio/MyMesh.cpp").read_text()
shared = (root / "src/helpers/BaseChatMesh.cpp").read_text()


def function(signature):
    start = shared.index(signature)
    opening = shared.index("{", start)
    depth = 1
    end = opening + 1
    while depth:
        depth += (shared[end] == "{") - (shared[end] == "}")
        end += 1
    return shared[start:end]


start = source.index('  addChannel("Public", PUBLIC_GROUP_PSK);')
end = source.index("\n  radio_driver.setParams", start)
boot = source[start:end]
assert boot.count("LoRagotchiChannel::ensureJoined(*this)") == 1
assert source.count("LoRagotchiChannel::ensureJoined(*this)") == 1
assert source.index("_store->loadChannels(this);", start) < source.index(
    "LoRagotchiChannel::ensureJoined(*this)", start
)
program = r'''
#include <cassert>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <array>
#include <algorithm>
#include <Utils.h>
#define MAX_GROUP_CHANNELS 8
#define PUBLIC_GROUP_PSK "mock public default"
#define MESH_DEBUG_PRINTLN(...) ((void)0)
struct ChannelDetails {
  char name[32];
  struct { uint8_t secret[32]; uint8_t hash[1]; } channel;
};
struct BaseChatMesh {
  std::array<ChannelDetails, MAX_GROUP_CHANNELS> channels{};
  bool getChannel(int, ChannelDetails&);
  bool setChannel(int, const ChannelDetails&);
};
''' + function("bool BaseChatMesh::getChannel(") + "\n" + function(
    "bool BaseChatMesh::setChannel("
) + r'''
#include "examples/companion_radio/LoRagotchiChannel.h"
using Result = LoRagotchiChannel::Result;
ChannelDetails named(const char* name, uint8_t secret) {
  ChannelDetails c{};
  strncpy(c.name, name, sizeof(c.name));
  memset(c.channel.secret, secret, 16);
  return c;
}
ChannelDetails target(const char* label = LoRagotchiChannel::NAME) {
  auto c = named(label, 0);
  mesh::Utils::sha256(c.channel.secret, 16,
      (const uint8_t*)LoRagotchiChannel::NAME, strlen(LoRagotchiChannel::NAME));
  return c;
}
struct Store {
  std::array<ChannelDetails, MAX_GROUP_CHANNELS> disk{};
  bool exists = false, writable = true;
  unsigned writes = 0;
  void loadChannels(BaseChatMesh* mesh) {
    if (exists) for (int i = 0; i < MAX_GROUP_CHANNELS; ++i) mesh->setChannel(i, disk[i]);
  }
  void saveChannels(BaseChatMesh* mesh) {
    ++writes;
    if (writable) { disk = mesh->channels; exists = true; }
  }
};
struct MyMesh : BaseChatMesh {
  Store* _store;
  bool rejectSet = false;
  explicit MyMesh(Store& store) : _store(&store) {}
  void addChannel(const char* name, const char*) {
    BaseChatMesh::setChannel(0, named(name, 0x42));
  }
  bool setChannel(int i, const ChannelDetails& c) {
    return !rejectSet && BaseChatMesh::setChannel(i, c);
  }
private:
  void saveChannels() { _store->saveChannels(this); }
public:
  void boot() {
''' + boot + r'''
  }
};
bool same(const ChannelDetails& a, const ChannelDetails& b) {
  // Channel hash is derived, not stored in /channels2.
  return !memcmp(a.name, b.name, 32) && !memcmp(a.channel.secret, b.channel.secret, 32);
}
int main() {
  // Compare against an independently calculated protocol vector.
  const uint8_t expected[] = { EXPECTED_KEY };
  auto channel = target();
  assert(!memcmp(channel.channel.secret, expected, 16));
  Store blank;
  MyMesh first(blank);
  first.boot();
#if !defined(LORAFISH_UI) && !defined(LORAGOTCHI_RX)
  assert(blank.writes == 0 && !first.channels[1].name[0]);
  puts("PASS generic companion boot unchanged");
  return 0;
#else
  assert(blank.writes == 1 && same(first.channels[0], named("Public", 0x42)));
  assert(same(first.channels[1], channel));
  const uint8_t zeros[16] = {};
  assert(!memcmp(first.channels[1].channel.secret + 16, zeros, 16));
  uint8_t hash[1]; mesh::Utils::sha256(hash, 1, expected, 16);
  assert(hash[0] == first.channels[1].channel.hash[0]);
  puts("PASS fresh boot joins standard 128-bit hashtag key; Public preserved");
  for (int i = 0; i < 5; ++i) {
    MyMesh reboot(blank); reboot.boot();
    assert(same(reboot.channels[1], channel));
  }
  assert(blank.writes == 1);
  puts("PASS persisted restart is idempotent with no repeated flash writes");

  // Fully restored slots must win over the default addChannel counter.
  Store saved; saved.exists = true;
  for (int i = 0; i < 8; ++i) saved.disk[i] = named("Existing", uint8_t(i + 1));
  saved.disk[5] = {};
  auto original = saved.disk;
  MyMesh sparse(saved); sparse.boot();
  assert(saved.writes == 1 && same(sparse.channels[5], channel));
  for (int i = 0; i < 8; ++i) if (i != 5) assert(same(sparse.channels[i], original[i]));
  puts("PASS first genuinely empty slot used; saved Public/private/index layout preserved");

  Store joined; joined.exists = true; joined.disk[7] = target("#loragotchi-DX");
  MyMesh existing(joined); existing.boot(); assert(joined.writes == 0);
  assert(same(existing.channels[7], joined.disk[7]) && !existing.channels[0].name[0]);
  assert(LoRagotchiChannel::ensureJoined(existing) == Result::AlreadyJoined);
  puts("PASS complete scan finds existing alias after holes without duplicates or rename");

  Store full; full.exists = true;
  for (int i = 0; i < 8; ++i) full.disk[i] = named("Occupied", uint8_t(i + 1));
  MyMesh noRoom(full); noRoom.boot(); assert(full.writes == 0);
  assert(LoRagotchiChannel::ensureJoined(noRoom) == Result::Full);
  for (int i = 0; i < 8; ++i) assert(same(noRoom.channels[i], full.disk[i]));
  full.disk[3] = {}; MyMesh freed(full); freed.boot();
  assert(full.writes == 1 && same(freed.channels[3], channel));
  puts("PASS full table unchanged; freeing a slot enables join on next boot");

  Store occupied; occupied.exists = true;
  occupied.disk[0] = named("", 7); // secret-only slot is not empty
  occupied.disk[1] = named("Named zero key", 0);
  memset(occupied.disk[2].name, 'x', 32); // nonterminated on-disk name
  auto preserved = occupied.disk;
  MyMesh guarded(occupied); guarded.boot();
  for (int i = 0; i < 3; ++i) assert(same(guarded.channels[i], preserved[i]));
  assert(same(guarded.channels[3], channel));
  puts("PASS secret-only, named zero-key and unterminated-name slots preserved");

  Store conflict; conflict.exists = true; conflict.disk[4] = named(LoRagotchiChannel::NAME, 7);
  MyMesh wrongKey(conflict); wrongKey.boot(); assert(conflict.writes == 0);
  assert(LoRagotchiChannel::ensureJoined(wrongKey) == Result::NameConflict);
  assert(same(wrongKey.channels[4], conflict.disk[4]));
  puts("PASS same-name/different-key channel not silently changed");

  Store wide; wide.exists = true; wide.disk[0] = channel; wide.disk[0].channel.secret[31] = 1;
  strcpy(wide.disk[0].name, "256-bit private");
  MyMesh wideKey(wide); wideKey.boot();
  assert(wide.writes == 1 && same(wideKey.channels[1], channel));
  puts("PASS full 32-byte key identity, not prefix or radio hash");

  Store caseSensitive; caseSensitive.exists = true;
  auto uppercase = named("#loragotchi-DX", 0);
  mesh::Utils::sha256(uppercase.channel.secret, 16,
      (const uint8_t*)uppercase.name, strlen(uppercase.name));
  caseSensitive.disk[0] = uppercase;
  MyMesh lower(caseSensitive); lower.boot();
  assert(caseSensitive.writes == 1 && same(lower.channels[0], uppercase));
  assert(same(lower.channels[1], channel));
  puts("PASS differently cased hashtag key is distinct; exact lowercase channel added");

  Store blocked; MyMesh failed(blocked); failed.rejectSet = true; failed.boot();
  assert(blocked.writes == 0 && LoRagotchiChannel::ensureJoined(failed) == Result::SetFailed);
  Store readOnly; readOnly.writable = false;
  MyMesh temporary(readOnly); temporary.boot();
  assert(readOnly.writes == 1 && !readOnly.exists && same(temporary.channels[1], channel));
  readOnly.writable = true; MyMesh retry(readOnly); retry.boot();
  assert(readOnly.writes == 2 && readOnly.exists);
  puts("PASS rejected insertion never saves; failed persistence retries next boot");

  blank.disk[1] = {}; MyMesh removed(blank); removed.boot();
  assert(blank.writes == 2 && same(removed.channels[1], channel));
  puts("PASS explicitly removed automatic channel returns on next startup");
#endif
}
'''
# Independent Python hashlib expected value; do not use the native fake SHA.
import hashlib
program = program.replace(
    "EXPECTED_KEY",
    ", ".join(hex(x) for x in hashlib.sha256(b"#loragotchi-dx").digest()[:16]),
)
with tempfile.TemporaryDirectory(prefix="loragotchi-channel-") as directory:
    temp = Path(directory)
    (temp / "Utils.h").write_text(r'''
#pragma once
#include <openssl/sha.h>
#include <cstdint>
#include <cstring>
namespace mesh {
struct Utils {
  static void sha256(uint8_t* output, size_t size, const uint8_t* data, int length) {
    uint8_t digest[32]; SHA256(data, length, digest); memcpy(output, digest, size);
  }
};
}
''')
    (temp / "check.cpp").write_text(program)
    for role in ("LORAFISH_UI", "LORAGOTCHI_RX", "GENERIC_COMPANION"):
        exe = temp / role
        subprocess.run([
            "g++", "-std=c++17", "-Wall", "-Wextra", "-Werror",
            "-fsanitize=undefined,address", "-fno-omit-frame-pointer", "-no-pie",
            f"-D{role}=1", "-I", str(temp), "-I", str(root),
            str(temp / "check.cpp"), "-lcrypto", "-o", str(exe),
        ], check=True)
        print(role, flush=True)
        subprocess.run([str(exe)], check=True)
