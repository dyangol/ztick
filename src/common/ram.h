#ifndef RAM_H
#define RAM_H

#include <stdint.h>

extern volatile const uint8_t *g_ram_exec_src;
extern volatile uint16_t g_ram_exec_len;
extern volatile uint16_t g_ram_exec_addr;

void ram_exec_run(void);

#endif
