#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <vector>
#include <string>
#include <map>
#include <cassert>
#define MAX_PATH_SIZE 64
#define MAX_PACKET_PAYLOAD 184
#define PUB_KEY_SIZE 32
#define PAYLOAD_TYPE_TXT_MSG 2
#define PAYLOAD_TYPE_ACK 3
#define PAYLOAD_TYPE_REQ 0
#define TXT_TYPE_PLAIN 0
#define ADV_TYPE_CHAT 1
#define ADV_TYPE_REPEATER 2
inline uint32_t now = 0;
inline uint32_t millis() { return now; }
struct FakeFS {
  std::map<std::string,std::vector<uint8_t>> files;
  bool fail = false;
  struct File {
    std::vector<uint8_t>* bytes = nullptr; size_t pos = 0; bool fail = false;
    explicit operator bool() const { return bytes; }
    size_t size() const { return bytes ? bytes->size() : 0; }
    size_t read(uint8_t* out, size_t n) {
      if (!bytes) return 0; n = std::min(n, bytes->size()-pos);
      memcpy(out, bytes->data()+pos, n); pos += n; return n;
    }
    size_t write(const uint8_t* data, size_t n) {
      if (!bytes || fail) return 0; bytes->assign(data, data+n); return n;
    }
    void flush() {} void close() {}
  };
  bool exists(const char* key) { return files.count(key); }
  File open(const char* key, const char* mode) {
    if (*mode == 'r' && !exists(key)) return {};
    auto& value = files[key]; if (*mode == 'w') value.clear();
    return {&value,0,fail};
  }
};
using FILESYSTEM = FakeFS;
namespace mesh {
struct Identity {
  uint8_t pub_key[32] = {};
  Identity() = default; explicit Identity(const uint8_t* key) { memcpy(pub_key,key,32); }
  bool matches(const uint8_t* key) const { return !memcmp(pub_key,key,32); }
  bool isHashMatch(const uint8_t* hash) const { return pub_key[0] == hash[0]; }
};
struct LocalIdentity : Identity {
  void calcSharedSecret(uint8_t* dest, const Identity& peer) const { memcpy(dest,peer.pub_key,32); }
};
struct Packet {
  uint8_t path[64] = {}, path_len = 0, target[32] = {};
  uint8_t extraType = 0; uint32_t ack = 0; std::vector<uint8_t> payload;
  bool flood = true, doNotRetransmit = false;
  void markDoNotRetransmit() { doNotRetransmit = true; }
  bool isRouteFlood() const { return flood; }
  uint8_t getPathHashSize() const { return 1; }
  size_t getRawLength() const { return payload.size()+10; }
  static bool isValidPathLen(uint8_t len) { return (len >> 6) < 3 && size_t((len&63)*((len>>6)+1)) <= 64; }
  static uint8_t copyPath(uint8_t* dst, const uint8_t* src, uint8_t len) {
    memcpy(dst,src,(len&63)*((len>>6)+1)); return len;
  }
};
struct MainBoard {}; struct Radio { uint32_t getEstAirtimeFor(size_t) const { return 100; } };
struct MillisecondClock {}; struct RNG {}; struct RTCClock {
  uint32_t unique=1000; uint32_t getCurrentTimeUnique() { return unique++; }
}; struct MeshTables {};
struct Utils {
  static void sha256(uint8_t* out, size_t n, const uint8_t* a, size_t an, const uint8_t* b, size_t bn) {
    uint32_t hash=2166136261U;
    for (size_t i=0;i<an;++i) hash=(hash^a[i])*16777619U;
    for (size_t i=0;i<bn;++i) hash=(hash^b[i])*16777619U;
    memcpy(out,&hash,n); // deterministic MOCK, not cryptography coverage
  }
};
}
struct AdvertDataParser {
  const uint8_t* app; size_t len;
  AdvertDataParser(const uint8_t* data,size_t n):app(data),len(n) {}
  bool isValid() const { return len>0; }
  int getType() const { return app[0]; }
};
class MyMesh {
protected:
  struct Manager { int total=0; int getOutboundTotal() const { return total; } } manager;
  Manager* _mgr=&manager; mesh::Radio* _radio; mesh::RTCClock* rtc;
  virtual int searchPeersByHash(const uint8_t* hash) {
    stock.clear(); for(size_t i=0;i<admins.size();++i) if(admins[i].isHashMatch(hash)) stock.push_back(i);
    return stock.size();
  }
  virtual void getPeerSharedSecret(uint8_t* dst,int index) { memcpy(dst,admins[stock[index]].pub_key,32); }
  virtual void onAdvertRecv(mesh::Packet*,const mesh::Identity&,uint32_t,const uint8_t*,size_t) { ++stockAdverts; }
  virtual void onPeerDataRecv(mesh::Packet*,uint8_t,int index,const uint8_t*,uint8_t*,size_t) { ++stockData; lastAdmin=admins[stock[index]]; }
  virtual bool onPeerPathRecv(mesh::Packet*,int,const uint8_t*,uint8_t*,uint8_t,uint8_t,uint8_t*,uint8_t) { ++stockPaths; return false; }
  virtual void onAckRecv(mesh::Packet*,uint32_t) {}
  mesh::Packet* make(const mesh::Identity& id,const uint8_t* data,size_t n) {
    if (noPackets) return nullptr; auto p = new mesh::Packet;
    memcpy(p->target,id.pub_key,32); p->payload.assign(data,data+n); return p;
  }
  mesh::Packet* createDatagram(uint8_t,const mesh::Identity& id,const uint8_t*,const uint8_t* data,size_t n) { return make(id,data,n); }
  mesh::Packet* createPathReturn(const mesh::Identity& id,const uint8_t*,const uint8_t*,uint8_t,uint8_t type,const uint8_t* data,size_t n) {
    auto p=make(id,data,n);if(p)p->extraType=type; return p;
  }
  mesh::Packet* createAck(uint32_t hash) { auto p = new mesh::Packet; p->ack=hash; return p; }
  void record(mesh::Packet* p,bool direct) { p->flood=!direct; sent.push_back(*p); delete p; }
  void sendTankFlood(mesh::Packet* p) { ++defaultFloods; record(p,false); }
  void sendFloodReply(mesh::Packet* p,uint32_t,uint8_t) { ++scopedReplies; record(p,false); }
  void sendDirect(mesh::Packet* p,const uint8_t*,uint8_t,uint32_t=0) { record(p,true); }
public:
  mesh::LocalIdentity self_id;
  std::vector<mesh::Packet> sent;
  std::vector<mesh::Identity> admins; std::vector<int> stock;
  unsigned stockData=0,stockAdverts=0,stockPaths=0,defaultFloods=0,scopedReplies=0;
  mesh::Identity lastAdmin; bool noPackets=false;
  MyMesh(mesh::MainBoard&,mesh::Radio& radio,mesh::MillisecondClock&,mesh::RNG&,mesh::RTCClock& clock,mesh::MeshTables&)
    :_radio(&radio),rtc(&clock) {self_id.pub_key[0]=99;}
  virtual ~MyMesh() {}
  mesh::RTCClock* getRTCClock() { return rtc; }
  void busy(int n) { manager.total=n; }
};
