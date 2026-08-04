#include <stdint.h>

#include "common.h"
#include "../bootstrap/rtos.h"
#include "../drivers/vdp.h"
#include "../lib/pipe.h"
#include "../lib/sprint.h"
#include "../lib/task.h"
#include "args_gchk.h"

#pragma codeseg CODE

#define GCHK_DEFAULT_SECONDS 3u
#define GCHK_TICKS_PER_SECOND 50u
#define GCHK_PATTERN_COUNT 5u
#define GCHK_GRID_COLS 32u
#define GCHK_GRID_ROWS 24u
#define GCHK_SPRITE_X_MAX ((uint8_t)(255u - 8u))
#define GCHK_SPRITE_STEP_TICKS 2u
/* R7 high nibble = text color, low nibble = backdrop/border color (see
 * VDP_SCREEN1_R7=0xF4 in vdp.c: text=15, backdrop=4). The border pattern
 * only varies the low nibble, colors 1-15 (0 is reserved/transparent). */
#define GCHK_R7_TEXT_COLOR 0xF0u
#define GCHK_BORDER_COLOR_COUNT 15u

static const uint8_t g_task_name_gchk[] = "gchk";
static const uint8_t g_gchk_start_args_usage[] = "[seconds]";
static const uint8_t g_gchk_name_colorbars[] = "colorbars";
static const uint8_t g_gchk_name_checkerboard[] = "checkerboard";
static const uint8_t g_gchk_name_corners[] = "corners";
static const uint8_t g_gchk_name_borders[] = "borders";
static const uint8_t g_gchk_name_sprite[] = "sprite";
static const uint8_t g_gchk_name_done[] = "DONE";

void main_gchk(void);

/* Task spec: manual-start visual smoke test, human-judged (see the file
 * comment below) -- not autostarted, same convention as rchk/vchk. */
const task_spec_t g_task_spec_gchk = {
    g_task_name_gchk,
    main_gchk,
    TASK_WEIGHT_MIN,
    (uint8_t)TASK_TTY_AUTO,
    g_gchk_start_args_usage,
    task_gchk_start_configure,
    task_gchk_start_reset
};

/* Unlike rchk/vchk, gchk can't verify its own result -- there's no way to
 * read back what's actually on screen. It cycles a fixed sequence of known
 * patterns, describing each over its tty, and leaves the pass/fail call to
 * whoever is looking at the screen. */

static void gchk_emit_pattern(uint8_t index, const uint8_t *name)
{
    uint8_t line[40];
    sprint_t out;

    sprint_begin(&out, line, (uint8_t)sizeof(line));
    (void)sprint_cstr(&out, (const uint8_t *)"gchk PATTERN ");
    (void)sprint_u8_dec(&out, index);
    (void)sprint_cstr(&out, (const uint8_t *)"/");
    (void)sprint_u8_dec(&out, (uint8_t)GCHK_PATTERN_COUNT);
    (void)sprint_cstr(&out, (const uint8_t *)" ");
    (void)sprint_cstr(&out, name);

    if (sprint_ok(&out) != 0u) {
        sprint_emit_line(&out);
    } else {
        pipe_write_cstr((const uint8_t *)"gchk ERR msg-overflow");
        pipe_newline();
    }
    pipe_flush();
}

static void gchk_emit_done(void)
{
    uint8_t line[16];
    sprint_t out;

    sprint_begin(&out, line, (uint8_t)sizeof(line));
    (void)sprint_cstr(&out, (const uint8_t *)"gchk ");
    (void)sprint_cstr(&out, g_gchk_name_done);

    if (sprint_ok(&out) != 0u) {
        sprint_emit_line(&out);
    }
    pipe_flush();
}

/* Puts the VDP back in the exact state boot.c leaves it in (registers,
 * pattern/name/color/sprite tables) -- run once before the first pattern so
 * gchk always starts from a known baseline regardless of what was on screen
 * before, and again on every exit path so it never leaves a test pattern
 * stuck on screen after it's done. DI/EI-wrapped because vdp_init_screen1()
 * is a long sequence of VDP port writes relying on the VDP's own address
 * auto-increment; a task switch into activity_indicator mid-sequence would
 * interleave its VDP writes and desync that pointer (same hazard documented
 * for vchk_test_byte() in src/vchk/main_vchk.c). */
static void gchk_reset_screen(void)
{
    CPU_DI();
    vdp_init_screen1();
    CPU_EI();
}

/* Returns 0 if a stop request cut the wait short (caller should bail out
 * without starting the next pattern), 1 if it ran the full duration. */
