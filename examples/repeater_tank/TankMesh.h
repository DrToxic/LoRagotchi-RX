#pragma once
#include "../simple_repeater/MyMesh.h"
#include "../companion_radio/LoRagotchiRXJournal.h"

// Separate application peers, never repeater ACL entries. No application peer
// gains login, CLI, region, configuration or forwarding permissions.
class TankMesh : public MyMesh, public lrx::IO {
  static constexpr unsigned PEERS = 16;
  struct Peer {
    mesh::Identity id;
    uint8_t secret[32], path[MAX_PATH_SIZE], pathLen = 0xFF;
    bool used = false;
  } peers[PEERS];
  int matches[PEERS] = {}, stockMatches = 0, tankMatches = 0;
  unsigned cursor = 0;
  bool pinned(const uint8_t* key) const;
  int remember(const mesh::Identity& id);
  bool applicationText(Peer&, mesh::Packet*, const uint8_t*, uint8_t*, size_t);
protected:
  int searchPeersByHash(const uint8_t*) override;
  void getPeerSharedSecret(uint8_t*, int) override;
  void onAdvertRecv(mesh::Packet*, const mesh::Identity&, uint32_t, const uint8_t*, size_t) override;
  void onPeerDataRecv(mesh::Packet*, uint8_t, int, const uint8_t*, uint8_t*, size_t) override;
  bool onPeerPathRecv(mesh::Packet*, int, const uint8_t*, uint8_t*, uint8_t, uint8_t, uint8_t*, uint8_t) override;
  void onAckRecv(mesh::Packet* packet, uint32_t hash) override {
    if (receiver.acknowledgeRadio(hash) && packet) packet->markDoNotRetransmit();
  }
public:
  lrx::Receiver receiver;
  TankMesh(mesh::MainBoard& board, mesh::Radio& radio, mesh::MillisecondClock& ms,
           mesh::RNG& rng, mesh::RTCClock& rtc, mesh::MeshTables& tables)
    : MyMesh(board, radio, ms, rng, rtc, tables), receiver(*this) {}
  void beginTank(FILESYSTEM* fs);
  void tickTank() { receiver.tick(millis()); }
  bool save(const lrx::Journal&) override;
  bool send(const uint8_t*, const char*, uint32_t&, uint32_t&) override;
  size_t command(const uint8_t* in, size_t len, uint8_t* out, uint8_t links) {
    // High bit explicitly advertises LRX-only, not the companion/admin API.
    return receiver.command(in, len, out, millis(), self_id.pub_key, links | 0x80);
  }
};
