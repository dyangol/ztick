#include <stdint.h>

#include "mem_probe.h"

#pragma codeseg CODE

/* Input/config fields consumed by mem_probe.s */
volatile uint8_t g_slot_probe_psr_old;
volatile uint8_t g_slot_probe_psr_new;
volatile uint8_t g_slot_probe_psr_port;
volatile uint8_t g_slot_probe_length;
volatile uint8_t g_slot_probe_value;
volatile uint8_t g_slot_probe_safe_mode;
volatile uint16_t g_slot_probe_base_addr;
volatile uint16_t g_slot_probe_safe_sp;
volatile uint16_t g_slot_probe_exec_addr;

/* Internal runtime scratch (owned by mem_probe.s). */
volatile uint16_t g_slot_probe_saved_sp;

/* Output/result fields produced by mem_probe.s. */
volatile uint8_t g_slot_probe_fail;
volatile uint16_t g_slot_probe_fail_addr;
volatile uint8_t g_slot_probe_read_value;

void mem_probe_configure(uint8_t value, uint8_t safe_mode, uint16_t safe_sp, uint16_t exec_addr, uint8_t psr_port)
{
    g_slot_probe_value = value;
    g_slot_probe_safe_mode = safe_mode;
    g_slot_probe_safe_sp = safe_sp;
    g_slot_probe_exec_addr = exec_addr;
    g_slot_probe_psr_port = psr_port;
}

void mem_probe_prepare_chunk(uint16_t base_addr, uint8_t length)
{
    g_slot_probe_base_addr = base_addr;
    g_slot_probe_length = length;
    g_slot_probe_fail = (uint8_t)MEM_PROBE_RESULT_OK;
    g_slot_probe_fail_addr = base_addr;
    g_slot_probe_read_value = 0u;
}