static uint8_t gchk_wait_ticks(uint16_t ticks)
{
    uint16_t start = g_tick_count;

    while ((uint16_t)(g_tick_count - start) < ticks) {
        if (rtos_task_stop_requested() != 0u) {
            return 0u;
        }
    }
    return 1u;
}

/* Pattern 1: 8 solid vertical bands, one per color 1-15 (skipping every
 * other one), each its own character code so it gets its own color-table
 * entry -- confirms all colors render distinctly and in the right place.
 * DI/EI-wrapped for the same reason as gchk_reset_screen(): vdp_define_char()
 * and the vdp_putc() sweep below are all multi-write VDP sequences relying
 * on address state (auto-increment or a just-set write address) staying
 * intact across writes; a task switch into activity_indicator partway
 * through would interleave its own VDP writes and corrupt the rest of the
 * pattern (confirmed on real hardware: showed up as "jailbars" instead of a
 * clean fill). */
static void gchk_pattern_colorbars(void)
{
    static const uint8_t solid_glyph[8] = {
        0xFFu, 0xFFu, 0xFFu, 0xFFu, 0xFFu, 0xFFu, 0xFFu, 0xFFu
    };
    uint8_t band;
    uint8_t band_width = (uint8_t)((uint8_t)GCHK_GRID_COLS / 8u);

    CPU_DI();
    for (band = 0u; band < 8u; ++band) {
        uint8_t code = (uint8_t)(band * 8u);
        uint8_t color = (uint8_t)(band + 1u);
        uint8_t packed_color = (uint8_t)((color << 4) | color);
        uint8_t col_start = (uint8_t)(band * band_width);
        uint8_t col_end = (uint8_t)(col_start + band_width);
        uint8_t col;
        uint8_t row;

        vdp_define_char(code, solid_glyph, packed_color);

        for (col = col_start; col < col_end; ++col) {
            for (row = 0u; row < (uint8_t)GCHK_GRID_ROWS; ++row) {
                vdp_putc(col, row, code);
            }
        }
    }
    CPU_EI();
}

/* Pattern 2: alternating blank/solid glyphs across the whole 32x24 grid --
 * reveals dead cells, alignment problems or artifacts anywhere on screen. */
static void gchk_pattern_checkerboard(void)
{
    static const uint8_t blank_glyph[8] = {0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u};
    static const uint8_t solid_glyph[8] = {
        0xFFu, 0xFFu, 0xFFu, 0xFFu, 0xFFu, 0xFFu, 0xFFu, 0xFFu
    };
    uint8_t x;
    uint8_t y;

    CPU_DI();
    vdp_define_char(0u, blank_glyph, 0xF1u);
    vdp_define_char(1u, solid_glyph, 0xF1u);

    for (y = 0u; y < (uint8_t)GCHK_GRID_ROWS; ++y) {
        for (x = 0u; x < (uint8_t)GCHK_GRID_COLS; ++x) {
            vdp_putc(x, y, (uint8_t)(((x + y) & 1u) ? 1u : 0u));
        }
    }
    CPU_EI();
}

/* Pattern 3: a blank screen with a marker at each of the 4 corners --
 * confirms the visible area really is the expected 32x24 grid (catches
 * overscan/addressing offsets a full-grid pattern can hide). Reuses codes
 * 0/1 already defined by gchk_pattern_checkerboard(). */
static void gchk_pattern_corners(void)
{
    uint8_t x;
    uint8_t y;

    CPU_DI();
    for (y = 0u; y < (uint8_t)GCHK_GRID_ROWS; ++y) {
        for (x = 0u; x < (uint8_t)GCHK_GRID_COLS; ++x) {
            vdp_putc(x, y, 0u);
        }
    }

    vdp_putc(0u, 0u, 1u);
    vdp_putc((uint8_t)(GCHK_GRID_COLS - 1u), 0u, 1u);
    vdp_putc(0u, (uint8_t)(GCHK_GRID_ROWS - 1u), 1u);
    vdp_putc((uint8_t)(GCHK_GRID_COLS - 1u), (uint8_t)(GCHK_GRID_ROWS - 1u), 1u);
    CPU_EI();
}

/* Pattern 4: cycles the backdrop/border color (R7 low nibble) through all 15
 * non-zero values, one at a time, over the pattern's whole duration -- the
 * interior grid is cleared to blank and the sprite hidden so the border is
 * the only thing changing on screen. Self-contained like the sprite pattern
 * below: it owns the whole total_ticks budget instead of the caller waiting
 * on it afterwards. */
