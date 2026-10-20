#pragma once
#include "../AbstractUITask.h"
#include "../LoRagotchiRXBridge.h"
#include "StatusScreen.h"
#include "../LoRagotchiChannelStatus.h"
#ifdef LORAGOTCHI_RX_EINK_TASK
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#endif
extern MyMesh the_mesh;

// Dedicated receiver UI: no pet state, care, ageing, breeding or local fish.
class UITask : public AbstractUITask {
  DisplayDriver* display = nullptr;
  lrx::StatusScreen screen;
  uint32_t lastSample = 0;
  bool sampled = false;
#ifdef LORAGOTCHI_RX_EINK_TASK
  QueueHandle_t frames = nullptr;
  static void renderTask(void* context) {
    auto& ui = *static_cast<UITask*>(context);
    lrx::DisplayStatus value{};
    for (;;) {
      if (xQueuePeek(ui.frames, &value, 0) == pdTRUE)
        ui.screen.update(*ui.display, value, millis());
      vTaskDelay(pdMS_TO_TICKS(100));
    }
  }
#endif
public:
  UITask(mesh::MainBoard* b, MultiSerialInterface* m) : AbstractUITask(b, m) {}
  void begin(DisplayDriver* d, SensorManager*, NodePrefs*) {
    display = d;
    the_mesh.setListener(this);
#ifdef LORAGOTCHI_RX_EINK_TASK
    if (!display) return;
    frames = xQueueCreate(1, sizeof(lrx::DisplayStatus));
    if (frames && xTaskCreate(renderTask, "rx-eink", 4096, this, 1, nullptr) == pdPASS) return;
    if (frames) vQueueDelete(frames);
    frames = nullptr;
    // Setup-only error frame. Never fall back to blocking panel I/O in loop().
    display->startFrame();
    display->setTextSize(1);
    display->drawTextCentered(display->width() / 2, 28, "Display worker unavailable");
    display->endFrame();
#endif
  }
  // Messages/ACKs cannot force an unchanged screen to refresh.
  void notify(UIEventType = UIEventType::none) override {}
  void loop() override {
    if (!display) return;
    const uint32_t now = millis();
    if (sampled && uint32_t(now - lastSample) < 100) return;
    sampled = true;
    lastSample = now;
    lrx::DisplayStatus value{};
    loragotchiRXStatus(value);
    value.channelFull = LoRagotchiChannel::startupResult == LoRagotchiChannel::Result::Full;
#ifdef BLE_PIN_CODE
    value.bluetooth = true;
    value.pin = the_mesh.getBLEPin();
#endif
#ifdef LORAGOTCHI_RX_EINK_TASK
    // Single-slot, by-value mailbox coalesces changes without blocking the radio.
    if (frames) xQueueOverwrite(frames, &value);
#else
    screen.update(*display, value, now);
#endif
  }
  void onMessageRecv(mesh::Packet*, const ContactInfo&, uint8_t, uint32_t, const char*) override {}
  void onChannelMessageRecv(mesh::Packet*, ChannelDetails&, const char*) override {}
  void onQueueSizeChanged(int) override {}
};
