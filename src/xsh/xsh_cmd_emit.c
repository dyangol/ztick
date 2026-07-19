#include <stdint.h>

#include "../bootstrap/rtos.h"
#include "../drivers/zbus.h"
#include "../lib/sprint.h"
#include "xsh.h"
#include "xsh_cmd_emit.h"

#pragma codeseg CODE

#define XSH_CMD_LINE_CAP ((uint8_t)(ZBUS_FRAME_MAX_LEN - 2u))

static const uint8_t g_xsh_cmd_state_unused[] = "unused";
static const uint8_t g_xsh_cmd_state_ready[] = "ready";
static const uint8_t g_xsh_cmd_state_wait_sem[] = "wait_sem";
static const uint8_t g_xsh_cmd_state_wait_q_send[] = "wait_q_send";
static const uint8_t g_xsh_cmd_state_wait_q_recv[] = "wait_q_recv";

const uint8_t g_xsh_cmd_txt_bad_task[] = "undefined task id";

void xsh_cmd_write_task_tty(xsh_t *sh, uint8_t slot)
{
    uint8_t tty_id;

    xsh_write_cstr(sh, (const uint8_t *)" tty=");
    if (zbus_tty_get_for_task(slot, &tty_id) != 0u) {
        xsh_write_u8_dec(sh, tty_id);
    } else {
        xsh_write_cstr(sh, (const uint8_t *)"-");
    }
}

const uint8_t *xsh_cmd_task_state_name(uint8_t state)
{
    if (state == TASK_READY) {
        return g_xsh_cmd_state_ready;
    }
    if (state == TASK_UNUSED) {
        return g_xsh_cmd_state_unused;
    }
    if (state == TASK_WAIT_SEM) {
        return g_xsh_cmd_state_wait_sem;
    }
    if (state == TASK_WAIT_Q_SEND) {
        return g_xsh_cmd_state_wait_q_send;
    }
    if (state == TASK_WAIT_Q_RECV) {
        return g_xsh_cmd_state_wait_q_recv;
    }
    return (const uint8_t *)"other";
}

uint8_t xsh_cmd_task_name_len(uint8_t slot)
{
    uint8_t name_len = g_tasks[slot].name_len;
    if (name_len > TASK_NAME_MAX) {
        name_len = TASK_NAME_MAX;
    }
    return name_len;
}

static void xsh_cmd_emit_buffer_line(xsh_t *sh, uint8_t *line, uint8_t len)
{
    line[len] = (uint8_t)'\r';
    line[(uint8_t)(len + 1u)] = (uint8_t)'\n';
    xsh_write_bytes(sh, line, (uint8_t)(len + 2u));
}

/* Builds a line into a fixed buffer to emit as one atomic write, falling
 * back to writing fields directly (still correct, just not atomic) if the
 * formatted line doesn't fit XSH_CMD_LINE_CAP. Once a field fails to fit,
 * the emitter flushes whatever DID fit so far, then streams every
 * remaining field (including the one that just failed) directly --
 * producing exactly the same bytes on the wire as if buffering had never
 * been attempted at all. */
typedef struct xsh_cmd_emitter {
    xsh_t *sh;
    sprint_t sp;
    /* +1: sprint_t reserves one byte of `cap` for its own NUL terminator,
     * which xsh_cmd_emit_buffer_line doesn't use (it appends "\r\n" using
     * sp.len directly) -- sized so the usable content length is still
     * exactly XSH_CMD_LINE_CAP, matching the pre-sprint_t behavior. */
    uint8_t buf[(uint8_t)(ZBUS_FRAME_MAX_LEN + 1u)];
    uint8_t buffered;
} xsh_cmd_emitter_t;

/* Single shared instance instead of a per-call local: xsh runs one
 * command at a time (no reentrancy, no concurrent tasks touching this
 * struct), so exactly one "in-progress buffered line" can ever exist.
 * Keeping this ~64-byte struct here instead of as a local variable makes
 * it structurally impossible for two emitter-owning call frames to be
 * alive on the stack at once -- which is exactly what overflowed xsh's
 * 320-byte task stack (confirmed via `stack` showing peak=320,
 * free_peak=0) when cmd_tasks's unfiltered branch used to keep its own
 * buffer alive while calling into a second function that needed another.
 * If xsh ever becomes reentrant/concurrent, this assumption breaks and
 * every xsh_cmd_emit_* function below needs an instance passed in again. */
static xsh_cmd_emitter_t g_xsh_cmd_emitter;

void xsh_cmd_emit_begin(xsh_t *sh)
{
    g_xsh_cmd_emitter.sh = sh;
    sprint_begin(&g_xsh_cmd_emitter.sp, g_xsh_cmd_emitter.buf, (uint8_t)(XSH_CMD_LINE_CAP + 1u));
    g_xsh_cmd_emitter.buffered = 1u;
}

static void xsh_cmd_emit_fallback(uint8_t good_len)
{
    g_xsh_cmd_emitter.sp.len = good_len;
    g_xsh_cmd_emitter.buffered = 0u;
    xsh_write_bytes(g_xsh_cmd_emitter.sh, g_xsh_cmd_emitter.buf, g_xsh_cmd_emitter.sp.len);
}

void xsh_cmd_emit_cstr(const uint8_t *text)
{
    if (g_xsh_cmd_emitter.buffered != 0u) {
        uint8_t good_len = g_xsh_cmd_emitter.sp.len;
        if (sprint_cstr(&g_xsh_cmd_emitter.sp, text) != 0u) {
            return;
        }
        xsh_cmd_emit_fallback(good_len);
    }
    xsh_write_cstr(g_xsh_cmd_emitter.sh, text);
}

