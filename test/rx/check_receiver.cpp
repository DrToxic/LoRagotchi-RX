#include "examples/companion_radio/LoRagotchiRX.h"
#include <cassert>
#include <vector>
#include <string>
#include <iostream>
#include <fstream>
struct Fake:lrx::IO {
  lrx::Journal saved{};bool fail=false;uint32_t hash=100,writes=0,estimate=10000;bool sendFailure=false;
  std::vector<std::string> sent;std::vector<uint8_t> owners;
  bool save(const lrx::Journal& j) override {++writes;if(fail)return false;saved=j;return true;}
  bool send(const uint8_t owner[32],const char* text,uint32_t& ack,uint32_t& timeout) override {sent.emplace_back(text);owners.push_back(owner[0]);ack=++hash;timeout=estimate;return !sendFailure;}
};
static uint8_t owner[32]={1},other[32]={2},rx[32]={3};
static int cases=0;
#define PASS(s) do{++cases;std::cout<<"PASS "<<s<<"\n";}while(0)
void intake(lrx::Receiver& r,uint8_t* key=owner,uint32_t now=0){
  assert(r.receive(key,"LFG1|REQ|7|8|2|Pebble",now));
  assert(r.receive(key,"LFG1|COMMIT|7|8|C|1|3|5|2016",now+1));
}
std::vector<uint8_t> command(lrx::Receiver& r,uint8_t op,const std::vector<uint8_t>& args={},uint32_t token=1,uint32_t now=1){
  uint8_t in[20]={0x70,'R','X',1,op},out[80]={};memcpy(in+5,&token,4);memcpy(in+9,args.data(),args.size());
  size_t n=r.command(in,9+args.size(),out,now,rx,1);assert(n<=20);return {out,out+n};
}
std::vector<uint8_t> id(uint32_t n){return {uint8_t(n),uint8_t(n>>8),uint8_t(n>>16),uint8_t(n>>24)};}
int main(int argc,char** argv){
  {
    uint32_t value=123;
    assert(lrx::parseUnsigned("4294967295",UINT32_MAX,value) && value==UINT32_MAX);
    assert(!lrx::parseUnsigned("4294967296",UINT32_MAX,value));
    assert(!lrx::parseUnsigned("999999999999999999999999999999999999",UINT32_MAX,value));
    assert(!lrx::parseUnsigned("-0",3,value) && !lrx::parseUnsigned("+1",3,value));
    assert(!lrx::parseUnsigned(" 1",3,value) && !lrx::parseUnsigned("",3,value));
    assert(lrx::parseUnsigned("3",3,value) && value==3 && !lrx::parseUnsigned("4",3,value));
    assert(lrx::parseUnsigned("65535",65535,value) && value==65535);
    assert(!lrx::parseUnsigned("65536",65535,value));
    Fake io;lrx::Receiver r(io);r.begin(nullptr,0);
    r.receive(owner,"LFG1|REQ|4294967296|8|2|Overflow",0);
    r.receive(owner,"LFG1|REQ|1|9999999999999999999999999999|2|Overflow",0);
    assert(io.sent.empty());
    r.receive(owner,"LFG1|REQ|4294967295|4294967295|3|Maximum",0);
    assert(io.sent.back()=="LFG1|ACK|4294967295|4294967295");
    PASS("CPU-width-independent unsigned limits, overflow/sign rejection and maximum-ID intake");
  }
  {
    Fake io;lrx::Receiver r(io);assert(r.begin(nullptr,0));intake(r);assert(io.sent.back()=="LFG1|ARRIVED|7|8");
    assert(r.data.slots[0].body==1 && r.data.slots[0].appearance);uint32_t eid=r.nextEvent();assert(eid);
    auto writes=io.writes;r.receive(owner,"LFG1|COMMIT|7|8",1000);assert(r.data.slots[0].body==1 && r.nextEvent()==eid && writes==io.writes);
    r.tick(4000);assert(io.sent.back()=="LFG1|ARRIVED|7|8");r.tick(4001);assert(io.sent.back()=="LFG1|RETURN|7|8|C|AQUARIUM1");
    r.receive(owner,"LFG1|COMMIT|7|8",5000);assert(io.sent.back()=="LFG1|RETURN|7|8|C|AQUARIUM1");assert(r.data.slots[0].attempts==1);
    assert(r.acknowledgeRadio(104));assert(r.data.slots[0].state==lrx::Delivered && r.nextEvent()==eid);
    PASS("colour capture, short duplicate, fixed hold, late COMMIT returns without resurrection, PC-independent RETURN");
    assert(command(r,0)[5]==0);assert(command(r,0,{},2)[5]==2);assert(command(r,0,{},2,16000)[5]==0);
    PASS("single active consumer lease, second consumer rejected, bounded reconnect takeover");
    auto out=command(r,4,id(eid),2,16001);assert(out[5]==0);assert(r.nextEvent()!=eid);
    uint8_t payload[80];auto size=r.event(eid,payload);assert(size==77);
    if(argc>1){std::ofstream f(argv[1],std::ios::binary);f.write((char*)payload,size);}
    lrx::Receiver restarted(io);assert(restarted.begin(&io.saved,0));assert(restarted.data.slots[0].id==eid && restarted.data.slots[0].uploaded);
    assert(restarted.nextEvent()==r.data.slots[0].statusId);
    PASS("durable upload ACK, independent status event, stable ID and journal restart");
  }
  {
    Fake io;lrx::Receiver r(io);r.begin(nullptr,0);intake(r);r.receive(other,"LFG1|COMMIT|7|8|C|4|4|4|65535",10);
    r.receive(other,"LFG1|RECALL|7|8",20);r.tick(21);assert(r.data.slots[0].body==1 && !r.data.slots[0].attempts);
    r.receive(other,"LFG1|REQ|7|8|1|Same names",30);assert(r.data.slots[1].state==lrx::Reserved);
    r.receive(owner,"LFG1|RECALL|7|8",40);r.tick(40);assert(r.data.slots[0].attempts==1 && io.owners.back()==1);
    PASS("identity binding, colliding fish/transaction IDs across devices, early recall");
  }
  {
    Fake io;lrx::Receiver r(io);r.begin(nullptr,0);intake(r);uint32_t eid=r.nextEvent();
    lrx::Receiver restart(io);restart.begin(&io.saved,100);restart.tick(100);assert(io.sent.back()=="LFG1|RETURN|7|8|C|AQUARIUM1" && restart.nextEvent()==eid);
    for(int i=1;i<=4;i++)restart.tick(100+i*10000);assert(restart.data.slots[0].state==lrx::Unresolved && restart.data.slots[0].attempts==4);
    lrx::Receiver reboot(io);reboot.begin(&io.saved,0);auto count=io.sent.size();reboot.tick(100000);assert(io.sent.size()==count);
    PASS("reset during hold prioritises return, four attempts persisted, exhausted jobs never evaporate");
  }
  {
    Fake io;lrx::Receiver r(io);r.begin(nullptr,0);
    for(uint8_t k=1;k<=4;k++){uint8_t key[32]={k};intake(r,key);}
    uint8_t key[32]={5};r.receive(key,"LFG1|REQ|7|8|1|Fifth",10);assert(io.sent.back()=="LFG1|BUSY|7|8");
    r.receive(key,"LFG1|COMMIT|7|8",11);assert(r.data.nextId==5);
    PASS("four-slot bounded backlog replies BUSY, unsolicited COMMIT cannot allocate");
  }
  {
    Fake io;lrx::Receiver r(io);r.begin(nullptr,0);r.receive(owner,"LFG1|REQ|7|8|2|Fish",0);
    io.fail=true;r.receive(owner,"LFG1|COMMIT|7|8|C|1|3|5|2016",1);assert(!r.healthy() && io.sent.back()=="LFG1|ACK|7|8");
    r.tick(1);assert(io.sent.back()=="LFG1|RETURN|7|8|C");assert(command(r,0)[5]==0 && command(r,2)[5]==3);
    io.fail=false;r.tick(5001);assert(r.healthy());assert(command(r,2,{},1,5001)[5]==0);
    PASS("storage failure prevents ARRIVED/export, still returns original; bounded recovery restores outbox");
  }
  {
    Fake io;lrx::Receiver r(io);r.begin(nullptr,0);intake(r);auto eid=r.nextEvent();command(r,0);io.fail=true;
    assert(command(r,4,id(eid))[5]==3);assert(!r.data.slots[0].uploaded);io.fail=false;r.tick(1);assert(r.nextEvent()==eid);
    PASS("failed durable ACK retains unacknowledged copy for replay");
  }
  {
    Fake io;lrx::Receiver r(io);r.begin(nullptr,0);r.receive(owner,"LFG1|REQ|7|8|2|Fish",0);r.tick(30000);
    r.receive(owner,"LFG1|COMMIT|7|8",30001);assert(!r.nextEvent());
    r.receive(owner,"LFG1|REQ|7|8|2|Fish",40000);r.receive(owner,"LFG1|COMMIT|7|8",40001);
    assert(r.data.slots[0].body==6 && !r.data.slots[0].appearance);
    auto copy=io.saved;reinterpret_cast<uint8_t*>(&copy)[45]^=1;assert(!lrx::valid(copy));
    PASS("reservation timeout, missing-colour fallback, corrupt journal detection");
  }
  {
    Fake io;lrx::Receiver r(io);assert(!r.begin(nullptr,0,false));r.tick(50000);
    r.receive(owner,"LFG1|REQ|7|8|2|Fish",0);assert(io.sent.back()=="LFG1|BUSY|7|8" && io.writes==0);
    PASS("corrupt existing banks fail closed without reusing event IDs or replacing recovery data");
  }
  {
    Fake io;lrx::Receiver r(io);r.begin(nullptr,0);
    r.receive(owner,"LFG1|REQ|-1|8|2|Fish",0);r.receive(owner,"LFG1|REQ|4294967296|8|2|Fish",0);
    assert(io.sent.empty());
    intake(r);r.tick(4001);uint32_t hash=io.hash;assert(r.acknowledgeRadio(hash));
    command(r,0);command(r,4,id(r.data.slots[0].id));command(r,4,id(r.data.slots[0].statusId));
    for(uint8_t k=2;k<=4;k++){uint8_t key[32]={k};intake(r,key);}
    uint8_t fifth[32]={5};intake(r,fifth);auto n=r.data.nextId;
    r.receive(owner,"LFG1|COMMIT|7|8|C|1|3|5|2016",20);assert(io.sent.back()=="LFG1|RETURN|7|8|C|AQUARIUM1" && r.data.nextId==n);
    PASS("malformed IDs rejected; evicted completed transaction tombstone rejects stale COMMIT");
  }
  {
    Fake io;lrx::Receiver r(io);r.begin(nullptr,0);
    command(r,0);r.receive(owner,"LFG1|REQ|7|8|2|Fish",0);io.fail=true;
    r.receive(owner,"LFG1|COMMIT|7|8|C|1|3|5|2016",1);
    auto args=id(r.data.slots[0].id);args.push_back(0);args.push_back(0);
    assert(command(r,3,args)[5]==3);
    io.fail=false;r.tick(5001);assert(command(r,3,args,1,5001)[5]==0);
    PASS("READ refuses an uncommitted snapshot even when its event ID is known");
  }
  {
    Fake io;lrx::Receiver r(io);r.begin(nullptr,0);
    assert(command(r,0,{42})[5]==1); // malformed HELLO cannot acquire a lease
    assert(command(r,0,{},2)[5]==0);
    assert(command(r,2,{},2,15001)[5]==1); // expiry requires HELLO
    assert(command(r,0,{},2,15001)[5]==0);
    assert(command(r,2,{0},2,15002)[5]==1);
    PASS("exact extension lengths and expired lease require fresh capability handshake");
  }
  {
    Fake io;lrx::Receiver r(io);r.begin(nullptr,0);
    const uint32_t start=UINT32_MAX-1000;
    for(uint8_t k=1;k<=4;++k){uint8_t key[32]={k};intake(r,key,start);}
    r.tick(start+4000);for(const auto& s:r.data.slots)assert(s.attempts==0);
    r.tick(start+4001);for(const auto& s:r.data.slots)assert(s.attempts==1);
    assert(io.owners[io.owners.size()-4]==1 && io.owners.back()==4);
    PASS("all four concurrent visitors return independently across millis wrap");
  }
  {
    Fake io;lrx::Receiver r(io);r.begin(nullptr,0);
    r.receive(owner,"LFG1|REQ|7|8|2|Fish",0);r.receive(owner,"LFG1|RECALL|7|8",1);
    assert(io.sent.back()=="LFG1|RETURN|7|8|C");
    intake(r);io.fail=true;r.tick(4001);
    assert(io.sent.back()=="LFG1|RETURN|7|8|C|AQUARIUM1"); // earlier capture remains durable
    lrx::Receiver reboot(io);reboot.begin(&io.saved,0);reboot.tick(0);
    assert(io.sent.back()=="LFG1|RETURN|7|8|C|AQUARIUM1");
    PASS("AQUARIUM1 only follows durable capture, survives restart and later save failure; cancellation is unmarked");
  }
  {
    Fake io;lrx::Receiver r(io);r.begin(nullptr,0);intake(r);io.estimate=45000;
    r.tick(4001);const auto first=io.hash;r.tick(14001);assert(r.data.slots[0].attempts==1);
    r.tick(49001);assert(r.data.slots[0].attempts==2 && r.acknowledgeRadio(first));
    assert(!r.acknowledgeRadio(first));
    PASS("estimated long-path timeout delays retries; earlier attempt ACK completes once");
  }
  {
    Fake io;lrx::Receiver r(io);r.begin(nullptr,0);intake(r);assert(!r.acknowledgeRadio(0));
    io.hash=UINT32_MAX;r.tick(4001);assert(r.acknowledgeRadio(0));
    Fake failed;lrx::Receiver otherReceiver(failed);otherReceiver.begin(nullptr,0);intake(otherReceiver);
    failed.hash=UINT32_MAX;failed.sendFailure=true;otherReceiver.tick(4001);
    assert(!otherReceiver.acknowledgeRadio(0));
    PASS("zero is a valid queued ACK hash, not an unused or failed-send entry");
  }
  std::cout<<cases<<" production-core scenarios passed\n";
}
