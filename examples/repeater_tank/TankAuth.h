#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
namespace lrx {
// LRXB challenge/proof contract, identical to Aquarium's existing bridge.
// HMAC authenticates a client; it does NOT encrypt the subsequent stream.
class TankAuth {
  uint8_t output[36] = {}, expected[32] = {}, proof[32] = {};
  size_t offset = 0, length = 0, received = 0;
  uint32_t started = 0;
  enum State { Challenge, Proof, Reply, Accepted, Rejected } state = Rejected;
  bool match = false;
public:
  void begin(const uint8_t* nonce, const uint8_t* hmac, uint32_t now) {
    memcpy(output, "LRXB", 4); memcpy(output + 4, nonce, 32);
    memcpy(expected, hmac, 32); memset(proof, 0, sizeof(proof));
    offset = received = 0; length = 36; started = now; state = Challenge; match = false;
  }
  bool ready() const { return state == Accepted; }
  bool rejected() const { return state == Rejected; }
  bool wantsProof() const { return state == Proof; }
  bool timedOut(uint32_t now) const { return !ready() && uint32_t(now-started) >= 5000; }
  const uint8_t* bytes() const { return output + offset; }
  size_t remaining() const { return length - offset; }
  void sent(size_t count) {
    if (count > remaining()) return;
    offset += count;
    if (offset == length) {
      if (state == Challenge) state = Proof;
      else if (state == Reply) state = match ? Accepted : Rejected;
    }
  }
  void feed(uint8_t c) {
    if (state != Proof || received >= 32) return;
    proof[received++] = c;
    if (received == 32) {
      uint8_t difference = 0;
      for (size_t i = 0; i < 32; ++i) difference |= proof[i] ^ expected[i];
      match = difference == 0;
      memcpy(output, match ? "OK" : "NO", 2); offset = 0; length = 2; state = Reply;
      memset(proof, 0, sizeof(proof)); memset(expected, 0, sizeof(expected));
    }
  }
};
} // namespace lrx
