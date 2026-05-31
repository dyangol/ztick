#include <stdint.h>

#include "common.h"
#include "target_task_d.h"
#include "../bootstrap/rtos.h"
#include "../drivers/io.h"
#include "../lib/mem_probe.h"
#include "../lib/pipe.h"
#include "../lib/sprint.h"

#pragma codeseg CODE

#define TASK_D_PAGE_SIZE 0x4000u
#define TASK_D_PAGE_LAST_OFFSET 0x3FFFu
#define TASK_D_CHUNK_MAX 0xFFu

static uint16_t task_d_page_base(uint8_t page)
{
    return (uint16_t)((uint16_t)page << 14);
}

static void task_d_emit_result(uint16_t range_start, uint16_t range_end)
{
    uint8_t line[64];
    sprint_t out;

    sprint_begin(&out, line, (uint8_t)sizeof(line));
    (void)sprint_cstr(&out, (const uint8_t *)"d ");
    (void)sprint_cstr(&out, (g_slot_probe_fail == 0u) ? (const uint8_t *)"OK" : (const uint8_t *)"FAIL");
    (void)sprint_cstr(&out, (const uint8_t *)" range=0x");
    (void)sprint_hex16(&out, range_start);
    (void)sprint_cstr(&out, (const uint8_t *)"-0x");
    (void)sprint_hex16(&out, range_end);

    if (sprint_ok(&out) != 0u) {
        sprint_emit_line(&out);
    } else {
        pipe_write_cstr((const uint8_t *)"d ERR msg-overflow");
        pipe_newline();
    }
}

static void task_d_emit_skip_already_mapped(uint16_t range_start, uint16_t range_end)
{
    uint8_t line[64];
    sprint_t out;

    sprint_begin(&out, line, (uint8_t)sizeof(line));
    (void)sprint_cstr(&out, (const uint8_t *)"d SKIP already-mapped range=0x");
    (void)sprint_hex16(&out, range_start);
    (void)sprint_cstr(&out, (const uint8_t *)"-0x");
    (void)sprint_hex16(&out, range_end);

    if (sprint_ok(&out) != 0u) {
        sprint_emit_line(&out);
    } else {
        pipe_write_cstr((const uint8_t *)"d ERR msg-overflow");
        pipe_newline();
    }
}

void main_d(void)
{
    uint8_t shift = (uint8_t)(TASK_D_PAGE * 2u);
    uint8_t mask = (uint8_t)(0x03u << shift);
    uint8_t mapped_slot;
    uint16_t page_base = task_d_page_base((uint8_t)TASK_D_PAGE);
    uint16_t range_start = page_base;
    uint16_t range_end = (uint16_t)(page_base + (uint16_t)TASK_D_PAGE_LAST_OFFSET);
    uint16_t remaining = (uint16_t)TASK_D_PAGE_SIZE;
    uint16_t chunk_base = page_base;

    g_slot_probe_value = (uint8_t)TASK_D_VALUE;
    g_slot_probe_safe_sp = (uint16_t)TASK_D_SAFE_SP;
    g_slot_probe_exec_addr = (uint16_t)TASK_D_EXEC_ADDR;
    g_slot_probe_psr_port = (uint8_t)PPI_PSR_PORT;

    if (rtos_task_stop_requested() == 0u) {
        g_slot_probe_psr_old = io_read_port((uint8_t)PPI_PSR_PORT);
        mapped_slot = (uint8_t)((g_slot_probe_psr_old >> shift) & 0x03u);
        if (mapped_slot == ((uint8_t)TASK_D_SLOT & 0x03u)) {
            task_d_emit_skip_already_mapped(range_start, range_end);
            task_exit();
            return;
        }

        g_slot_probe_psr_new = (uint8_t)((g_slot_probe_psr_old & (uint8_t)(~mask)) | (uint8_t)(((uint8_t)TASK_D_SLOT & 0x03u) << shift));

        while (remaining > 0u) {
            uint8_t chunk_len = (remaining > (uint16_t)TASK_D_CHUNK_MAX) ? (uint8_t)TASK_D_CHUNK_MAX : (uint8_t)remaining;

            g_slot_probe_base_addr = chunk_base;
            g_slot_probe_length = chunk_len;
            g_slot_probe_fail = 0u;
            g_slot_probe_fail_addr = g_slot_probe_base_addr;
            g_slot_probe_read_value = 0u;

            slot_probe_run();
            if (g_slot_probe_fail != 0u) {
                break;
            }

            chunk_base = (uint16_t)(chunk_base + (uint16_t)chunk_len);
            remaining = (uint16_t)(remaining - (uint16_t)chunk_len);
        }

        task_d_emit_result(range_start, range_end);
    }

    task_exit();
}
