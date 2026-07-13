#include <stdint.h>

#include "../common/common.h"
#include "../drivers/zbus.h"
#include "xsh.h"

#pragma codeseg CODE

#define XSH_BACKSPACE 0x08u
#define XSH_DELETE 0x7Fu
#define XSH_CR 0x0Du
#define XSH_LF 0x0Au
#define XSH_WRITE_RETRY_MAX 64u
#define XSH_ARGC_OVERFLOW 0xFFu

static const uint8_t g_xsh_default_prompt[] = "xsh> ";
static const uint8_t g_xsh_default_unknown[] = "unknown command";
static const uint8_t g_xsh_too_many_args[] = "too many args";
static const uint8_t g_xsh_crlf[] = "\r\n";
static const uint8_t g_xsh_bs_seq[3] = {XSH_BACKSPACE, (uint8_t)' ', XSH_BACKSPACE};

static uint8_t xsh_strlen_u8(const uint8_t *text)
{
    uint8_t len = 0u;

    if (text == (const uint8_t *)0) {
        return 0u;
    }

    while (text[len] != 0u) {
        len++;
    }

    return len;
}

uint8_t xsh_line_append_cstr(uint8_t *buf, uint8_t max, uint8_t *io_len, const uint8_t *text)
{
    uint8_t idx;

    if ((buf == (uint8_t *)0) || (io_len == (uint8_t *)0) || (text == (const uint8_t *)0)) {
        return 0u;
    }

    idx = *io_len;
    while (text[0] != 0u) {
        if (idx >= max) {
            *io_len = idx;
            return 0u;
        }
        buf[idx] = text[0];
        idx++;
        text++;
    }

    *io_len = idx;
    return 1u;
}

/* Shared by the buffered (xsh_line_append_u16_dec) and direct-write
 * (xsh_write_u16_dec_inner) decimal paths: fills digits[] least-significant
 * first and returns how many were written, so both callers can emit them
 * in the same most-significant-first loop. */
static uint8_t xsh_u16_to_digits(uint16_t value, uint8_t digits[5])
{
    uint8_t count = 0u;

    if (value == 0u) {
        digits[0] = (uint8_t)'0';
        return 1u;
    }

    while (value > 0u) {
        digits[count] = (uint8_t)((value % 10u) + (uint8_t)'0');
        value = (uint16_t)(value / 10u);
        count++;
    }

    return count;
}

uint8_t xsh_line_append_u16_dec(uint8_t *buf, uint8_t max, uint8_t *io_len, uint16_t value)
{
    uint8_t digits[5];
    uint8_t count;
    uint8_t i;

    if ((buf == (uint8_t *)0) || (io_len == (uint8_t *)0)) {
        return 0u;
    }

    count = xsh_u16_to_digits(value, digits);

    for (i = count; i > 0u; --i) {
        if (*io_len >= max) {
            return 0u;
        }
        buf[*io_len] = digits[i - 1u];
        *io_len = (uint8_t)(*io_len + 1u);
    }

    return 1u;
}

uint8_t xsh_line_append_u8_dec(uint8_t *buf, uint8_t max, uint8_t *io_len, uint8_t value)
{
    return xsh_line_append_u16_dec(buf, max, io_len, (uint16_t)value);
}

static uint8_t xsh_try_write_bytes(const xsh_t *sh, const uint8_t *data, uint8_t len)
{
    uint8_t written;

    if ((sh == (const xsh_t *)0) || (data == (const uint8_t *)0) || (len == 0u)) {
        return 0u;
    }

    if (sh->tty_id == (uint8_t)XSH_INVALID_TTY) {
        return 0u;
    }

    written = zbus_write_tty(sh->tty_id, data, len);
    return (written == len) ? 1u : 0u;
}

static void xsh_reset_session(xsh_t *sh)
{
    sh->line_len = 0u;
    sh->last_was_cr = 0u;
    sh->started = 0u;
    sh->pending_prompt = 0u;
}

void xsh_init(xsh_t *sh, const xsh_cmd_t *cmd_table, uint8_t cmd_count)
{
    if (sh == (xsh_t *)0) {
        return;
    }

    sh->tty_id = (uint8_t)XSH_INVALID_TTY;
    sh->cmd_table = cmd_table;
    sh->cmd_count = cmd_count;
    sh->banner = (const uint8_t *)0;
    sh->prompt = g_xsh_default_prompt;
    sh->unknown_cmd_text = g_xsh_default_unknown;
    xsh_reset_session(sh);
}

void xsh_set_tty(xsh_t *sh, uint8_t tty_id)
{
    if (sh == (xsh_t *)0) {
        return;
    }

    if (sh->tty_id != tty_id) {
        sh->tty_id = tty_id;
        xsh_reset_session(sh);
    }
}

