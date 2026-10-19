#pragma once
#include "../AbstractUITask.h"
#include "../LoRagotchiRXBridge.h"
extern MyMesh the_mesh;
// Dedicated receiver UI: no pet state, care, ageing, breeding or local fish.
class UITask : public AbstractUITask {
  DisplayDriver* display=nullptr;uint32_t next=0;
public:
  UITask(mesh::MainBoard* b,MultiSerialInterface* m):AbstractUITask(b,m){}
  void begin(DisplayDriver* d,SensorManager*,NodePrefs*) {display=d;the_mesh.setListener(this);}
  void notify(UIEventType=UIEventType::none) override {next=0;}
  void loop() override {
    if(!display || int32_t(millis()-next)<0)return;next=millis()+1000;
    display->startFrame();display->setTextSize(1);display->setColor(UIColor::primary_txt);
    display->drawTextCentered(display->width()/2,2,"LoRagotchi-RX");
    uint8_t uploads=0,returns=0,failed=0;bool healthy=false;
    loragotchiRXStatus(uploads,returns,failed,healthy);
    char line[24];snprintf(line,sizeof(line),"Copies queued: %u",uploads);display->drawTextCentered(display->width()/2,17,line);
    snprintf(line,sizeof(line),"Return jobs: %u / %u",returns,failed);display->drawTextCentered(display->width()/2,29,line);
#ifdef BLE_PIN_CODE
    snprintf(line,sizeof(line),"BLE PIN: %06lu",(unsigned long)the_mesh.getBLEPin());
#else
    snprintf(line,sizeof(line),"USB companion 115200");
#endif
    display->drawTextCentered(display->width()/2,41,line);
    display->drawTextCentered(display->width()/2,53,healthy?"Ready for visitors":"STORAGE NOT READY");display->endFrame();
  }
  void onMessageRecv(mesh::Packet*,const ContactInfo&,uint8_t,uint32_t,const char*) override {}
  void onChannelMessageRecv(mesh::Packet*,ChannelDetails&,const char*) override {}
  void onQueueSizeChanged(int) override {}
};
