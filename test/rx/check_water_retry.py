#!/usr/bin/env python3
"""Actual DX frozen retry -> actual RX -> PC decoder; mocked radio/flash only."""
from pathlib import Path
import subprocess, sys, tempfile
root=Path(__file__).resolve().parents[2]
dx=root.parent/'loragotchi-dx-water'
pc=root.parent/'loragotchi-aquarium-water'
sys.path.insert(0,str(dx/'test/ui'))
import check_dx_handlers as f
sys.path.insert(0,str(pc))
from tank.protocol import decode_event
code=f.pre+f.structs+f.globals+f.receive+f.methods+r'''
#include "examples/companion_radio/LoRagotchiRX.h"
#include <fstream>
struct IO:lrx::IO {
 bool save(const lrx::Journal& j){return lrx::valid(j);}
 bool send(const uint8_t*,const char*,uint32_t& ack,uint32_t& timeout){ack=1;timeout=10000;return true;}
};
int main(int argc,char** argv){
 MyMesh m;ContactInfo dest;dest.id.pub_key[0]=9;m.contacts={dest};
 uint8_t owner[32]={1};IO io;lrx::Receiver r(io);assert(r.begin(nullptr,0));
 r.receive(owner,"LFG1|REQ|7|8|2|Water",0);
 loragotchiDXHome={};loragotchiDXHome.pending=true;loragotchiDXHome.destinationKnown=true;
 memcpy(loragotchiDXHome.destinationKey,dest.id.pub_key,32);loragotchiDXHome.txid=7;loragotchiDXHome.fishid=8;
 loragotchiDXLocalSpecies=13;loragotchiDXLocalHasColorGenes=true;
 loragotchiDXLocalColorIndex=220;loragotchiDXLocalScalesColorIndex=120;loragotchiDXLocalEyesColorIndex=6;
 loragotchiDXVisit={};now=100;m.estimate=30000;m.receive(dest,"LFG1|ACK|7|8|S1");
 std::string first=m.sent.back();assert(first.find("|S1|13")!=std::string::npos && first.find("|H1|")!=std::string::npos);
 loragotchiDXLocalSpecies=1;loragotchiDXLocalColorIndex=0;now=loragotchiDXVisit.due;
 m.serviceLoRaFishVisit();assert(m.sent.back()==first);r.receive(owner,m.sent.back().c_str(),now);
 assert(r.nextEvent());r.tick(now+4000);assert(r.data.slots[0].attempts==1);
 uint8_t out[84];assert(r.event(r.nextEvent(),out,true)==84);std::ofstream(argv[1],std::ios::binary).write((char*)out,84);
}
'''
with tempfile.TemporaryDirectory(prefix='water-e2e-') as d:
    p=Path(d);(p/'test.cpp').write_text(code)
    subprocess.run(['g++','-std=c++17','-I',str(dx),'-I',str(root),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
    subprocess.run([str(p/'test'),str(p/'event')],check=True)
    e=decode_event('03'+'00'*31,(p/'event').read_bytes())
    assert (e.species_id,e.body_gene,e.scales_gene,e.eyes_gene)==(13,220,120,6)
    print('PASS lost rich COMMIT: actual DX retry -> RX capture/return -> PC Water decode')
