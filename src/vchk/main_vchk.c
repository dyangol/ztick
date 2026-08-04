#include <stdint.h>

#include "common.h"
#include "../bootstrap/rtos.h"
#include "../drivers/vdp.h"
#include "../lib/chunk_report.h"
#include "../lib/pipe.h"
#include "../lib/sprint.h"
#include "../lib/task.h"

#pragma codeseg CODE

/* Test byte and chunking are tuned the same way as rchk's RAM sweep. */
#define VCHK_VALUE 0xA5u
#define VCHK_PROGRESS_MIN_LEN 512u
#define VCHK_PROGRESS_CHUNK 64u

static const uint8_t g_task_name_vchk[] = "vchk";
static const uint8_t g_task_vchk_start_args_usage[] = "";

void main_vchk(void);

/* Task spec: manual-start diagnostic, no start args (like rchk, not
 * autostarted -- see BOOT_AUTOSTART in the target manifests). */
const task_spec_t g_task_spec_vchk = {
    g_task_name_vchk,
    main_vchk,
    TASK_WEIGHT_MIN,
    (uint8_t)TASK_TTY_AUTO,
    g_task_vchk_start_args_usage,
    (task_start_args_configure_t)0,
    (task_start_args_reset_t)0
};

typedef struct vchk_region {
    const uint8_t *name;
    uint16_t base;
    uint16_t length;
} vchk_region_t;

/* Named string constants, not inline literals, as the struct-array
 * initializers below -- SDCC emits duplicate/conflicting labels for string
 * literals used directly as static aggregate initializers (confirmed via a
 * build failure: sdasz80 "phase error" / "multiple definitions error" on
 * this exact pattern), unlike the same literals used as plain function-call
 * arguments elsewhere in this file. */
static const uint8_t g_region_name_pattern[] = "pattern";
static const uint8_t g_region_name_names[] = "names";
static const uint8_t g_region_name_colors[] = "colors";
static const uint8_t g_region_name_sprattr[] = "sprattr";
static const uint8_t g_region_name_sprpat[] = "sprpat";

/* Same five regions and sizes vdp_init_screen1() itself fills. */
static const vchk_region_t g_vchk_regions[] = {
    { g_region_name_pattern, VDP_PATTERN_TABLE_ADDR,  0x0800u },
    { g_region_name_names,   VDP_NAME_TABLE_ADDR,     0x0300u },
    { g_region_name_colors,  VDP_COLOR_TABLE_ADDR,    0x0020u },
    { g_region_name_sprattr, VDP_SPRITE_ATTR_ADDR,    0x0080u },
    { g_region_name_sprpat,  VDP_SPRITE_PATTERN_ADDR, 0x0800u }
};
#define VCHK_REGION_COUNT ((uint8_t)(sizeof(g_vchk_regions) / sizeof(g_vchk_regions[0])))

static void vchk_emit_result(const uint8_t *region, uint16_t range_start, uint16_t range_end,
                              uint16_t total_chunks, uint16_t failed_chunks)
{
    uint8_t line[64];
    sprint_t out;

    sprint_begin(&out, line, (uint8_t)sizeof(line));
    (void)sprint_cstr(&out, (const uint8_t *)"vchk ");
    (void)sprint_cstr(&out, (failed_chunks == 0u) ? (const uint8_t *)"OK" : (const uint8_t *)"Error");
    (void)sprint_cstr(&out, (const uint8_t *)" region=");
    (void)sprint_cstr(&out, region);
    (void)sprint_cstr(&out, (const uint8_t *)" range=0x");
    (void)sprint_hex16(&out, range_start);
    (void)sprint_cstr(&out, (const uint8_t *)"-0x");
    (void)sprint_hex16(&out, range_end);
    chunk_report_append_result(&out, total_chunks, failed_chunks);

    if (sprint_ok(&out) != 0u) {
        sprint_emit_line(&out);
    } else {
        pipe_write_cstr((const uint8_t *)"vchk ERR msg-overflow");
        pipe_newline();
    }
    pipe_flush();
}

static void vchk_emit_mismatch(const uint8_t *region, uint16_t addr, uint8_t expected, uint8_t got)
{
    uint8_t line[56];
    sprint_t out;

    sprint_begin(&out, line, (uint8_t)sizeof(line));
    (void)sprint_cstr(&out, (const uint8_t *)"vchk ERR region=");
    (void)sprint_cstr(&out, region);
    (void)sprint_cstr(&out, (const uint8_t *)" addr=0x");
    (void)sprint_hex16(&out, addr);
    (void)sprint_cstr(&out, (const uint8_t *)" expected=0x");
    (void)sprint_hex8(&out, expected);
    (void)sprint_cstr(&out, (const uint8_t *)" got=0x");
    (void)sprint_hex8(&out, got);

    if (sprint_ok(&out) != 0u) {
        sprint_emit_line(&out);
    } else {
        pipe_write_cstr((const uint8_t *)"vchk ERR msg-overflow");
        pipe_newline();
    }
    pipe_flush();
}

