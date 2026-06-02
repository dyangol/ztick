#include <stdint.h>

#include "rchk.h"

#pragma codeseg CODE

/* The C side only stages parameters and reads back results; rchk.s owns the actual byte check. */
volatile uint8_t g_rchk_psr_old;
volatile uint8_t g_rchk_psr_new;
volatile uint8_t g_rchk_psr_port;
volatile uint8_t g_rchk_length;
volatile uint8_t g_rchk_value;
volatile uint8_t g_rchk_safe_mode;
volatile uint16_t g_rchk_base_addr;
volatile uint16_t g_rchk_safe_sp;
volatile uint16_t g_rchk_exec_addr;

/* Internal runtime scratch (owned by rchk.s). */
volatile uint16_t g_rchk_saved_sp;

/* Output/result fields produced by rchk.s. */
volatile uint8_t g_rchk_fail;
volatile uint16_t g_rchk_fail_addr;
volatile uint8_t g_rchk_read_value;

void rchk_configure(uint8_t value, uint8_t safe_mode, uint16_t safe_sp, uint16_t exec_addr, uint8_t psr_port)
{
    /* These values are latched once per run so the assembler trampoline can read them directly. */
    g_rchk_value = value;
    g_rchk_safe_mode = safe_mode;
    g_rchk_safe_sp = safe_sp;
    g_rchk_exec_addr = exec_addr;
    g_rchk_psr_port = psr_port;
}

void rchk_prepare_chunk(uint16_t base_addr, uint8_t length)
{
    /* Reset the result fields before each chunk; the assembly stub overwrites them on failure. */
    g_rchk_base_addr = base_addr;
    g_rchk_length = length;
    g_rchk_fail = (uint8_t)RCHK_RESULT_OK;
    g_rchk_fail_addr = base_addr;
    g_rchk_read_value = 0u;
}
