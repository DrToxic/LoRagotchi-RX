#pragma once
// LoRagotchi-owned intake/outbox. No Arduino dependencies: the production state
// machine is also compiled by the host fault-injection tests.
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>

namespace lrx {
static constexpr uint8_t COMMAND = 0x70, VERSION = 1, CAPACITY = 4;
static constexpr uint32_t HOLD_MS = 4000, RESERVE_MS = 30000, RETRY_MS = 10000;
enum State : uint8_t { Empty, Reserved, Captured, Delivered, Unresolved };
#pragma pack(push, 1)
struct Slot {
  uint8_t owner[32]; uint32_t tx, fish, id, statusId;
  uint8_t state, stage, mode, body, scales, eyes, appearance;
  uint16_t tint; char name[24];
  uint8_t attempts, uploaded, statusUploaded;
};
struct Tombstone { uint8_t owner[32]; uint32_t tx, fish; };
struct Journal {
  uint32_t magic, version, generation, nextId;
  Slot slots[CAPACITY]; Tombstone recent[8]; uint8_t cursor;
  uint32_t checksum;
};
#pragma pack(pop)
inline uint32_t checksum(const Journal& j) {
  uint32_t h=2166136261UL; const auto* p=reinterpret_cast<const uint8_t*>(&j);
  for(size_t i=0;i<offsetof(Journal, checksum);++i) h=(h^p[i])*16777619UL;
  return h;
}
inline bool valid(const Journal& j) {
  return j.magic==0x3158524cUL && j.version==1 && j.nextId>0 && j.cursor<8 && j.checksum==checksum(j);
}
// strtoul saturates at ULONG_MAX on overflow, which equals UINT32_MAX on the
// radios but not on the 64-bit host. Validate before multiplying on every CPU.
inline bool parseUnsigned(const char* text,uint32_t maximum,uint32_t& value) {
  if(!text || !*text)return false;
  uint32_t result=0;
  for(const char* p=text;*p;++p) {
    if(*p<'0' || *p>'9')return false;
    const uint32_t digit=uint32_t(*p-'0');
    if(result>maximum/10 || (result==maximum/10 && digit>maximum%10))return false;
    result=result*10+digit;
  }
  value=result;return true;
}
struct IO {
  virtual bool save(const Journal& journal)=0;
  virtual bool send(const uint8_t owner[32], const char* text, uint32_t& ack, uint32_t& timeout)=0;
  virtual ~IO() {}
};
class Receiver {
  IO& io;
  uint32_t due[CAPACITY]={}, ack[CAPACITY][4]={};
  bool ackKnown[CAPACITY][4]={};
  bool capturedDurably[CAPACITY]={};
  uint32_t lease=0, leaseUntil=0, saveRetry=0;
  bool storageOK=true, storageBlocked=false;
  static bool reached(uint32_t now,uint32_t at) {return int32_t(now-at)>=0;}
  static uint32_t u32(const uint8_t* p) {return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;}
  static void put32(uint8_t* p,uint32_t v) {for(int i=0;i<4;i++)p[i]=v>>(8*i);}
  bool persist() {
    ++data.generation; data.checksum=checksum(data);
    storageOK=io.save(data);
    if(storageOK)for(int i=0;i<CAPACITY;++i)
      if(data.slots[i].state>=Captured)capturedDurably[i]=true;
    return storageOK;
  }
  bool same(const Slot& s,const uint8_t* owner,uint32_t tx,uint32_t fish) const {
    return s.state && s.tx==tx && s.fish==fish && !memcmp(s.owner,owner,32);
  }
  void reply(const uint8_t* owner,const char* verb,uint32_t tx,uint32_t fish,bool aquarium=false) {
    char msg[80]; uint32_t unused=0,timeout=0;
    snprintf(msg,sizeof(msg),"LFG1|%s|%lu|%lu%s",verb,(unsigned long)tx,(unsigned long)fish,
        !strcmp(verb,"RETURN")?(aquarium?"|C|AQUARIUM1":"|C"):""); io.send(owner,msg,unused,timeout);
  }
  int reusable() const {
    for(int i=0;i<CAPACITY;++i) if(data.slots[i].state==Empty)return i;
    int best=-1;
    for(int i=0;i<CAPACITY;++i) {
      const auto& s=data.slots[i];
      // Unresolved jobs are never evicted; operator recovery is explicit.
      if(s.state==Delivered && s.uploaded && s.statusUploaded && (best<0 || s.id<data.slots[best].id))best=i;
    }
    return best;
  }
 public:
  Journal data{};
  explicit Receiver(IO& backend):io(backend) {
    data.magic=0x3158524cUL;data.version=1;data.nextId=1;
  }
  bool begin(const Journal* saved,uint32_t now,bool allowFresh=true) {
    if(!saved && !allowFresh){storageBlocked=true;storageOK=false;return false;}
    if(saved && valid(*saved)) {
      data=*saved;
      for(int i=0;i<CAPACITY;++i)capturedDurably[i]=data.slots[i].state>=Captured;
    }
    for(int i=0;i<CAPACITY;++i) {
      if(data.slots[i].state==Reserved)data.slots[i]=Slot{};
      due[i]=now; // reboot prioritises unfinished returns, not another hold
    }
    return persist();
  }
  bool healthy() const {return storageOK;}
  bool receive(const uint8_t owner[32],const char* text,uint32_t now) {
    if(!text || strncmp(text,"LFG1|",5))return false;
    char buf[160]; if(strlen(text)>=sizeof(buf))return true;
    strcpy(buf,text); char* fields[10]={}; size_t n=0; char* context=nullptr;
    for(char* p=strtok_r(buf,"|",&context);p && n<10;p=strtok_r(nullptr,"|",&context))fields[n++]=p;
    if(n<4)return true;
    uint32_t tx=0,fish=0;
    if(!parseUnsigned(fields[2],UINT32_MAX,tx) || !parseUnsigned(fields[3],UINT32_MAX,fish))return true;
    int index=-1;
    for(int i=0;i<CAPACITY;i++)if(same(data.slots[i],owner,tx,fish))index=i;
    if(index<0)for(const auto& t:data.recent)if(t.tx==tx && t.fish==fish && !memcmp(t.owner,owner,32)) {
      reply(owner,"RETURN",tx,fish,true);return true;
    }
    if(!strcmp(fields[1],"REQ")) {
      if(n<5)return true;
      uint32_t stage=0;if(!parseUnsigned(fields[4],3,stage))return true;
      if(index>=0) {
        reply(owner,data.slots[index].state>=Delivered || data.slots[index].attempts ? "RETURN":"ACK",tx,fish,capturedDurably[index]);
        return true;
      }
      index=reusable();
      if(index<0 || !storageOK || data.nextId>=0x7ffffffeUL) {reply(owner,"BUSY",tx,fish);return true;}
      auto& s=data.slots[index];
      if(s.state) {
        auto& t=data.recent[data.cursor++];data.cursor%=8;
        memcpy(t.owner,s.owner,32);t.tx=s.tx;t.fish=s.fish;
      }
      s=Slot{};capturedDurably[index]=false;memcpy(s.owner,owner,32);s.tx=tx;s.fish=fish;s.stage=stage;
      s.state=Reserved;s.body=s.scales=s.eyes=6;s.tint=0xffff;s.mode='M';
      if(n>=6)strncpy(s.name,fields[5],sizeof(s.name)-1);
      due[index]=now+RESERVE_MS;memset(ack[index],0,sizeof(ack[index]));
      memset(ackKnown[index],0,sizeof(ackKnown[index]));
      reply(owner,"ACK",tx,fish);return true;
    }
    if(index<0)return true; // unsolicited/late COMMIT cannot create a fish
    auto& s=data.slots[index];
    if(!strcmp(fields[1],"COMMIT")) {
      if(s.state>=Delivered || s.attempts) {reply(owner,"RETURN",tx,fish,capturedDurably[index]);return true;}
      if(s.state==Reserved) {
        if(n==9 && (fields[4][0]=='C' || fields[4][0]=='M') && !fields[4][1]) {
          uint32_t values[4]={};bool good=true;
          for(int k=0;k<4;k++)if(!parseUnsigned(fields[k+5],k==3?65535U:6U,values[k]))good=false;
          if(good){s.mode=fields[4][0];s.body=values[0];s.scales=values[1];s.eyes=values[2];s.tint=values[3];s.appearance=1;}
        }
        s.id=data.nextId++;s.state=Captured;due[index]=now+HOLD_MS;
        if(!persist()) {due[index]=now;return true;} // no ARRIVED without durable snapshot
      }
      if(storageOK)reply(owner,"ARRIVED",tx,fish);
    } else if(!strcmp(fields[1],"RECALL")) {
      if(s.state==Reserved) {reply(owner,"RETURN",tx,fish);s=Slot{};}
      else if(s.state==Captured)due[index]=now;
      else reply(owner,"RETURN",tx,fish,capturedDurably[index]);
    }
    return true;
  }
  void tick(uint32_t now) {
    if(!storageBlocked && !storageOK && reached(now,saveRetry)){persist();saveRetry=now+5000;}
    for(int i=0;i<CAPACITY;i++) {
      auto& s=data.slots[i];
      if(s.state==Reserved && reached(now,due[i])){s=Slot{};continue;}
      if(s.state!=Captured || !reached(now,due[i]))continue;
      if(s.attempts>=4) {s.state=Unresolved;s.statusId=data.nextId++;persist();continue;}
      char msg[80];snprintf(msg,sizeof(msg),"LFG1|RETURN|%lu|%lu|C%s",(unsigned long)s.tx,(unsigned long)s.fish,
          capturedDurably[i]?"|AQUARIUM1":"");
      uint8_t attempt=s.attempts++;persist(); // attempt budget survives reset
      uint32_t hash=0,timeout=0,wait=RETRY_MS;
      if(io.send(s.owner,msg,hash,timeout)) {
        ack[i][attempt]=hash;ackKnown[i][attempt]=true;
        if(timeout>wait)wait=timeout>0x7fffffffUL?0x7fffffffUL:timeout;
      }
      due[i]=now+wait;
    }
  }
  bool acknowledgeRadio(uint32_t hash) {
    for(int i=0;i<CAPACITY;i++)for(int attempt=0;attempt<4;++attempt)if(ackKnown[i][attempt] && ack[i][attempt]==hash &&
        (data.slots[i].state==Captured || data.slots[i].state==Unresolved)) {
      data.slots[i].state=Delivered;data.slots[i].statusId=data.nextId++;data.slots[i].statusUploaded=0;persist();return true;
    }
    return false;
  }
  uint16_t event(uint32_t id,uint8_t* out) const {
    for(const auto& s:data.slots)if(s.state>=Captured) {
      if(id==s.id) {
        size_t p=0;out[p++]=1;put32(out+p,id);p+=4;memcpy(out+p,s.owner,32);p+=32;
        put32(out+p,s.fish);p+=4;put32(out+p,s.tx);p+=4;
        out[p++]=s.stage;out[p++]=s.mode;out[p++]=s.body;out[p++]=s.scales;out[p++]=s.eyes;
        out[p++]=s.tint;out[p++]=s.tint>>8;out[p++]=s.appearance;
        memcpy(out+p,s.name,24);p+=24;return p;
      }
      if(id==s.statusId && s.state>=Delivered) {
        out[0]=2;put32(out+1,id);put32(out+5,s.id);out[9]=s.state==Delivered?1:2;out[10]=s.attempts;return 11;
      }
    }
    return 0;
  }
  uint32_t nextEvent() const {
    uint32_t next=0;
    for(const auto& s:data.slots)if(s.state>=Captured) {
      uint32_t id=!s.uploaded?s.id:(!s.statusUploaded && s.state>=Delivered?s.statusId:0);
      if(id && (!next || id<next))next=id;
    }
    return next;
  }
  // Extension fragments always fit the default BLE ATT payload (20 bytes).
  // GATT notifications/writes are complete companion frames, not streams.
  size_t command(const uint8_t* in,size_t len,uint8_t* out,uint32_t now,const uint8_t rxKey[32],uint8_t transports) {
    if(!len || in[0]!=COMMAND)return 0;
    out[0]=COMMAND;out[1]='R';out[2]='X';out[3]=VERSION;out[4]=len>4?in[4]:255;out[5]=1;
    if(len<9 || in[1]!='R' || in[2]!='X' || in[3]!=VERSION)return 6;
    static constexpr uint8_t sizes[]={9,10,9,15,13,9,13};
    if(in[4]>=sizeof(sizes) || len!=sizes[in[4]])return 6;
    uint32_t token=u32(in+5);if(!token)return 6;
    if(lease && lease!=token && !reached(now,leaseUntil)){out[5]=2;return 6;}
    if(in[4]!=0 && (lease!=token || reached(now,leaseUntil)))return 6;
    lease=token;leaseUntil=now+15000;out[5]=0;
    switch(in[4]) {
      case 0:out[6]=CAPACITY;out[7]=transports;out[8]=10;out[9]=storageOK;return 10;
      case 1: {
        if(len!=10 || in[9]>=32)break;
        uint8_t offset=in[9],count=32-offset;if(count>12)count=12;
        out[6]=offset;out[7]=32;memcpy(out+8,rxKey+offset,count);return 8+count;
      }
      case 2: {
        if(!storageOK){out[5]=3;return 6;}
        uint32_t id=nextEvent();uint8_t buf[80];uint16_t size=event(id,buf);
        put32(out+6,id);out[10]=size;out[11]=size>>8;return 12;
      }
      case 3: {
        if(!storageOK){out[5]=3;return 6;}
        if(len!=15)break;uint8_t buf[80];uint16_t size=event(u32(in+9),buf);
        uint16_t offset=uint16_t(in[13])|uint16_t(in[14])<<8;if(!size || offset>=size)break;
        uint16_t count=size-offset;if(count>10)count=10;
        out[6]=offset;out[7]=offset>>8;out[8]=size;out[9]=size>>8;memcpy(out+10,buf+offset,count);return 10+count;
      }
      case 4: {
        if(len!=13)break;uint32_t id=u32(in+9);
        for(auto& s:data.slots)if(s.state>=Captured && (id==s.id || (id==s.statusId && s.state>=Delivered))) {
          uint8_t& flag=id==s.statusId?s.statusUploaded:s.uploaded;
          if(!flag){flag=1;if(!persist()){flag=0;out[5]=3;}}
          return 6;
        }
        break;
      }
      case 5:lease=0;return 6;
      // Explicit operator retry of an unresolved RETURN; upload identity unchanged.
      case 6: {
        if(len!=13)break;uint32_t id=u32(in+9);
        for(int i=0;i<CAPACITY;i++)if(data.slots[i].id==id && data.slots[i].state==Unresolved){
          auto& s=data.slots[i];s.state=Captured;s.attempts=0;s.statusUploaded=0;due[i]=now;
          if(!persist())out[5]=3;return 6;
        }
        break;
      }
    }
    out[5]=1;return 6;
  }
};
} // namespace lrx
