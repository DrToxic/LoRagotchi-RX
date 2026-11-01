#include "TankMesh.h"

namespace {
const char* paths[] = {"/lrx-a.dat", "/lrx-b.dat"};
struct Files : lrx::JournalFiles {
  FILESYSTEM* fs = nullptr;
  bool exists(unsigned bank) override { return fs->exists(paths[bank]); }
  template<class T> bool readBank(unsigned bank, T& value) {
#if defined(NRF52_PLATFORM)
    auto file = fs->open(paths[bank], FILE_O_READ);
#else
    auto file = fs->open(paths[bank], "r");
#endif
    if (!file) return false;
    bool ok = file.size() == sizeof(value) && file.read((uint8_t*)&value, sizeof(value)) == sizeof(value);
    file.close(); return ok;
  }
  bool read(unsigned bank, lrx::Journal& value) override { return readBank(bank, value); }
  bool readV1(unsigned bank, lrx::JournalV1& value) override { return readBank(bank, value); }
  bool header(unsigned bank, uint32_t& magic, uint32_t& version) override {
#if defined(NRF52_PLATFORM)
    auto file = fs->open(paths[bank], FILE_O_READ);
#else
    auto file = fs->open(paths[bank], "r");
#endif
    if (!file) return false;
    uint32_t h[2] = {}; bool ok = file.read((uint8_t*)h, sizeof(h)) == sizeof(h);
    file.close(); magic = h[0]; version = h[1]; return ok;
  }
  bool write(unsigned bank, const lrx::Journal& value) override {
#if defined(NRF52_PLATFORM)
    if (exists(bank) && !fs->remove(paths[bank])) return false;
    auto file = fs->open(paths[bank], FILE_O_WRITE);
#else
    auto file = fs->open(paths[bank], "w");
#endif
    if (!file) return false;
    size_t written = file.write((const uint8_t*)&value, sizeof(value));
    file.flush(); file.close(); return written == sizeof(value);
  }
} files;
lrx::JournalStore journal(files);
}
void TankMesh::beginTank(FILESYSTEM* fs) {
  files.fs = fs;
  if (!fs) { receiver.begin(nullptr, millis(), false); return; }
  lrx::Journal saved{}; bool fresh = false;
  bool loaded = journal.load(saved, fresh);
  receiver.begin(loaded ? &saved : nullptr, millis(), fresh);
  // Durable owner keys rehydrate ECDH peers after reboot, even if the original
  // advert is no longer present. Unknown paths use the normal scoped flood.
  for (const auto& s : receiver.data.slots)
    if (s.state != lrx::Empty) remember(mesh::Identity(s.owner));
}
bool TankMesh::save(const lrx::Journal& value) { return files.fs && journal.save(value); }
bool TankMesh::pinned(const uint8_t* key) const {
  for (const auto& s : receiver.data.slots)
    if (s.state != lrx::Empty && !memcmp(s.owner, key, 32)) return true;
  return false;
}
int TankMesh::remember(const mesh::Identity& id) {
  for (unsigned i = 0; i < PEERS; ++i)
    if (peers[i].used && peers[i].id.matches(id.pub_key)) return i;
  for (unsigned n = 0; n < PEERS; ++n) {
    unsigned i = (cursor + n) % PEERS;
    if (!peers[i].used || !pinned(peers[i].id.pub_key)) {
      peers[i] = Peer{}; peers[i].id = id; peers[i].used = true;
      self_id.calcSharedSecret(peers[i].secret, id);
      cursor = (i + 1) % PEERS; return i;
    }
  }
  return -1;
}
int TankMesh::searchPeersByHash(const uint8_t* hash) {
  stockMatches = MyMesh::searchPeersByHash(hash); tankMatches = 0;
  for (unsigned i = 0; i < PEERS; ++i)
    if (peers[i].used && peers[i].id.isHashMatch(hash)) matches[tankMatches++] = i;
  // Application entries first so an existing admin's LFG1 plain text is not
  // accidentally executed as a CLI command. Non-application text is delegated.
  return tankMatches + stockMatches;
}
void TankMesh::getPeerSharedSecret(uint8_t* dest, int index) {
  if (index < tankMatches) memcpy(dest, peers[matches[index]].secret, 32);
  else MyMesh::getPeerSharedSecret(dest, index - tankMatches);
}
void TankMesh::onAdvertRecv(mesh::Packet* packet, const mesh::Identity& id, uint32_t time,
                           const uint8_t* app, size_t len) {
  MyMesh::onAdvertRecv(packet, id, time, app, len);
  AdvertDataParser parser(app, len);
  if (parser.isValid() && parser.getType() == ADV_TYPE_CHAT) remember(id);
}
bool TankMesh::applicationText(Peer& peer, mesh::Packet* packet, const uint8_t* secret,
                               uint8_t* data, size_t len) {
  if (len <= 5 || len > MAX_PACKET_PAYLOAD || (data[4] >> 2) != TXT_TYPE_PLAIN) return false;
  // Decrypted packets are padded. Never write data[len] in the shared buffer.
  char text[MAX_PACKET_PAYLOAD + 1];
  size_t textLen = strnlen((char*)data + 5, len - 5);
  if (textLen < 5 || memcmp(data + 5, "LFG1|", 5)) return false;
  memcpy(text, data + 5, textLen); text[textLen] = 0;
  receiver.receive(peer.id.pub_key, text, millis());
  uint32_t hash;
  mesh::Utils::sha256((uint8_t*)&hash, 4, data, 5 + textLen, peer.id.pub_key, 32);
  if (packet->isRouteFlood()) {
    auto reply = createPathReturn(peer.id, secret, packet->path, packet->path_len,
                                  PAYLOAD_TYPE_ACK, (uint8_t*)&hash, 4);
    if (reply) sendFloodReply(reply, 250, packet->getPathHashSize());
  } else {
    auto ack = createAck(hash);
    if (ack) {
      if (peer.pathLen == 0xFF) sendFloodReply(ack, 250, packet->getPathHashSize());
      else sendDirect(ack, peer.path, peer.pathLen, 250);
    }
  }
  return true;
}
void TankMesh::onPeerDataRecv(mesh::Packet* packet, uint8_t type, int index,
                             const uint8_t* secret, uint8_t* data, size_t len) {
  if (index < tankMatches) {
    auto& peer = peers[matches[index]];
    if (type == PAYLOAD_TYPE_TXT_MSG && applicationText(peer, packet, secret, data, len)) return;
    // A key may occur in both tables. The first successful ECDH decryption wins
    // in Mesh::onRecvPacket, so explicitly dispatch other traffic to its ACL.
    for (int i = 0; i < stockMatches; ++i) {
      uint8_t candidate[32]; MyMesh::getPeerSharedSecret(candidate, i);
      if (!memcmp(candidate, secret, 32)) { MyMesh::onPeerDataRecv(packet, type, i, secret, data, len); return; }
    }
    return; // application peers cannot execute admin commands
  }
  MyMesh::onPeerDataRecv(packet, type, index - tankMatches, secret, data, len);
}
bool TankMesh::onPeerPathRecv(mesh::Packet* packet, int index, const uint8_t* secret,
                            uint8_t* path, uint8_t len, uint8_t extraType,
                            uint8_t* extra, uint8_t extraLen) {
  if (index >= tankMatches)
    return MyMesh::onPeerPathRecv(packet, index - tankMatches, secret, path, len, extraType, extra, extraLen);
  auto& peer = peers[matches[index]];
  if (!mesh::Packet::isValidPathLen(len)) return false;
  peer.pathLen = mesh::Packet::copyPath(peer.path, path, len);
  if (extraType == PAYLOAD_TYPE_ACK && extraLen >= 4) {
    uint32_t hash; memcpy(&hash, extra, 4); receiver.acknowledgeRadio(hash);
  }
  for (int i = 0; i < stockMatches; ++i) {
    uint8_t candidate[32]; MyMesh::getPeerSharedSecret(candidate, i);
    if (!memcmp(candidate, secret, 32)) {
      MyMesh::onPeerPathRecv(packet, i, secret, path, len, extraType, extra, extraLen); break;
    }
  }
  return false;
}
bool TankMesh::send(const uint8_t* owner, const char* text, uint32_t& ack, uint32_t& timeout) {
  // Keep capacity for normal repeater forwarding/admin traffic. No success or
  // ACK hash is reported until a real packet has been allocated and queued.
  if (_mgr->getOutboundTotal() >= 4) return false;
  int index = remember(mesh::Identity(owner)); if (index < 0) return false;
  auto& peer = peers[index];
  size_t len = strlen(text); if (len > 160) return false;
  uint8_t data[165]; uint32_t timestamp = getRTCClock()->getCurrentTimeUnique();
  memcpy(data, &timestamp, 4); data[4] = 0; memcpy(data + 5, text, len);
  auto packet = createDatagram(PAYLOAD_TYPE_TXT_MSG, peer.id, peer.secret, data, len + 5);
  if (!packet) return false;
  mesh::Utils::sha256((uint8_t*)&ack, 4, data, len + 5, self_id.pub_key, 32);
  uint32_t airtime = _radio->getEstAirtimeFor(packet->getRawLength());
  if (peer.pathLen == 0xFF) {
    timeout = 500 + 16 * airtime;
    sendTankFlood(packet);
  } else {
    timeout = 500 + (6 * airtime + 250) * ((peer.pathLen & 63) + 1);
    sendDirect(packet, peer.path, peer.pathLen);
  }
  return true;
}