static uint8_t xsh_attach_current_tty(xsh_t *sh)
{
    uint8_t tty_id;

    if (sh == (xsh_t *)0) {
        return 0u;
    }

    if (zbus_tty_get_current(&tty_id) == 0u) {
        return 0u;
    }

    xsh_set_tty(sh, tty_id);
    return 1u;
}

void xsh_set_banner(xsh_t *sh, const uint8_t *banner)
{
    if (sh == (xsh_t *)0) {
        return;
    }

    sh->banner = banner;
}

void xsh_set_prompt(xsh_t *sh, const uint8_t *prompt)
{
    if (sh == (xsh_t *)0) {
        return;
    }

    sh->prompt = (prompt != (const uint8_t *)0) ? prompt : g_xsh_default_prompt;
}

void xsh_set_unknown_text(xsh_t *sh, const uint8_t *unknown_cmd_text)
{
    if (sh == (xsh_t *)0) {
        return;
    }

    sh->unknown_cmd_text = (unknown_cmd_text != (const uint8_t *)0) ? unknown_cmd_text : g_xsh_default_unknown;
}

void xsh_write_bytes(const xsh_t *sh, const uint8_t *data, uint8_t len)
{
    uint8_t retries = XSH_WRITE_RETRY_MAX;

    while (retries > 0u) {
        if (xsh_try_write_bytes(sh, data, len) != 0u) {
            return;
        }
        retries--;
        CPU_NOP();
    }
}

void xsh_write_cstr(const xsh_t *sh, const uint8_t *text)
{
    uint8_t len = xsh_strlen_u8(text);
    if (len == 0u) {
        return;
    }

    xsh_write_bytes(sh, text, len);
}

static void xsh_write_u16_dec_inner(const xsh_t *sh, uint16_t value)
{
    uint8_t digits[5];
    uint8_t count = xsh_u16_to_digits(value, digits);
    uint8_t i;

    for (i = count; i > 0u; --i) {
        xsh_write_bytes(sh, &digits[i - 1u], 1u);
    }
}

void xsh_write_u8_dec(const xsh_t *sh, uint8_t value)
{
    xsh_write_u16_dec_inner(sh, (uint16_t)value);
}

void xsh_write_u16_dec(const xsh_t *sh, uint16_t value)
{
    xsh_write_u16_dec_inner(sh, value);
}

void xsh_write_u16_hex(const xsh_t *sh, uint16_t value)
{
    uint8_t i;

    for (i = 0u; i < 4u; ++i) {
        uint8_t nibble = (uint8_t)((value >> (uint8_t)((3u - i) * 4u)) & 0x0Fu);
        uint8_t digit = (nibble < 10u) ? (uint8_t)('0' + nibble) : (uint8_t)('A' + (nibble - 10u));
        xsh_write_bytes(sh, &digit, 1u);
    }
}

void xsh_newline(const xsh_t *sh)
{
    xsh_write_bytes(sh, g_xsh_crlf, 2u);
}

static uint8_t xsh_try_prompt(xsh_t *sh)
{
    uint8_t len;

    if (sh == (xsh_t *)0) {
        return 0u;
    }

    len = xsh_strlen_u8(sh->prompt);
    if (len == 0u) {
        return 1u;
    }

    return xsh_try_write_bytes(sh, sh->prompt, len);
}

uint8_t xsh_parse_u8(const uint8_t *text, uint8_t *out_value)
{
    uint16_t value = 0u;
    uint8_t i = 0u;

    if ((text == (const uint8_t *)0) || (out_value == (uint8_t *)0) || (text[0] == 0u)) {
        return 0u;
    }

    while (text[i] != 0u) {
        uint8_t ch = text[i];
        if ((ch < (uint8_t)'0') || (ch > (uint8_t)'9')) {
            return 0u;
        }
        value = (uint16_t)(value * 10u + (uint16_t)(ch - (uint8_t)'0'));
        if (value > 255u) {
            return 0u;
        }
        i++;
    }

    *out_value = (uint8_t)value;
    return 1u;
}

static uint8_t xsh_token_equals(const uint8_t *token, const uint8_t *lit)
{
    uint8_t i = 0u;

    if ((token == (const uint8_t *)0) || (lit == (const uint8_t *)0)) {
        return 0u;
    }

    while (lit[i] != 0u) {
        if (token[i] != lit[i]) {
            return 0u;
        }
        i++;
    }

    return (token[i] == 0u) ? 1u : 0u;
}

static uint8_t xsh_split_argv(uint8_t *line, uint8_t line_len, uint8_t *argv[XSH_ARGV_MAX])
{
    uint8_t argc = 0u;
    uint8_t i = 0u;

    while (i < line_len) {
        while ((i < line_len) && (line[i] == (uint8_t)' ')) {
            i++;
        }

        if (i >= line_len) {
            break;
        }

        if (argc >= XSH_ARGV_MAX) {
            return (uint8_t)XSH_ARGC_OVERFLOW;
        }

        argv[argc] = &line[i];
        argc++;

        while ((i < line_len) && (line[i] != (uint8_t)' ')) {
            i++;
        }

        if (i < line_len) {
            line[i] = 0u;
            i++;
        }
    }

    return argc;
}

