#include <iostream>
#include <string>
#include "paper_mocks.h"
void loragotchiRXStatus(uint8_t&, uint8_t&, uint8_t&, bool&);
#include "UITask.h"
ColorVal UIColor::window_bkg=1, UIColor::primary_txt=0;
MyMesh the_mesh;
LoRagotchiChannel::Result LoRagotchiChannel::startupResult = LoRagotchiChannel::Result::AlreadyJoined;
bool storageHealthy=true;
void loragotchiRXStatus(uint8_t& uploads, uint8_t& returns, uint8_t& failed, bool& healthy) {
  uploads=returns=failed=LoRagotchiChannel::startupResult==LoRagotchiChannel::Result::Full ? 255 : 4; healthy=storageHealthy;
}
struct Panel : DisplayDriver {
  bool on=true; int x=0,y=0;
  std::vector<int> ys;
  std::vector<std::string> lines;
  Panel():DisplayDriver(128,64) {}
  bool isOn() override {return on;}
  void turnOn() override {on=true;}
  void turnOff() override {on=false;}
  void clear() override {}
  void startFrame(ColorVal=0) override {lines.clear();ys.clear();}
  void setTextSize(int n) override {assert(n==1);}
  void setColor(ColorVal) override {}
  void setCursor(int a,int b) override {x=a;y=b;}
  void print(const char* text) override {
    assert(x>=0 && x+getTextWidth(text)<=width() && y>=0 && y+8<=height());
    for(auto old:ys)assert(y+8<=old || y>=old+8);
    ys.push_back(y); lines.emplace_back(text);
  }
  void fillRect(int,int,int,int) override {}
  void drawRect(int,int,int,int) override {}
  void drawXbm(int,int,const uint8_t*,int,int) override {}
  uint16_t getTextWidth(const char* text) override {return strlen(text)*6;}
  void endFrame() override {}
  bool has(const char* text) const {
    for(auto& line:lines)if(line.find(text)!=std::string::npos)return true;
    return false;
  }
};
int main() {
  using Result=LoRagotchiChannel::Result;
  for(auto result:{Result::AlreadyJoined,Result::Added,Result::Full}) {
    LoRagotchiChannel::startupResult=result;
    Panel d;UITask ui(nullptr,nullptr);mockNow=0;ui.begin(&d,nullptr,nullptr);
    storageHealthy=true;ui.loop();
    assert(d.has("Ready for visitors"));
    if(result==Result::Full) {
      assert(d.has("#loragotchi-dx") && d.has("Could not join") && d.has("No free channel slot"));
      assert(d.has("Q:255 R:255 Fail:255"));
    } else assert(!d.has("No free channel slot") && d.has("Copies queued: 4"));
#ifdef BLE_PIN_CODE
    assert(d.has("BLE PIN: 123456"));
#else
    assert(d.has("USB companion 115200"));
#endif
    storageHealthy=false;mockNow=60000;ui.loop();
    assert(d.has("STORAGE NOT READY"));
    assert(d.has("No free channel slot")== (result==Result::Full));
  }
  UITask noDisplay(nullptr,nullptr);noDisplay.begin(nullptr,nullptr,nullptr);noDisplay.loop();
  std::cout<<"PASS compact RX notice persists with counters, pairing and storage errors; 128x64 bounds/overlap checked\n";
}
