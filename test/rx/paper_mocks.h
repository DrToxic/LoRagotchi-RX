#pragma once
#include <cassert>
#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>
#include "examples/companion_radio/ui-rx-paper/ReceiverStatus.h"
inline uint32_t mockNow = 0;
inline uint32_t millis() { return mockNow; }
namespace mesh { struct MainBoard {}; struct Packet {}; }
struct ContactInfo {};
struct ChannelDetails {};
struct SensorManager {};
struct NodePrefs {};
struct MultiSerialInterface {};
enum class UIEventType { none, ack };
struct MyMesh {
  struct Listener {
    virtual void onMessageRecv(mesh::Packet*, const ContactInfo&, uint8_t, uint32_t, const char*) = 0;
    virtual void onChannelMessageRecv(mesh::Packet*, ChannelDetails&, const char*) = 0;
    virtual void onQueueSizeChanged(int) = 0;
  };
  uint32_t pin = 123456;
  void setListener(Listener*) {}
  uint32_t getBLEPin() { return pin; }
};
struct AbstractUITask : MyMesh::Listener {
  AbstractUITask(mesh::MainBoard*, MultiSerialInterface*) {}
  virtual void notify(UIEventType = UIEventType::none) = 0;
  virtual void loop() = 0;
};
void loragotchiRXStatus(lrx::DisplayStatus&);
// Only the platform boundaries are mocks. A task runs until its first delay;
// endFrame can simulate the radio loop continuing during a panel BUSY period.
struct Queue { std::vector<uint8_t> bytes; bool full = false; };
using QueueHandle_t = Queue*;
inline std::vector<std::unique_ptr<Queue>> queues;
inline bool failQueue = false, failTask = false;
inline unsigned overwrites = 0, deletedQueues = 0;
inline void (*taskFunction)(void*) = nullptr;
inline void* taskContext = nullptr;
constexpr int pdTRUE = 1, pdPASS = 1;
inline uint32_t pdMS_TO_TICKS(uint32_t ms) { return ms; }
inline QueueHandle_t xQueueCreate(unsigned count, size_t size) {
  assert(count == 1);
  if (failQueue) return nullptr;
  queues.emplace_back(new Queue{{}, false});
  queues.back()->bytes.resize(size);
  return queues.back().get();
}
inline int xQueueOverwrite(QueueHandle_t q, const void* data) {
  ++overwrites; memcpy(q->bytes.data(), data, q->bytes.size()); q->full = true; return pdTRUE;
}
inline int xQueuePeek(QueueHandle_t q, void* data, uint32_t wait) {
  assert(wait == 0);
  if (!q->full) return 0;
  memcpy(data, q->bytes.data(), q->bytes.size()); return pdTRUE;
}
inline void vQueueDelete(QueueHandle_t q) { ++deletedQueues; q->full = false; }
inline int xTaskCreate(void(*fn)(void*), const char*, uint32_t stack, void* ctx, unsigned, void*) {
  assert(stack == 4096);
  if (failTask) return 0;
  taskFunction = fn; taskContext = ctx; return pdPASS;
}
struct TaskYield {};
inline void vTaskDelay(uint32_t delay) { assert(delay == 100); throw TaskYield{}; }
inline void runTaskStep() {
  assert(taskFunction);
  try { taskFunction(taskContext); } catch (const TaskYield&) {}
}
