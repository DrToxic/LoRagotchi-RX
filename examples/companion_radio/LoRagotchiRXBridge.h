#pragma once
#ifdef LORAGOTCHI_RX
#include <stddef.h>
#include <stdint.h>
void loragotchiRXBegin();
void loragotchiRXStatus(uint8_t& uploads,uint8_t& returns,uint8_t& failed,bool& healthy);
void loragotchiRXTick();
bool loragotchiRXReceive(const uint8_t* owner,const char* text);
void loragotchiRXAck(uint32_t hash);
size_t loragotchiRXCommand(const uint8_t* in,size_t len,uint8_t* out,const uint8_t* identity);
#endif
