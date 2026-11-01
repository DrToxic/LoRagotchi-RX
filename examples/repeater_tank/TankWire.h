#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>

namespace lrx {
// Incremental, fixed storage, one completed request per scheduler turn. The
// same framing is used by TCP and USB; CLI is permitted only on physical USB.
class TankWire {
  uint8_t state = 0;
  uint16_t expected = 0, count = 0;
  uint32_t last = 0;
  bool overflow = false;
public:
  enum Result { None, Frame, Line };
  uint8_t frame[20] = {};
  char line[160] = {};
  size_t size = 0;
  void reset() { state = 0; expected = count = 0; overflow = false; size = 0; }
  bool active() const { return state || count; }
  void expire(uint32_t now) { if (active() && uint32_t(now-last) >= 5000) reset(); }
  Result feed(uint8_t c, uint32_t now, bool cli) {
    expire(now); last = now;
    if (!state) {
      if (!count && c == '<') { state = 1; return None; }
      if (!cli) return None;
      if (c == '\r') {
        if (!overflow && count) { line[count] = 0; size = count; count = 0; return Line; }
        count = 0; overflow = false; return None;
      }
      if (c == '\n') return None;
      if (count < sizeof(line)-1) line[count++] = c; else overflow = true;
      return None;
    }
    if (state == 1) { expected = c; state = 2; return None; }
    if (state == 2) {
      expected |= uint16_t(c) << 8; count = 0; state = 3;
      if (!expected) reset();
      return None;
    }
    if (expected <= sizeof(frame)) frame[count] = c;
    if (++count == expected) {
      bool valid = expected <= sizeof(frame); size = expected;
      state = 0; count = expected = 0;
      return valid ? Frame : None;
    }
    return None;
  }
};
class TankReply {
  uint8_t data[23] = {};
  size_t count = 0, offset = 0;
public:
  bool pending() const { return offset < count; }
  void clear() { count = offset = 0; }
  bool set(const uint8_t* value, size_t size, bool framed = true) {
    if (pending() || !size || size > 20) return false;
    offset = 0; count = size + (framed ? 3 : 0);
    if (framed) { data[0] = '>'; data[1] = size; data[2] = 0; }
    memcpy(data + (framed ? 3 : 0), value, size); return true;
  }
  const uint8_t* bytes() const { return data + offset; }
  size_t remaining() const { return count - offset; }
  void sent(size_t size) { if (size <= remaining()) offset += size; }
};
} // namespace lrx
