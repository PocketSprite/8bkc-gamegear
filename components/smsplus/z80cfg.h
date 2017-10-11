
#include "shared.h"

static inline uint8_t cpu_readmembyte(uint16_t Addr) {
	return cpu_readmap[(Addr) >> 13][(Addr) & 0x1FFF];
}


#define readbyte_internal(Addr) cpu_readmembyte(Addr)
#define readbyte(Addr) cpu_readmembyte(Addr)
#define writebyte(Addr,Data) cpu_writemem16(Addr, Data)
#define writeport(Addr,Data,tstates) cpu_writeport(Addr, Data)
#define readport(Addr, tstates) cpu_readport(Addr)
#define opcode_fetch(Addr) cpu_readmembyte(Addr)