/* One line per VCHK_PROGRESS_CHUNK bytes actually checked, only for regions
 * at or above VCHK_PROGRESS_MIN_LEN -- same convention as rchk's PROGRESS
 * lines (chunk address range plus the fraction of its bytes that
 * mismatched, shared via chunk_report_append_progress()). */
static void vchk_emit_progress(const uint8_t *region, uint16_t chunk_start, uint16_t chunk_end, uint16_t bytes_failed)
{
    /* "vchk PROGRESS region=" (21) + longest region name ("sprattr"/
     * "pattern", 7) + up to 24 more from chunk_report_append_progress()
     * (" 0x"+4+"-0x"+4+" failed="+5) = 41 chars -- line[40] was 2-3 bytes
     * short even before that suffix existed (confirmed on real hardware:
     * every PROGRESS line fell back to "vchk ERR progress-msg-overflow"
     * instead), so this is sized with headroom rather than to the exact
     * worst case. */
    uint8_t line[64];
    sprint_t out;

    sprint_begin(&out, line, (uint8_t)sizeof(line));
    (void)sprint_cstr(&out, (const uint8_t *)"vchk PROGRESS region=");
    (void)sprint_cstr(&out, region);
    chunk_report_append_progress(&out, chunk_start, chunk_end, bytes_failed);

    if (sprint_ok(&out) != 0u) {
        sprint_emit_line(&out);
    } else {
        pipe_write_cstr((const uint8_t *)"vchk ERR progress-msg-overflow");
        pipe_newline();
    }
}

/* Reads the original byte, writes/reads back the test pattern, then always
 * restores the original -- VRAM is actively on screen while xsh runs, so
 * unlike rchk there's no "unsafe" mode that skips the restore. The whole
 * read-write-read-write sequence runs under DI: other tasks (task_b/task_c's
 * activity_indicator animations) also drive the VDP's single shared
 * address/data port pair, and a task switch mid-sequence would leave the
 * VDP's internal address pointer pointing wherever that other task last
 * left it, not where this function expects. */
static uint8_t vchk_test_byte(uint16_t addr, uint8_t *out_got)
{
    uint8_t original;
    uint8_t got;

    CPU_DI();
    original = vdp_read_vram(addr);
    vdp_write_vram(addr, (uint8_t)VCHK_VALUE);
    got = vdp_read_vram(addr);
    vdp_write_vram(addr, original);
    CPU_EI();

    *out_got = got;
    return (got == (uint8_t)VCHK_VALUE) ? 1u : 0u;
}

/* Every byte in the region is tested regardless of earlier mismatches --
 * VCHK_PROGRESS_CHUNK-sized groups are tallied into total_chunks/
 * failed_chunks for the final result line, mirroring rchk's per-chunk
 * counting (see rchk.s/main_rchk.c). Only the first mismatch found gets its
 * own detail line (vchk_emit_mismatch); the rest just count. */
static void vchk_run_region(const vchk_region_t *region)
{
    uint16_t offset;
    uint16_t chunk_start = region->base;
    uint16_t chunk_bytes_failed = 0u;
    uint8_t progress_enabled = (region->length >= (uint16_t)VCHK_PROGRESS_MIN_LEN) ? 1u : 0u;
    uint16_t total_chunks = 0u;
    uint16_t failed_chunks = 0u;
    uint8_t mismatch_reported = 0u;

    for (offset = 0u; offset < region->length; ++offset) {
        uint16_t addr;
        uint8_t got;
        uint16_t next_offset;

        if (rtos_task_stop_requested() != 0u) {
            break;
        }

        addr = (uint16_t)(region->base + offset);
        if (vchk_test_byte(addr, &got) == 0u) {
            chunk_bytes_failed++;
            if (mismatch_reported == 0u) {
                mismatch_reported = 1u;
                vchk_emit_mismatch(region->name, addr, (uint8_t)VCHK_VALUE, got);
            }
        }

        next_offset = (uint16_t)(offset + 1u);
        if (((next_offset % (uint16_t)VCHK_PROGRESS_CHUNK) == 0u) || (next_offset == region->length)) {
            uint16_t chunk_end = (uint16_t)(region->base + next_offset - 1u);

            total_chunks++;
            if (chunk_bytes_failed != 0u) {
                failed_chunks++;
            }
            if (progress_enabled != 0u) {
                vchk_emit_progress(region->name, chunk_start, chunk_end, chunk_bytes_failed);
            }

            chunk_start = (uint16_t)(chunk_end + 1u);
            chunk_bytes_failed = 0u;
        }
    }

    vchk_emit_result(region->name, region->base, (uint16_t)(region->base + region->length - 1u),
                      total_chunks, failed_chunks);
}

void main_vchk(void)
{
    uint8_t i;

    for (i = 0u; i < (uint8_t)VCHK_REGION_COUNT; ++i) {
        if (rtos_task_stop_requested() != 0u) {
            break;
        }
        vchk_run_region(&g_vchk_regions[i]);
    }

    task_exit();
}
