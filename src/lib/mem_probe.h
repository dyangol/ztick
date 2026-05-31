#ifndef MEM_PROBE_H
#define MEM_PROBE_H

#include <stdint.h>

/* Probe mode: restore original byte after check (safe) or leave test byte (unsafe). */
#define MEM_PROBE_MODE_UNSAFE 0u
#define MEM_PROBE_MODE_SAFE 1u

/* Probe result flag values. */
#define MEM_PROBE_RESULT_OK 0u
#define MEM_PROBE_RESULT_FAIL 1u

/* Context registers (set by caller before running a chunk). */
extern volatile uint8_t g_slot_probe_psr_old;
extern volatile uint8_t g_slot_probe_psr_new;
extern volatile uint8_t g_slot_probe_psr_port;
extern volatile uint8_t g_slot_probe_length;
extern volatile uint8_t g_slot_probe_value;
extern volatile uint8_t g_slot_probe_safe_mode;
extern volatile uint16_t g_slot_probe_base_addr;
extern volatile uint16_t g_slot_probe_safe_sp;
extern volatile uint16_t g_slot_probe_exec_addr;
extern volatile uint8_t g_slot_probe_fail;
extern volatile uint16_t g_slot_probe_fail_addr;
extern volatile uint8_t g_slot_probe_read_value;

/* Configure persistent probe parameters. */
void mem_probe_configure(uint8_t value, uint8_t safe_mode, uint16_t safe_sp, uint16_t exec_addr, uint8_t psr_port);

/* Prepare one chunk (base+len) and clear previous result fields. */
void mem_probe_prepare_chunk(uint16_t base_addr, uint8_t length);

/* Execute currently prepared chunk. */
void mem_probe_run(void);

/* Backward-compat alias for existing callers. */
#define slot_probe_run mem_probe_run

#endif
