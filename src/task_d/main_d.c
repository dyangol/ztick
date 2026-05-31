#include <stdint.h>

#include "common.h"
#include "target_task_d.h"
#include "../bootstrap/rtos.h"
#include "../drivers/io.h"
#include "../lib/mem_probe.h"
#include "../lib/pipe.h"

#pragma codeseg CODE

static uint16_t task_d_page_base(uint8_t page)
{
    return (uint16_t)((uint16_t)page << 14);
}

static uint8_t task_d_clamp_length(uint16_t offset, uint8_t length)
{
    uint16_t max_len = (uint16_t)(0x4000u - offset);

    if (length == 0u) {
        return 1u;
    }
    if (max_len < (uint16_t)length) {
        return (uint8_t)max_len;
    }
    return length;
}

static uint8_t task_d_hex_digit(uint8_t nibble)
{
    nibble &= 0x0Fu;
    return (nibble < 10u) ? (uint8_t)('0' + nibble) : (uint8_t)('A' + (nibble - 10u));
}

static void task_d_line_append_hex8(uint8_t *line, uint8_t *io_len, uint8_t value)
{
    uint8_t len = *io_len;
    line[len++] = task_d_hex_digit((uint8_t)(value >> 4));
    line[len++] = task_d_hex_digit(value);
    line[len] = 0u;
    *io_len = len;
}

static void task_d_line_append_hex16(uint8_t *line, uint8_t *io_len, uint16_t value)
{
    uint8_t len = *io_len;
    line[len++] = task_d_hex_digit((uint8_t)(value >> 12));
    line[len++] = task_d_hex_digit((uint8_t)(value >> 8));
    line[len++] = task_d_hex_digit((uint8_t)(value >> 4));
    line[len++] = task_d_hex_digit((uint8_t)value);
    line[len] = 0u;
    *io_len = len;
}

static void task_d_emit_result(void)
{
    uint8_t line[64];
    uint8_t len = 0u;
    uint16_t report_addr = (g_slot_probe_fail == 0u) ? g_slot_probe_base_addr : g_slot_probe_fail_addr;

    line[len++] = (uint8_t)'d';
    line[len++] = (uint8_t)' ';
    line[len++] = (g_slot_probe_fail == 0u) ? (uint8_t)'O' : (uint8_t)'F';
    line[len++] = (g_slot_probe_fail == 0u) ? (uint8_t)'K' : (uint8_t)'A';
    if (g_slot_probe_fail != 0u) {
        line[len++] = (uint8_t)'I';
        line[len++] = (uint8_t)'L';
    }
    line[len++] = (uint8_t)' ';
    line[len++] = (uint8_t)'a';
    line[len++] = (uint8_t)'d';
    line[len++] = (uint8_t)'d';
    line[len++] = (uint8_t)'r';
    line[len++] = (uint8_t)'=';
    line[len++] = (uint8_t)'0';
    line[len++] = (uint8_t)'x';
    line[len] = 0u;
    task_d_line_append_hex16(line, &len, report_addr);
    line[len++] = (uint8_t)' ';
    line[len++] = (uint8_t)'r';
    line[len++] = (uint8_t)'d';
    line[len++] = (uint8_t)'=';
    line[len++] = (uint8_t)'0';
    line[len++] = (uint8_t)'x';
    line[len] = 0u;
    task_d_line_append_hex8(line, &len, g_slot_probe_read_value);

    pipe_write_cstr(line);
    pipe_newline();
}

void main_d(void)
{
    uint8_t wait = 0u;
    uint8_t shift = (uint8_t)(TASK_D_PAGE * 2u);
    uint8_t mask = (uint8_t)(0x03u << shift);
    uint16_t offset = (uint16_t)(TASK_D_OFFSET & 0x3FFFu);
    uint8_t length = task_d_clamp_length(offset, (uint8_t)TASK_D_LENGTH);
    uint16_t base = (uint16_t)(task_d_page_base((uint8_t)TASK_D_PAGE) + offset);

    g_slot_probe_base_addr = base;
    g_slot_probe_length = length;
    g_slot_probe_value = (uint8_t)TASK_D_VALUE;
    g_slot_probe_safe_sp = (uint16_t)TASK_D_SAFE_SP;
    g_slot_probe_exec_addr = (uint16_t)TASK_D_EXEC_ADDR;
    g_slot_probe_psr_port = (uint8_t)PPI_PSR_PORT;

    while (1) {
        if (rtos_task_stop_requested() != 0u) {
            break;
        }

        g_slot_probe_psr_old = io_read_port((uint8_t)PPI_PSR_PORT);
        g_slot_probe_psr_new = (uint8_t)((g_slot_probe_psr_old & (uint8_t)(~mask)) | (uint8_t)(((uint8_t)TASK_D_SLOT & 0x03u) << shift));

        g_slot_probe_fail = 0u;
        g_slot_probe_fail_addr = g_slot_probe_base_addr;
        g_slot_probe_read_value = 0u;

        slot_probe_run();
        task_d_emit_result();

        for (wait = 0u; wait != 0xFFu; ++wait) {
            CPU_NOP();
            if (rtos_task_stop_requested() != 0u) {
                break;
            }
        }
    }

    task_exit();
}
