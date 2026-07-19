#ifndef XSH_CMD_EMIT_H
#define XSH_CMD_EMIT_H

#include <stdint.h>

#include "xsh.h"

/* Shared "undefined task id" text: used by both control commands (weight)
 * and report commands (tasks/heap/stack/cpu), all in different .c files. */
extern const uint8_t g_xsh_cmd_txt_bad_task[];

/* Buffered-with-fallback line emitter (see xsh_cmd_emit.c for the design
 * rationale) and small per-task display helpers built on top of it. All
 * operate on a single shared static instance -- see xsh_cmd_emit.c. */
void xsh_cmd_emit_begin(xsh_t *sh);
void xsh_cmd_emit_cstr(const uint8_t *text);
void xsh_cmd_emit_u8_dec(uint8_t value);
void xsh_cmd_emit_u16_dec(uint16_t value);
void xsh_cmd_emit_hex16(uint16_t value);

/* Table-style column emitters: same buffer-with-fallback pattern as the
 * plain xsh_cmd_emit_* above, just left-justified and padded with spaces
 * to a fixed column width. */
void xsh_cmd_emit_cstr_padded(const uint8_t *text, uint8_t width);
void xsh_cmd_emit_u8_dec_padded(uint8_t value, uint8_t width);
void xsh_cmd_emit_u16_dec_padded(uint16_t value, uint8_t width);
void xsh_cmd_emit_tty_padded(uint8_t slot, uint8_t width);

void xsh_cmd_emit_tty(uint8_t slot);
void xsh_cmd_emit_name(const uint8_t *name, uint8_t name_len);
void xsh_cmd_emit_finish(void);
void xsh_cmd_emit_line(xsh_t *sh, const uint8_t *text);
void xsh_cmd_emit_usage(xsh_t *sh, const uint8_t *usage);

void xsh_cmd_write_task_tty(xsh_t *sh, uint8_t slot);
const uint8_t *xsh_cmd_task_state_name(uint8_t state);
uint8_t xsh_cmd_task_name_len(uint8_t slot);

#endif