static const xsh_cmd_t *xsh_find_cmd(const xsh_t *sh, const uint8_t *name)
{
    uint8_t i;

    if ((sh == (const xsh_t *)0) || (name == (const uint8_t *)0) || (sh->cmd_table == (const xsh_cmd_t *)0)) {
        return (const xsh_cmd_t *)0;
    }

    for (i = 0u; i < sh->cmd_count; ++i) {
        const xsh_cmd_t *cmd = &sh->cmd_table[i];
        if ((cmd->name != (const uint8_t *)0) && (xsh_token_equals(name, cmd->name) != 0u)) {
            return cmd;
        }
    }

    return (const xsh_cmd_t *)0;
}

static void xsh_execute_line(xsh_t *sh)
{
    uint8_t *argv[XSH_ARGV_MAX];
    uint8_t argc;
    const xsh_cmd_t *cmd;
    uint8_t cmd_argc;

    if ((sh == (xsh_t *)0) || (sh->line_len == 0u)) {
        return;
    }

    sh->line[sh->line_len] = 0u;
    argc = xsh_split_argv(sh->line, sh->line_len, argv);
    if (argc == 0u) {
        return;
    }
    if (argc == (uint8_t)XSH_ARGC_OVERFLOW) {
        xsh_write_cstr(sh, g_xsh_too_many_args);
        xsh_newline(sh);
        return;
    }

    cmd = xsh_find_cmd(sh, argv[0]);
    if (cmd == (const xsh_cmd_t *)0) {
        xsh_write_cstr(sh, sh->unknown_cmd_text);
        xsh_newline(sh);
        return;
    }

    cmd_argc = (uint8_t)(argc - 1u);
    if ((cmd_argc < cmd->min_args) || (cmd_argc > cmd->max_args)) {
        if (cmd->usage != (const uint8_t *)0) {
            xsh_write_cstr(sh, cmd->usage);
            xsh_newline(sh);
        }
        return;
    }

    if (cmd->handler != (xsh_cmd_handler_t)0) {
        (void)cmd->handler(sh, argc, argv);
    }
}

static void xsh_on_char(xsh_t *sh, uint8_t ch)
{
    if (sh == (xsh_t *)0) {
        return;
    }

    if (ch == (uint8_t)XSH_LF) {
        if (sh->last_was_cr != 0u) {
            sh->last_was_cr = 0u;
            return;
        }
    } else {
        sh->last_was_cr = (ch == (uint8_t)XSH_CR) ? 1u : 0u;
    }

    if ((ch == (uint8_t)XSH_CR) || (ch == (uint8_t)XSH_LF)) {
        xsh_newline(sh);
        xsh_execute_line(sh);
        sh->line_len = 0u;
        if (xsh_try_prompt(sh) == 0u) {
            sh->pending_prompt = 1u;
        } else {
            sh->pending_prompt = 0u;
        }
        return;
    }

    if ((ch == (uint8_t)XSH_BACKSPACE) || (ch == (uint8_t)XSH_DELETE)) {
        if (sh->line_len > 0u) {
            sh->line_len--;
            xsh_write_bytes(sh, g_xsh_bs_seq, 3u);
        }
        return;
    }

    if ((ch >= (uint8_t)32u) && (ch <= (uint8_t)126u)) {
        if (sh->line_len < (uint8_t)(XSH_LINE_MAX - 1u)) {
            sh->line[sh->line_len] = ch;
            sh->line_len++;
            xsh_write_bytes(sh, &ch, 1u);
        }
    }
}

void xsh_tick(xsh_t *sh)
{
    uint8_t rx[XSH_RX_BURST];
    uint8_t len;
    uint8_t i;

    if (sh == (xsh_t *)0) {
        return;
    }

    if (sh->tty_id == (uint8_t)XSH_INVALID_TTY) {
        (void)xsh_attach_current_tty(sh);
        if (sh->tty_id == (uint8_t)XSH_INVALID_TTY) {
            return;
        }
    }

    if (sh->started == 0u) {
        if (sh->banner != (const uint8_t *)0) {
            xsh_write_cstr(sh, sh->banner);
        }
        if (xsh_try_prompt(sh) == 0u) {
            sh->pending_prompt = 1u;
        } else {
            sh->pending_prompt = 0u;
        }
        sh->started = 1u;
    }

    len = zbus_read_tty(sh->tty_id, rx, (uint8_t)sizeof(rx));
    for (i = 0u; i < len; ++i) {
        xsh_on_char(sh, rx[i]);
    }

    if (sh->pending_prompt != 0u) {
        if (xsh_try_prompt(sh) != 0u) {
            sh->pending_prompt = 0u;
        }
    }
}
