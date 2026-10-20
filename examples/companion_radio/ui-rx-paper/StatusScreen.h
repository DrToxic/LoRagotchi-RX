#pragma once
#include <stdint.h>
#include <stdio.h>
#include <helpers/ui/DisplayDriver.h>

namespace lrx {
// Value-only snapshot. The display worker never reads mesh, journal, or BLE state.
struct DisplayStatus {
  uint8_t uploads = 0, returns = 0, failed = 0;
  bool healthy = false;
  uint32_t lastId = 0, lastFish = 0, pin = 0;
  bool bluetooth = false, channelFull = false;

  bool operator==(const DisplayStatus& b) const {
    return uploads == b.uploads && returns == b.returns && failed == b.failed &&
      healthy == b.healthy && lastId == b.lastId && lastFish == b.lastFish &&
      pin == b.pin && bluetooth == b.bluetooth && channelFull == b.channelFull;
  }
};

class StatusScreen {
  DisplayStatus previous{};
  bool drawn = false;
  uint32_t lastDraw = 0;
public:
  // A minimum interval, not a periodic refresh: an idle screen stays untouched.
  static constexpr uint32_t EINK_INTERVAL_MS = 15000;
  bool update(DisplayDriver& display, const DisplayStatus& value, uint32_t now) {
    if (!display.isOn()) {
      drawn = false;
      return false;
    }
    const bool paper = display.isEink();
    DisplayStatus visible = value;
    // Only presence and fish ID are rendered, not the internal event number.
    visible.lastId = visible.lastId ? 1 : 0;
    if (!paper) visible.lastId = visible.lastFish = 0;
    if (drawn && (visible == previous ||
        uint32_t(now - lastDraw) < (paper ? EINK_INTERVAL_MS : 1000U))) return false;

    display.startFrame();
    display.setTextSize(1);
    display.setColor(UIColor::primary_txt);
    const int mid = display.width() / 2;
    const bool full = value.channelFull;
    display.drawTextCentered(mid, paper ? (full ? 4 : 8) : 2,
                             full && !paper ? "#loragotchi-dx" : "LoRagotchi-RX");
    char line[40];
    if (full) {
      display.drawTextCentered(mid, paper ? 20 : 12,
                               paper ? "#loragotchi-dx: join failed" : "Could not join");
      display.drawTextCentered(mid, paper ? 34 : 22, "No free channel slot");
    }
    if (full && !paper) {
      snprintf(line, sizeof(line), "Q:%u R:%u Fail:%u",
               unsigned(value.uploads), unsigned(value.returns), unsigned(value.failed));
      display.drawTextCentered(mid, 32, line);
    } else {
      snprintf(line, sizeof(line), "Copies queued: %u", unsigned(value.uploads));
      display.drawTextCentered(mid, paper ? (full ? 48 : 30) : 17, line);
      snprintf(line, sizeof(line), "Return jobs: %u / %u",
               unsigned(value.returns), unsigned(value.failed));
      display.drawTextCentered(mid, paper ? (full ? 62 : 50) : 29, line);
    }
    if (value.bluetooth) snprintf(line, sizeof(line), "BLE PIN: %06lu", (unsigned long)value.pin);
    else snprintf(line, sizeof(line), "USB companion 115200");
    display.drawTextCentered(mid, paper ? (full ? 76 : 70) : (full ? 42 : 41), line);
    if (paper) {
      if (value.lastId) snprintf(line, sizeof(line), "><> Last fish: %08lX", (unsigned long)value.lastFish);
      else snprintf(line, sizeof(line), "No fish received yet");
      display.drawTextCentered(mid, 90, line);
    }
    display.drawTextCentered(mid, paper ? 108 : 53,
                             value.healthy ? "Ready for visitors" : "STORAGE NOT READY");
    display.endFrame();
    previous = visible;
    lastDraw = now;
    drawn = true;
    return true;
  }
};
}