void xsh_cmd_emit_u8_dec(uint8_t value)
{
    if (g_xsh_cmd_emitter.buffered != 0u) {
        uint8_t good_len = g_xsh_cmd_emitter.sp.len;
        if (sprint_u8_dec(&g_xsh_cmd_emitter.sp, value) != 0u) {
            return;
        }
        xsh_cmd_emit_fallback(good_len);
    }
    xsh_write_u8_dec(g_xsh_cmd_emitter.sh, value);
}

void xsh_cmd_emit_u16_dec(uint16_t value)
{
    if (g_xsh_cmd_emitter.buffered != 0u) {
        uint8_t good_len = g_xsh_cmd_emitter.sp.len;
        if (sprint_u16_dec(&g_xsh_cmd_emitter.sp, value) != 0u) {
            return;
        }
        xsh_cmd_emit_fallback(good_len);
    }
    xsh_write_u16_dec(g_xsh_cmd_emitter.sh, value);
}

void xsh_cmd_emit_hex16(uint16_t value)
{
    if (g_xsh_cmd_emitter.buffered != 0u) {
        uint8_t good_len = g_xsh_cmd_emitter.sp.len;
        if (sprint_hex16(&g_xsh_cmd_emitter.sp, value) != 0u) {
            return;
        }
        xsh_cmd_emit_fallback(good_len);
    }
    xsh_write_u16_hex(g_xsh_cmd_emitter.sh, value);
}

/* Used only by the multi-row ("all slots") listings -- single-slot
 * queries keep the plain key=value format, which is already self-
 * describing and doesn't benefit from a header row. */
void xsh_cmd_emit_cstr_padded(const uint8_t *text, uint8_t width)
{
    if (g_xsh_cmd_emitter.buffered != 0u) {
        uint8_t good_len = g_xsh_cmd_emitter.sp.len;
        if (sprint_cstr_padded(&g_xsh_cmd_emitter.sp, text, width) != 0u) {
            return;
        }
        xsh_cmd_emit_fallback(good_len);
    }
    xsh_write_cstr(g_xsh_cmd_emitter.sh, text);
}

void xsh_cmd_emit_u8_dec_padded(uint8_t value, uint8_t width)
{
    if (g_xsh_cmd_emitter.buffered != 0u) {
        uint8_t good_len = g_xsh_cmd_emitter.sp.len;
        if (sprint_u8_dec_padded(&g_xsh_cmd_emitter.sp, value, width) != 0u) {
            return;
        }
        xsh_cmd_emit_fallback(good_len);
    }
    xsh_write_u8_dec(g_xsh_cmd_emitter.sh, value);
}

void xsh_cmd_emit_u16_dec_padded(uint16_t value, uint8_t width)
{
    if (g_xsh_cmd_emitter.buffered != 0u) {
        uint8_t good_len = g_xsh_cmd_emitter.sp.len;
        if (sprint_u16_dec_padded(&g_xsh_cmd_emitter.sp, value, width) != 0u) {
            return;
        }
        xsh_cmd_emit_fallback(good_len);
    }
    xsh_write_u16_dec(g_xsh_cmd_emitter.sh, value);
}

void xsh_cmd_emit_tty_padded(uint8_t slot, uint8_t width)
{
    uint8_t tty_id;

    if (zbus_tty_get_for_task(slot, &tty_id) != 0u) {
        xsh_cmd_emit_u8_dec_padded(tty_id, width);
    } else {
        xsh_cmd_emit_cstr_padded((const uint8_t *)"-", width);
    }
}

/* tty= field for a task: unified so every emitter-based line always
 * includes it (an earlier version of xsh_cmd_emit_task_name_line's
 * buffered path silently omitted it while its fallback path included it,
 * so the "tasks" summary could gain/lose a field depending on whether the
 * line happened to fit the buffer). */
void xsh_cmd_emit_tty(uint8_t slot)
{
    uint8_t tty_id;

    xsh_cmd_emit_cstr((const uint8_t *)" tty=");
    if (zbus_tty_get_for_task(slot, &tty_id) != 0u) {
        xsh_cmd_emit_u8_dec(tty_id);
    } else {
        xsh_cmd_emit_cstr((const uint8_t *)"-");
    }
}

void xsh_cmd_emit_name(const uint8_t *name, uint8_t name_len)
{
    uint8_t i;

    if (name_len == 0u) {
        xsh_cmd_emit_cstr((const uint8_t *)"-");
        return;
    }

    if (g_xsh_cmd_emitter.buffered != 0u) {
        uint8_t good_len = g_xsh_cmd_emitter.sp.len;
        uint8_t fits = 1u;

        for (i = 0u; i < name_len; ++i) {
            if (sprint_char(&g_xsh_cmd_emitter.sp, name[i]) == 0u) {
                fits = 0u;
                break;
            }
        }
        if (fits != 0u) {
            return;
        }
        xsh_cmd_emit_fallback(good_len);
    }

    for (i = 0u; i < name_len; ++i) {
        xsh_write_bytes(g_xsh_cmd_emitter.sh, &name[i], 1u);
    }
}

void xsh_cmd_emit_finish(void)
{
    if (g_xsh_cmd_emitter.buffered != 0u) {
        xsh_cmd_emit_buffer_line(g_xsh_cmd_emitter.sh, g_xsh_cmd_emitter.buf, g_xsh_cmd_emitter.sp.len);
    } else {
        xsh_newline(g_xsh_cmd_emitter.sh);
    }
}

void xsh_cmd_emit_line(xsh_t *sh, const uint8_t *text)
{
    xsh_cmd_emit_begin(sh);
    xsh_cmd_emit_cstr(text);
    xsh_cmd_emit_finish();
}

void xsh_cmd_emit_usage(xsh_t *sh, const uint8_t *usage)
{
    xsh_write_cstr(sh, usage);
    xsh_newline(sh);
}
