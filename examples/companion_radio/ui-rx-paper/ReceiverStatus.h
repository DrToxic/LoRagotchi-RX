#pragma once
#include "../LoRagotchiRX.h"
#include "StatusScreen.h"

namespace lrx {
inline DisplayStatus displayStatus(const Receiver& receiver) {
  DisplayStatus status{};
  status.healthy = receiver.healthy();
  for (const auto& slot : receiver.data.slots) {
    if (slot.state >= Captured && !slot.uploaded) ++status.uploads;
    if (slot.state == Captured) ++status.returns;
    if (slot.state == Unresolved) ++status.failed;
    // Do not label an unsaved capture as received. After recovery the journal
    // retains the latest copy even when both the return and PC ACKs are complete.
    if (status.healthy && slot.state >= Captured && slot.id > status.lastId) {
      status.lastId = slot.id;
      status.lastFish = slot.fish;
    }
  }
  return status;
}
}
