#ifndef RCHK_H
#define RCHK_H

#include <stdint.h>

/* Probe mode: restore original byte after check (safe) or leave test byte (unsafe). */
#define RCHK_MODE_UNSAFE 0u
#define RCHK_MODE_SAFE 1u

/* Probe result flag values. */
#define RCHK_RESULT_OK 0u
#define RCHK_RESULT_FAIL 1u

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
void rchk_configure(uint8_t value, uint8_t safe_mode, uint16_t safe_sp, uint16_t exec_addr, uint8_t psr_port);

/* Prepare one chunk (base+len) and clear previous result fields. */
void rchk_prepare_chunk(uint16_t base_addr, uint8_t length);

/* Execute currently prepared chunk. */
void rchk_run(void);

#endif
