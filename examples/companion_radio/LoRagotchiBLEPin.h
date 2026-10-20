#pragma once

#include <stdint.h>

namespace LoRagotchiBLE {

// A saved nonzero PIN is an explicit choice. The inherited build default
// 123456 means automatic selection; a different build PIN is also explicit.
// Select once at boot, without persisting the automatic PIN or altering bonds.
template <typename RandomSource>
uint32_t selectPin(uint32_t savedPin, uint32_t buildPin, RandomSource& rng) {
  if (savedPin != 0) return savedPin;
  if (buildPin != 123456) return buildPin;

  constexpr uint32_t count = 900000; // inclusive range 100000..999999
  constexpr uint32_t threshold = (uint32_t(0) - count) % count;
  uint32_t sample;
  do {
    rng.random(reinterpret_cast<uint8_t*>(&sample), sizeof(sample));
  } while (sample < threshold); // avoid modulo bias
  return 100000 + sample % count;
}

} // namespace LoRagotchiBLE
