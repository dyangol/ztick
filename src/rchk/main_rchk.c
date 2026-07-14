#include <stdint.h>

#include "common.h"
#include "target_rchk.h"
#include "../bootstrap/rtos.h"
#include "../drivers/io.h"
#include "rchk.h"
#include "../lib/pipe.h"
#include "../lib/sprint.h"
#include "../lib/task.h"
#include "args_rchk.h"

#pragma codeseg CODE

/* Chunking and progress are tuned to keep the RAM trampoline small and the console readable. */
#define RCHK_CHUNK_MAX 0xFFu
#define RCHK_PROGRESS_STEP 5u
#define RCHK_PROGRESS_MIN_LEN 512u

/* Task identity and start-argument surface exposed to the loader/shell. */
static const uint8_t g_task_name_rchk[] = "rchk";
static const uint8_t g_rchk_start_args_usage[] = "safe|unsafe";

void main_rchk(void);

/* Task spec: auto-started checker with a safe/unsafe start-argument selector. */
const task_spec_t g_task_spec_rchk = {
    g_task_name_rchk,
    main_rchk,
    TASK_WEIGHT_MIN,
    (uint8_t)TASK_TTY_AUTO,
    g_rchk_start_args_usage,
    rchk_start_configure,
    rchk_start_reset
};

static uint16_t rchk_page_base(uint8_t page)
{
    /* Each ROM page occupies a 16 KiB window in the mapped address space. */
    return (uint16_t)((uint16_t)page << 14);
}

static void rchk_emit_result(uint8_t page, uint8_t slot, uint16_t range_start, uint16_t range_end, uint8_t safe_mode)
{
    uint8_t line[64];
    sprint_t out;

    sprint_begin(&out, line, (uint8_t)sizeof(line));
    (void)sprint_cstr(&out, (const uint8_t *)"rchk ");
    (void)sprint_cstr(&out, (g_rchk_fail == 0u) ? (const uint8_t *)"OK" : (const uint8_t *)"Error");
    (void)sprint_cstr(&out, (const uint8_t *)" mode=");
    (void)sprint_cstr(&out, (safe_mode != 0u) ? (const uint8_t *)"safe" : (const uint8_t *)"unsafe");
    (void)sprint_cstr(&out, (const uint8_t *)" page=");
    (void)sprint_u8_dec(&out, page);
    (void)sprint_cstr(&out, (const uint8_t *)" slot=");
    (void)sprint_u8_dec(&out, slot);
    (void)sprint_cstr(&out, (const uint8_t *)" range=0x");
    (void)sprint_hex16(&out, range_start);
    (void)sprint_cstr(&out, (const uint8_t *)"-0x");
    (void)sprint_hex16(&out, range_end);

    if (sprint_ok(&out) != 0u) {
        sprint_emit_line(&out);
    } else {
        pipe_write_cstr((const uint8_t *)"rchk ERR msg-overflow");
        pipe_newline();
    }
    pipe_flush();
}

static void rchk_emit_config_error(uint8_t page, const uint8_t *reason)
{
    uint8_t line[48];
    sprint_t out;

    sprint_begin(&out, line, (uint8_t)sizeof(line));
    (void)sprint_cstr(&out, (const uint8_t *)"rchk ERR page=");
    (void)sprint_u8_dec(&out, page);
    (void)sprint_cstr(&out, (const uint8_t *)" ");
    (void)sprint_cstr(&out, reason);

    if (sprint_ok(&out) != 0u) {
        sprint_emit_line(&out);
    } else {
        pipe_write_cstr((const uint8_t *)"rchk ERR msg-overflow");
        pipe_newline();
    }
    pipe_flush();
}

static void rchk_emit_progress(uint8_t page, uint8_t percent)
{
    uint8_t line[32];
    sprint_t out;

    sprint_begin(&out, line, (uint8_t)sizeof(line));
    (void)sprint_cstr(&out, (const uint8_t *)"rchk PROGRESS page=");
    (void)sprint_u8_dec(&out, page);
    (void)sprint_cstr(&out, (const uint8_t *)" ");
    (void)sprint_u8_dec(&out, percent);
    (void)sprint_cstr(&out, (const uint8_t *)"%");

    if (sprint_ok(&out) != 0u) {
        sprint_emit_line(&out);
    } else {
        pipe_write_cstr((const uint8_t *)"rchk ERR progress-msg-overflow");
        pipe_newline();
    }
}

/* Runs the check for a single g_rchk_tests[] entry. Config errors (an
 * invalid range, or an unsafe-mode request to test the currently-mapped
 * slot) only skip this entry -- they don't abort the rest of the sweep,
 * so one misconfigured page doesn't hide results for the others. */
