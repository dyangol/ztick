#include <stdint.h>

#include "common.h"
#include "target_task_d.h"
#include "../bootstrap/rtos.h"
#include "../drivers/io.h"
#include "../lib/mem_probe.h"
#include "../lib/pipe.h"
#include "../lib/sprint.h"
#include "args_d.h"

#pragma codeseg CODE

#define TASK_D_CHUNK_MAX 0xFFu

static uint16_t task_d_page_base(uint8_t page)
{
    return (uint16_t)((uint16_t)page << 14);
}

static void task_d_emit_result(uint16_t range_start, uint16_t range_end, uint8_t safe_mode)
{
    uint8_t line[64];
    sprint_t out;

    sprint_begin(&out, line, (uint8_t)sizeof(line));
    (void)sprint_cstr(&out, (const uint8_t *)"d ");
    (void)sprint_cstr(&out, (g_slot_probe_fail == 0u) ? (const uint8_t *)"OK" : (const uint8_t *)"Error");
    (void)sprint_cstr(&out, (const uint8_t *)" mode=");
    (void)sprint_cstr(&out, (safe_mode != 0u) ? (const uint8_t *)"safe" : (const uint8_t *)"unsafe");
    (void)sprint_cstr(&out, (const uint8_t *)" slot=");
    (void)sprint_u8_dec(&out, (uint8_t)TASK_D_SLOT);
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
    pipe_flush();
}

static void task_d_emit_config_error(const uint8_t *reason)
{
    pipe_write_cstr((const uint8_t *)"d ERR ");
    pipe_write_cstr(reason);
    pipe_newline();
    pipe_flush();
}

void main_d(void)
{
    uint8_t shift = (uint8_t)(TASK_D_PAGE * 2u);
    uint8_t mask = (uint8_t)(0x03u << shift);
    uint8_t mapped_slot;
    uint16_t page_base = task_d_page_base((uint8_t)TASK_D_PAGE);
    volatile uint16_t cfg_allowed_start = (uint16_t)(TASK_D_ALLOWED_START & 0x3FFFu);
    volatile uint16_t cfg_allowed_end = (uint16_t)(TASK_D_ALLOWED_END & 0x3FFFu);
    volatile uint16_t cfg_req_start = (uint16_t)(TASK_D_OFFSET & 0x3FFFu);
    volatile uint16_t cfg_req_len = (uint16_t)TASK_D_LENGTH;
    volatile uint8_t cfg_safe_mode = (uint8_t)TASK_D_SAFE_MODE;
    uint16_t allowed_start_off = cfg_allowed_start;
    uint16_t allowed_end_off = cfg_allowed_end;
    uint16_t req_start_off = cfg_req_start;
    uint16_t req_len_cfg = cfg_req_len;
    uint16_t max_len;
    uint16_t req_len;
    uint16_t range_start;
    uint16_t range_end;
    uint16_t remaining;
    uint16_t chunk_base;
    uint8_t safe_mode = task_d_safe_mode_resolve((cfg_safe_mode != 0u) ? 1u : 0u);

    if (allowed_start_off > allowed_end_off) {
        task_d_emit_config_error((const uint8_t *)"invalid-allowed-range");
        task_exit();
        return;
    }
    if ((req_start_off < allowed_start_off) || (req_start_off > allowed_end_off)) {
        task_d_emit_config_error((const uint8_t *)"start-outside-allowed");
        task_exit();
        return;
    }

    max_len = (uint16_t)(allowed_end_off - req_start_off + 1u);
    if ((req_len_cfg == 0u) || (req_len_cfg > max_len)) {
        req_len = max_len;
    } else {
        req_len = req_len_cfg;
    }

    range_start = (uint16_t)(page_base + req_start_off);
    range_end = (uint16_t)(range_start + req_len - 1u);
    remaining = req_len;
    chunk_base = range_start;

    mem_probe_configure((uint8_t)TASK_D_VALUE, safe_mode, (uint16_t)TASK_D_SAFE_SP, (uint16_t)TASK_D_EXEC_ADDR, (uint8_t)PPI_PSR_PORT);

    if (rtos_task_stop_requested() == 0u) {
        g_slot_probe_psr_old = io_read_port((uint8_t)PPI_PSR_PORT);
        mapped_slot = (uint8_t)((g_slot_probe_psr_old >> shift) & 0x03u);
        if (safe_mode == 0u) {
            if (mapped_slot == ((uint8_t)TASK_D_SLOT & 0x03u)) {
                task_d_emit_config_error((const uint8_t *)"unsafe-same-slot");
                task_exit();
                return;
            }
        }
        g_slot_probe_psr_new = (uint8_t)((g_slot_probe_psr_old & (uint8_t)(~mask)) | (uint8_t)(((uint8_t)TASK_D_SLOT & 0x03u) << shift));

        while (remaining > 0u) {
            uint8_t chunk_len = (remaining > (uint16_t)TASK_D_CHUNK_MAX) ? (uint8_t)TASK_D_CHUNK_MAX : (uint8_t)remaining;

            mem_probe_prepare_chunk(chunk_base, chunk_len);

            mem_probe_run();
            if (g_slot_probe_fail != 0u) {
                break;
            }

            chunk_base = (uint16_t)(chunk_base + (uint16_t)chunk_len);
            remaining = (uint16_t)(remaining - (uint16_t)chunk_len);
        }

        task_d_emit_result(range_start, range_end, safe_mode);
    }

    task_exit();
}
