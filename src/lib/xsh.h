#ifndef XSH_H
#define XSH_H

#include <stdint.h>

#ifndef XSH_LINE_MAX
#define XSH_LINE_MAX 56u
#endif

#ifndef XSH_RX_BURST
#define XSH_RX_BURST 8u
#endif

#ifndef XSH_ARGV_MAX
#define XSH_ARGV_MAX 8u
#endif

#ifndef XSH_INVALID_TTY
#define XSH_INVALID_TTY 0xFFu
#endif

typedef struct xsh xsh_t;

typedef uint8_t (*xsh_cmd_handler_t)(xsh_t *sh, uint8_t argc, uint8_t *argv[]);

typedef struct xsh_cmd {
    const uint8_t *name;
    const uint8_t *usage;
    uint8_t min_args;
    uint8_t max_args;
    xsh_cmd_handler_t handler;
} xsh_cmd_t;

struct xsh {
    uint8_t tty_id;
    uint8_t line[XSH_LINE_MAX];
    uint8_t line_len;
    uint8_t last_was_cr;
    uint8_t started;
    uint8_t pending_prompt;
    const uint8_t *banner;
    const uint8_t *prompt;
    const uint8_t *unknown_cmd_text;
    const xsh_cmd_t *cmd_table;
    uint8_t cmd_count;
};

void xsh_init(xsh_t *sh, const xsh_cmd_t *cmd_table, uint8_t cmd_count);
void xsh_set_tty(xsh_t *sh, uint8_t tty_id);
void xsh_set_banner(xsh_t *sh, const uint8_t *banner);
void xsh_set_prompt(xsh_t *sh, const uint8_t *prompt);
void xsh_set_unknown_text(xsh_t *sh, const uint8_t *unknown_cmd_text);
void xsh_tick(xsh_t *sh);

void xsh_write_bytes(const xsh_t *sh, const uint8_t *data, uint8_t len);
void xsh_write_cstr(const xsh_t *sh, const uint8_t *text);
void xsh_write_u8_dec(const xsh_t *sh, uint8_t value);
void xsh_write_u16_dec(const xsh_t *sh, uint16_t value);
void xsh_write_u16_hex(const xsh_t *sh, uint16_t value);
void xsh_newline(const xsh_t *sh);
uint8_t xsh_parse_u8(const uint8_t *text, uint8_t *out_value);
uint8_t xsh_line_append_cstr(uint8_t *buf, uint8_t max, uint8_t *io_len, const uint8_t *text);
uint8_t xsh_line_append_u16_dec(uint8_t *buf, uint8_t max, uint8_t *io_len, uint16_t value);
uint8_t xsh_line_append_u8_dec(uint8_t *buf, uint8_t max, uint8_t *io_len, uint8_t value);

#endif