static void rchk_run_one(const rchk_test_entry_t *entry, uint8_t safe_mode)
{
    uint8_t page = entry->page;
    uint8_t slot = (uint8_t)(entry->slot & 0x03u);
    uint8_t shift = (uint8_t)(page * 2u);
    uint8_t mask = (uint8_t)(0x03u << shift);
    uint8_t mapped_slot;
    uint16_t page_base = rchk_page_base(page);
    uint16_t allowed_start_off = (uint16_t)(entry->allowed_start & 0x3FFFu);
    uint16_t allowed_end_off = (uint16_t)(entry->allowed_end & 0x3FFFu);
    uint16_t req_start_off = (uint16_t)(entry->offset & 0x3FFFu);
    uint16_t req_len_cfg = entry->length;
    uint16_t max_len;
    uint16_t req_len;
    uint16_t range_start;
    uint16_t range_end;
    uint16_t remaining;
    uint16_t chunk_base;
    uint8_t progress_enabled;
    uint8_t next_progress = (uint8_t)RCHK_PROGRESS_STEP;
    uint8_t total_chunks = 0u;
    uint8_t done_chunks = 0u;

    /* The build-time target defines the test window; reject impossible setups early. */
    if (allowed_start_off > allowed_end_off) {
        rchk_emit_config_error(page, (const uint8_t *)"invalid-allowed-range");
        return;
    }
    if ((req_start_off < allowed_start_off) || (req_start_off > allowed_end_off)) {
        rchk_emit_config_error(page, (const uint8_t *)"start-outside-allowed");
        return;
    }

    /* Clamp the requested run to the configured window so the check never escapes it. */
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
    progress_enabled = (req_len >= (uint16_t)RCHK_PROGRESS_MIN_LEN) ? 1u : 0u;
    if (progress_enabled != 0u) {
        total_chunks = (uint8_t)((req_len + ((uint16_t)RCHK_CHUNK_MAX - 1u)) / (uint16_t)RCHK_CHUNK_MAX);
    }

    if (rtos_task_stop_requested() != 0u) {
        return;
    }

    /* Remap only this entry's page; unsafe mode refuses to self-test the active slot. */
    g_rchk_psr_old = io_read_port((uint8_t)PPI_PSR_PORT);
    mapped_slot = (uint8_t)((g_rchk_psr_old >> shift) & 0x03u);
    if (safe_mode == 0u) {
        if (mapped_slot == slot) {
            rchk_emit_config_error(page, (const uint8_t *)"unsafe-same-slot");
            return;
        }
    }
    g_rchk_psr_new = (uint8_t)((g_rchk_psr_old & (uint8_t)(~mask)) | (uint8_t)(slot << shift));

    while (remaining > 0u) {
        /* Keep each pass within a 255-byte chunk so the trampoline stays small. */
        uint8_t chunk_len = (remaining > (uint16_t)RCHK_CHUNK_MAX) ? (uint8_t)RCHK_CHUNK_MAX : (uint8_t)remaining;

        rchk_prepare_chunk(chunk_base, chunk_len);

        rchk_run();
        if (g_rchk_fail != 0u) {
            break;
        }

        chunk_base = (uint16_t)(chunk_base + (uint16_t)chunk_len);
        remaining = (uint16_t)(remaining - (uint16_t)chunk_len);

        if (progress_enabled != 0u) {
            uint8_t progress_pct;

            done_chunks++;
            progress_pct = (uint8_t)(((uint16_t)done_chunks * 100u) / (uint16_t)total_chunks);

            /* Emit coarse progress only when a new threshold is crossed. */
            while ((next_progress <= 100u) && (next_progress <= progress_pct)) {
                rchk_emit_progress(page, next_progress);
                next_progress = (uint8_t)(next_progress + (uint8_t)RCHK_PROGRESS_STEP);
            }
        }
    }

    /* Final report reflects the exact checked range and whether any byte mismatched. */
    rchk_emit_result(page, slot, range_start, range_end, safe_mode);
}

void main_rchk(void)
{
    uint8_t safe_mode = rchk_safe_mode_resolve((uint8_t)RCHK_SAFE_MODE);
    /* Copied to a variable (rather than comparing against the RCHK_TEST_COUNT
     * macro directly in the loop condition) so SDCC doesn't fold a
     * single-entry target's trip count into a compile-time constant and warn
     * about it -- harmless either way, but this keeps the build quiet for
     * every target regardless of how many entries it configures. */
    uint8_t count = (uint8_t)RCHK_TEST_COUNT;
    uint8_t i;

    /* Publish the runtime parameters consumed by the RAM-executed assembler
     * stub -- shared by every entry in the sweep below, since they all use
     * the same trampoline/stack location and safe/unsafe mode. */
    rchk_configure((uint8_t)RCHK_VALUE, safe_mode, (uint16_t)RCHK_SAFE_SP, (uint16_t)RCHK_EXEC_ADDR, (uint8_t)PPI_PSR_PORT);

    for (i = 0u; i < count; ++i) {
        if (rtos_task_stop_requested() != 0u) {
            break;
        }
        rchk_run_one(&g_rchk_tests[i], safe_mode);
    }

    task_exit();
}
