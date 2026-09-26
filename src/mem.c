#include "os-cfg.h"

#ifdef MM_PAGING
/* MM_PAGING mode: provide stubs */
#include "mem.h"
#include <stdio.h>

void init_mem(void) { }
addr_t alloc_mem(uint32_t size, struct pcb_t * proc) { return 0; }
int free_mem(addr_t address, struct pcb_t * proc) { return 0; }
int read_mem(addr_t address, struct pcb_t * proc, BYTE * data) { return 1; }
int write_mem(addr_t address, struct pcb_t * proc, BYTE data) { return 1; }
void dump(void) { printf("dump() not available in MM_PAGING mode\n"); }

#else
#error "MM_PAGING must be defined to compile this OS."
#endif
