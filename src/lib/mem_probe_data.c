#include <stdint.h>

#include "mem_probe.h"

#pragma codeseg CODE

volatile uint8_t g_slot_probe_psr_old;
volatile uint8_t g_slot_probe_psr_new;
volatile uint8_t g_slot_probe_psr_port;
volatile uint8_t g_slot_probe_length;
volatile uint8_t g_slot_probe_value;
volatile uint16_t g_slot_probe_base_addr;
volatile uint16_t g_slot_probe_safe_sp;
volatile uint16_t g_slot_probe_exec_addr;
volatile uint16_t g_slot_probe_saved_sp;
volatile uint8_t g_slot_probe_fail;
volatile uint16_t g_slot_probe_fail_addr;
volatile uint8_t g_slot_probe_read_value;
