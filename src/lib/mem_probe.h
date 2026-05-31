#ifndef MEM_PROBE_H
#define MEM_PROBE_H

#include <stdint.h>

extern volatile uint8_t g_slot_probe_psr_old;
extern volatile uint8_t g_slot_probe_psr_new;
extern volatile uint8_t g_slot_probe_psr_port;
extern volatile uint8_t g_slot_probe_length;
extern volatile uint8_t g_slot_probe_value;
extern volatile uint16_t g_slot_probe_base_addr;
extern volatile uint16_t g_slot_probe_safe_sp;
extern volatile uint16_t g_slot_probe_exec_addr;
extern volatile uint16_t g_slot_probe_saved_sp;
extern volatile uint8_t g_slot_probe_fail;
extern volatile uint16_t g_slot_probe_fail_addr;
extern volatile uint8_t g_slot_probe_read_value;

void slot_probe_run(void);

#endif
