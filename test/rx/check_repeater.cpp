#include "TankMesh.h"
#include "TankWire.h"
#include "TankAuth.h"
#include <iostream>
#include <array>
struct Probe : TankMesh {
  using TankMesh::TankMesh; using TankMesh::searchPeersByHash;
  using TankMesh::getPeerSharedSecret; using TankMesh::onAdvertRecv;
  using TankMesh::onPeerDataRecv; using TankMesh::onPeerPathRecv; using TankMesh::onAckRecv;
  int find(const mesh::Identity& key) {
    int count=searchPeersByHash(key.pub_key); uint8_t secret[32];
    for(int i=0;i<count;++i) {getPeerSharedSecret(secret,i);if(!memcmp(secret,key.pub_key,32))return i;}
    return -1;
  }
  void advert(const mesh::Identity& key) {uint8_t app=ADV_TYPE_CHAT;mesh::Packet p;onAdvertRecv(&p,key,0,&app,1);}
  void text(const mesh::Identity& key,const char* text,uint8_t kind=TXT_TYPE_PLAIN) {
    int index=find(key);assert(index>=0);std::vector<uint8_t> data(5+strlen(text)+1);
    data[4]=kind<<2;memcpy(data.data()+5,text,strlen(text));mesh::Packet p;
    onPeerDataRecv(&p,PAYLOAD_TYPE_TXT_MSG,index,key.pub_key,data.data(),data.size());
  }
};
int main(int argc,char**) {
  if (argc>1) {
    mesh::MainBoard board;mesh::Radio radio;mesh::MillisecondClock clock;mesh::RNG random;
    mesh::RTCClock rtc;mesh::MeshTables tables;FakeFS fs;
    Probe mesh(board,radio,clock,random,rtc,tables);mesh.beginTank(&fs);
    mesh::Identity owner;owner.pub_key[0]=7;mesh.advert(owner);
    mesh.text(owner,"LFG1|REQ|7|8|2|Water");
    mesh.text(owner,"LFG1|COMMIT|7|8|C|1|2|3|65535|H1|220|120|6|S1|13");
    owner.pub_key[31]=2;mesh.advert(owner);
    mesh.text(owner,"LFG1|REQ|9|10|2|Legacy");mesh.text(owner,"LFG1|COMMIT|9|10|C|1|2|3|65535");
    now=5000;mesh.tickTank();
    // Mock home radios acknowledge the real outbound RETURN payloads.
    auto sent=mesh.sent;
    for(const auto& packet:sent) if(packet.payload.size()>5 &&
        std::string((char*)packet.payload.data()+5,packet.payload.size()-5).find("LFG1|RETURN|")==0) {
      uint32_t hash;mesh::Utils::sha256((uint8_t*)&hash,4,packet.payload.data(),packet.payload.size(),mesh.self_id.pub_key,32);
      mesh.onAckRecv(nullptr,hash);
    }
    while (true) {
      uint8_t header[3];if(!std::cin.read((char*)header,3))break;
      size_t size=header[1]+size_t(header[2])*256;assert(header[0]=='<' && size<=20);
      uint8_t input[20],output[20];if(!std::cin.read((char*)input,size))break;
      size_t n=mesh.command(input,size,output,4);if(!n){output[0]=1;output[1]=6;n=2;}
      uint8_t response[3]={'>',uint8_t(n),0};std::cout.write((char*)response,2);std::cout.flush();
      std::cout.write((char*)response+2,1);std::cout.write((char*)output,n);std::cout.flush();
    }
    return 0;
  }
  mesh::MainBoard board;mesh::Radio radio;mesh::MillisecondClock clock;mesh::RNG random;
  mesh::RTCClock rtc;mesh::MeshTables tables;FakeFS fs;
  auto key=[](uint8_t tail){mesh::Identity id;id.pub_key[0]=7;id.pub_key[31]=tail;return id;};
  auto a=key(1),b=key(2),admin=key(3);
  Probe mesh(board,radio,clock,random,rtc,tables);mesh.beginTank(&fs);
  mesh.admins.push_back(admin);mesh.advert(a);mesh.advert(b);mesh.advert(admin);
  assert(mesh.stockAdverts==3 && mesh.admins.size()==1);
  mesh.text(a,"password reset");assert(mesh.stockData==0);
  mesh.text(admin,"get name");assert(mesh.stockData==1 && mesh.lastAdmin.matches(admin.pub_key));
  mesh.text(admin,"LFG1|REQ|3|3|2|AdminFish");assert(mesh.stockData==1);
  mesh.text(a,"LFG1|REQ|7|8|2|Water");
  mesh.text(b,"LFG1|COMMIT|7|8|C|1|2|3|65535|H1|220|120|6|S1|13");
  assert(!mesh.receiver.nextEvent()); // full key, not same-prefix/tx/fish binding
  mesh.text(a,"LFG1|COMMIT|7|8|C|1|2|3|65535|H1|220|120|6|S1|13");
  assert(mesh.receiver.nextEvent());assert(mesh.receiver.data.slots[1].species==13);
  uint32_t snapshot=mesh.receiver.nextEvent();
  for(int i=10;i<70;++i)mesh.advert(key(i));
  assert(mesh.find(a)>=0 && mesh.find(admin)>=0); // active slots survive peer churn
  now=5000;mesh.tickTank();assert(mesh.receiver.data.slots[1].attempts==1);
  auto returned=mesh.sent[mesh.sent.size()-1];
  assert(!memcmp(returned.target,a.pub_key,32));
  assert(std::string((char*)returned.payload.data()+5,returned.payload.size()-5).find("AQUARIUM1")!=std::string::npos);
  assert(mesh.defaultFloods>0); // no tank ever connected
  uint32_t hash=0;mesh::Utils::sha256((uint8_t*)&hash,4,returned.payload.data(),returned.payload.size(),mesh.self_id.pub_key,32);
  mesh.onAckRecv(nullptr,hash);assert(mesh.receiver.data.slots[1].state==lrx::Delivered);
  assert(mesh.receiver.nextEvent()==snapshot); // return does not remove the copy
  uint32_t ack=123,timeout=0;mesh.busy(4);assert(!mesh.send(a.pub_key,"test",ack,timeout)&&ack==123);
  mesh.busy(0);mesh.noPackets=true;assert(!mesh.send(a.pub_key,"test",ack,timeout));mesh.noPackets=false;
  uint8_t path[2]={1,2};int index=mesh.find(a);
  mesh.onPeerPathRecv(nullptr,index,a.pub_key,path,2,0,nullptr,0);
  assert(mesh.send(a.pub_key,"test",ack,timeout));assert(!mesh.sent.back().flood && timeout==3050);
  mesh.text(a,"not CLI",3);assert(mesh.stockData==1);
  // The admin also appears in the application table; PATH and non-LFG CLI
  // still reach the original ACL implementation exactly once.
  index=mesh.find(admin);mesh.onPeerPathRecv(nullptr,index,admin.pub_key,path,2,0,nullptr,0);assert(mesh.stockPaths==1);
  Probe reboot(board,radio,clock,random,rtc,tables);now=6000;reboot.beginTank(&fs);
  assert(reboot.find(a)>=0 && reboot.receiver.nextEvent()==snapshot);
  reboot.text(a,"LFG1|COMMIT|7|8");assert(reboot.receiver.data.slots[1].species==13);
  // Corrupt/torn storage fails closed for intake, not stock admin/forwarder.
  for(auto& entry:fs.files)entry.second.assign(3,0);
  Probe corrupt(board,radio,clock,random,rtc,tables);corrupt.beginTank(&fs);
  assert(!corrupt.receiver.healthy());corrupt.admins.push_back(admin);corrupt.text(admin,"get name");assert(corrupt.stockData==1);
  std::cout<<"PASS production repeater: full-key/ACL dispatch, peer churn, Water durability, disconnected returns, reboot, congestion, scoped/direct routing and corruption\n";
  lrx::TankWire wire;uint8_t request[]={ '<',3,0,1,2,3 };
  for(unsigned i=0;i<sizeof(request);++i)assert(wire.feed(request[i],now,false)==(i==5?lrx::TankWire::Frame:lrx::TankWire::None));
  assert(wire.size==3&&!memcmp(wire.frame,request+3,3));
  wire.feed('<',now,false);wire.feed(21,now,false);wire.feed(0,now,false);
  for(int i=0;i<21;++i)assert(wire.feed('x',now,false)==lrx::TankWire::None);
  for(char c:std::string("get name\r")) {auto result=wire.feed(c,now,true);if(c=='\r')assert(result==lrx::TankWire::Line&&!strcmp(wire.line,"get name"));}
  for(int i=0;i<200;++i)wire.feed('x',now,true);assert(wire.feed('\r',now,true)==lrx::TankWire::None);
  wire.feed('<',now,false);wire.expire(now+5000);assert(!wire.active());
  lrx::TankReply reply;uint8_t response[]={1,2};assert(reply.set(response,2));assert(!reply.set(response,2));
  reply.sent(1);assert(reply.remaining()==4&&reply.bytes()[0]==2);reply.sent(4);assert(!reply.pending());
  std::cout<<"PASS bounded production stream framing: fragments, CLI, overflow, timeout and partial-write backpressure\n";
  lrx::TankAuth auth;uint8_t nonce[32]={1},proof[32]={2};auth.begin(nonce,proof,now);
  auth.feed(2);assert(!auth.ready()&&!auth.wantsProof());auth.sent(1);assert(auth.remaining()==35);
  auth.sent(35);assert(auth.wantsProof());for(auto c:proof)auth.feed(c);assert(!auth.ready());auth.sent(1);assert(!auth.ready());auth.sent(1);assert(auth.ready());
  auth.begin(nonce,proof,now);auth.sent(36);for(int i=0;i<32;++i)auth.feed(0);assert(auth.bytes()[0]=='N');auth.sent(2);assert(auth.rejected());
  auth.begin(nonce,proof,0xfffffff0U);assert(auth.timedOut(0xfffffff0U+5000U));
  std::cout<<"PASS production LRXB authentication state: partial writes/proof, no preauth acceptance, wrong proof and wrap-safe timeout\n";
}
