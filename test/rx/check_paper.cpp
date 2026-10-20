#include <functional>
#include <iostream>
#include <string>
#include "UITask.h"
ColorVal UIColor::window_bkg=1, UIColor::primary_txt=0;
MyMesh the_mesh;
struct Backend : lrx::IO {
  bool saving = true;
  std::vector<std::string> sent;
  bool save(const lrx::Journal&) override { return saving; }
  bool send(const uint8_t*, const char* text, uint32_t& ack, uint32_t& timeout) override {
    sent.emplace_back(text); ack=17; timeout=10000; return true;
  }
};
Backend backend;
lrx::Receiver receiver(backend);
void loragotchiRXStatus(lrx::DisplayStatus& s) { s=lrx::displayStatus(receiver); }
struct Panel : DisplayDriver {
  unsigned frames=0;
  bool on=true;
  int x=0,y=0;
  std::vector<std::string> lines;
  std::function<void()> duringRefresh;
  Panel():DisplayDriver(250,122) {}
  bool isOn() override {return on;}
  bool isEink() override {return true;}
  void turnOn() override {on=true;}
  void turnOff() override {on=false;}
  void clear() override {}
  void startFrame(ColorVal=0) override {++frames;lines.clear();}
  void setTextSize(int n) override {assert(n==1);}
  void setColor(ColorVal) override {}
  void setCursor(int a,int b) override {x=a;y=b;}
  void print(const char* text) override {
    assert(x>=0 && x+getTextWidth(text)<=width()); assert(y>=0 && y+8<=height());
    lines.emplace_back(text);
  }
  void fillRect(int,int,int,int) override {}
  void drawRect(int,int,int,int) override {}
  void drawXbm(int,int,const uint8_t*,int,int) override {}
  uint16_t getTextWidth(const char* text) override {return strlen(text)*6;}
  void endFrame() override {if(duringRefresh)duringRefresh();}
  bool has(const char* text) const {for(auto& line:lines)if(line.find(text)!=std::string::npos)return true;return false;}
};
int main() {
  unsigned checks=0;
  { // Unchanged state causes no polling refresh, even after a day.
    Panel d;lrx::StatusScreen screen;lrx::DisplayStatus s{};s.healthy=true;
    assert(screen.update(d,s,0));
    for(uint32_t t=100;t<=86400000;t+=100)assert(!screen.update(d,s,t));
    assert(d.frames==1);++checks;
  }
  { // Coalesce many changes and render newest state, never a stale queue.
    Panel d;lrx::StatusScreen screen;lrx::DisplayStatus s{};
    screen.update(d,s,0);
    for(uint32_t t=1;t<15000;++t){s.uploads=t%5;assert(!screen.update(d,s,t));}
    s.uploads=4;s.failed=4;s.returns=4;s.healthy=true;s.bluetooth=true;s.pin=999999;s.lastId=8;s.lastFish=0xffffffff;
    assert(screen.update(d,s,15000));
    assert(d.has("Copies queued: 4") && d.has("FFFFFFFF") && d.has("999999"));
    s.healthy=false;assert(!screen.update(d,s,15001));assert(screen.update(d,s,30000));
    assert(d.has("STORAGE NOT READY"));++checks;
  }
  { // Wrap-safe time and real changes at the 15-second boundary.
    Panel d;lrx::StatusScreen screen;lrx::DisplayStatus s{};
    uint32_t start=0xfffff000;screen.update(d,s,start);s.uploads=1;
    assert(!screen.update(d,s,start+14999));assert(screen.update(d,s,start+15000));++checks;
  }
  { // A short visit remains visible after upload and return finish.
    Backend io;lrx::Receiver r(io);r.begin(nullptr,0);uint8_t key[32]={1};
    r.receive(key,"LFG1|REQ|1|99|2|Fish",0);r.receive(key,"LFG1|COMMIT|1|99",0);
    auto s=lrx::displayStatus(r);assert(s.lastFish==99 && s.uploads==1 && s.returns==1);
    r.tick(4000);r.acknowledgeRadio(17);
    r.data.slots[0].uploaded=r.data.slots[0].statusUploaded=1;
    s=lrx::displayStatus(r);assert(s.lastFish==99 && s.uploads==0 && s.returns==0);
    auto saved=r.data;saved.checksum=lrx::checksum(saved);lrx::Receiver reboot(io);reboot.begin(&saved,0);
    assert(lrx::displayStatus(reboot).lastFish==99);++checks;
  }
  { // Unsaved intake is not presented as a successfully received copy.
    Backend io;lrx::Receiver r(io);r.begin(nullptr,0);uint8_t key[32]={2};
    r.receive(key,"LFG1|REQ|2|100|2|Fish",0);io.saving=false;
    r.receive(key,"LFG1|COMMIT|2|100",0);
    auto s=lrx::displayStatus(r);assert(!s.healthy && s.lastId==0);++checks;
  }
  { // Actual UI entrypoint only queues: panel BUSY cannot enter the radio path.
    receiver.begin(nullptr,0);Panel d;UITask ui(nullptr,nullptr);mockNow=0;ui.begin(&d,nullptr,nullptr);
    ui.loop();assert(d.frames==0);runTaskStep();assert(d.frames==1);
    uint8_t key[32]={3};mockNow=20000;
    receiver.receive(key,"LFG1|REQ|3|101|2|Fish",mockNow);
    receiver.receive(key,"LFG1|COMMIT|3|101",mockNow);ui.loop();
    d.duringRefresh=[&]{
      for(uint32_t t=20000;t<=24000;t+=100){mockNow=t;receiver.tick(t);ui.notify(UIEventType::ack);ui.loop();}
    };
    runTaskStep();d.duringRefresh={};
    assert(d.frames==2);assert(backend.sent.back()=="LFG1|RETURN|3|101|C|AQUARIUM1");
    mockNow=35000;ui.loop();runTaskStep();assert(d.has("00000065"));
#ifdef BLE_PIN_CODE
    assert(d.has("BLE PIN"));the_mesh.pin=654321;mockNow=50000;ui.loop();runTaskStep();assert(d.has("654321"));
#else
    assert(d.has("USB companion 115200"));
#endif
    ++checks;
  }
  { // Allocation failure never creates a synchronous fallback in loop().
    for(int which=0;which<2;++which){
      failQueue=which==0;failTask=which==1;Panel d;UITask ui(nullptr,nullptr);ui.begin(&d,nullptr,nullptr);
      assert(d.frames==1 && d.has("unavailable"));auto before=overwrites;
      for(unsigned n=0;n<500;++n){mockNow+=100;ui.loop();}
      assert(d.frames==1 && overwrites==before);
    }
    assert(deletedQueues==1);failQueue=failTask=false;++checks;
  }
  { // Missing display is harmless; no task or radio state changes.
    auto before=overwrites;UITask ui(nullptr,nullptr);ui.begin(nullptr,nullptr,nullptr);ui.loop();
    assert(overwrites==before);++checks;
  }
  std::cout<<checks<<" production paper UI scenarios passed ("
#ifdef BLE_PIN_CODE
    <<"BLE"
#else
    <<"USB"
#endif
    <<"); mocked hardware/scheduler, not physical panel proof\n";
}