static void gchk_pattern_borders(uint16_t total_ticks)
{
    uint8_t hide_attr[4] = { 0xD0u, 0u, 0u, 0u };
    uint16_t ticks_per_color = (uint16_t)(total_ticks / (uint16_t)GCHK_BORDER_COLOR_COUNT);
    uint8_t color;

    /* DI/EI only around the two bulk VRAM writes -- see gchk_pattern_colorbars()
     * for why. Left out of the per-color loop below on purpose: it calls
     * gchk_wait_ticks(), which needs the tick interrupt running. */
    CPU_DI();
    vdp_fill_vram(VDP_NAME_TABLE_ADDR, 0x0300u, 0u);
    vdp_write_block(VDP_SPRITE_ATTR_ADDR, hide_attr, 4u);
    CPU_EI();

    for (color = 1u; color <= (uint8_t)GCHK_BORDER_COLOR_COUNT; ++color) {
        vdp_write_register((uint8_t)(GCHK_R7_TEXT_COLOR | color), 7u);
        if (gchk_wait_ticks(ticks_per_color) == 0u) {
            return;
        }
    }
}

/* Pattern 5: one 8x8 sprite (MAG=0/SIZE=0, per VDP_SCREEN1_R1 -- so the
 * sprite generator NAME byte is a plain 8-byte-block index, no quadrant
 * mapping) sliding left-to-right, exercising the sprite plane itself --
 * something vchk's VRAM-only check can never touch. The rest of the sprite
 * attribute table is still the 0xD0 (208) terminator vdp_init_screen1()
 * already filled it with, so only entry 0 needs writing. */
static void gchk_pattern_sprite(uint16_t total_ticks)
{
    static const uint8_t sprite_pattern[8] = {
        0x3Cu, 0x7Eu, 0xFFu, 0xFFu, 0xFFu, 0xFFu, 0x7Eu, 0x3Cu
    };
    uint8_t attr[4];
    uint8_t x = 0u;
    uint16_t start = g_tick_count;
    uint16_t last_move = start;

    /* DI/EI-wrapped for the same reason as gchk_pattern_colorbars() -- see
     * that function's comment. */
    CPU_DI();
    vdp_write_block(VDP_SPRITE_PATTERN_ADDR, sprite_pattern, 8u);

    attr[0] = 100u; /* Y */
    attr[1] = x;    /* X */
    attr[2] = 0u;   /* NAME: sprite pattern block 0 */
    attr[3] = 15u;  /* color=white, EC=0 */
    vdp_write_block(VDP_SPRITE_ATTR_ADDR, attr, 4u);
    CPU_EI();

    while ((uint16_t)(g_tick_count - start) < total_ticks) {
        if (rtos_task_stop_requested() != 0u) {
            return;
        }
        if ((uint16_t)(g_tick_count - last_move) >= (uint16_t)GCHK_SPRITE_STEP_TICKS) {
            last_move = g_tick_count;
            x = (x < (uint8_t)GCHK_SPRITE_X_MAX) ? (uint8_t)(x + 1u) : 0u;
            CPU_DI();
            vdp_write_vram((uint16_t)(VDP_SPRITE_ATTR_ADDR + 1u), x);
            CPU_EI();
        }
    }
}

/* Common exit path for every branch below: leave the VDP back in its known
 * boot-time state (see gchk_reset_screen()) before handing control back to
 * the scheduler, whether gchk ran to completion or was cancelled mid-pattern. */
static void gchk_finish(void)
{
    gchk_reset_screen();
    task_exit();
}

void main_gchk(void)
{
    uint8_t seconds = task_gchk_seconds_resolve((uint8_t)GCHK_DEFAULT_SECONDS);
    uint16_t pattern_ticks = (uint16_t)((uint16_t)seconds * (uint16_t)GCHK_TICKS_PER_SECOND);

    gchk_reset_screen();

    gchk_emit_pattern(1u, g_gchk_name_colorbars);
    gchk_pattern_colorbars();
    if (gchk_wait_ticks(pattern_ticks) == 0u) {
        gchk_finish();
    }

    gchk_emit_pattern(2u, g_gchk_name_checkerboard);
    gchk_pattern_checkerboard();
    if (gchk_wait_ticks(pattern_ticks) == 0u) {
        gchk_finish();
    }

    gchk_emit_pattern(3u, g_gchk_name_corners);
    gchk_pattern_corners();
    if (gchk_wait_ticks(pattern_ticks) == 0u) {
        gchk_finish();
    }

    gchk_emit_pattern(4u, g_gchk_name_borders);
    gchk_pattern_borders(pattern_ticks);
    if (rtos_task_stop_requested() != 0u) {
        gchk_finish();
    }

    gchk_emit_pattern(5u, g_gchk_name_sprite);
    gchk_pattern_sprite(pattern_ticks);

    gchk_emit_done();
    gchk_finish();
}
