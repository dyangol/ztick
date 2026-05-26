#include <stdint.h>

#include "../common/common.h"
#include "../drivers/zbus.h"
#include "pipe.h"

#pragma codeseg CODE

#define PIPE_WRITE_RETRY_MAX 64u

static const uint8_t g_pipe_crlf[] = "\r\n";

static uint8_t pipe_strlen_u8(const uint8_t *text)
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

static uint8_t pipe_try_write_bytes(const uint8_t *data, uint8_t len)
{
    uint8_t written;

    if ((data == (const uint8_t *)0) || (len == 0u)) {
        return 0u;
    }

    written = zbus_write(data, len);
    return (written == len) ? 1u : 0u;
}

void pipe_write_bytes(const uint8_t *data, uint8_t len)
{
    uint8_t retries = PIPE_WRITE_RETRY_MAX;

    while (retries > 0u) {
        if (pipe_try_write_bytes(data, len) != 0u) {
            return;
        }
        retries--;
        CPU_NOP();
    }
}

void pipe_write_cstr(const uint8_t *text)
{
    uint8_t len = pipe_strlen_u8(text);

    if (len == 0u) {
        return;
    }

    pipe_write_bytes(text, len);
}

static void pipe_write_u16_dec_inner(uint16_t value)
{
    uint8_t digits[5];
    uint8_t count = 0u;
    uint8_t i;

    if (value == 0u) {
        pipe_write_bytes((const uint8_t *)"0", 1u);
        return;
    }

    while (value > 0u) {
        digits[count] = (uint8_t)((value % 10u) + (uint8_t)'0');
        value = (uint16_t)(value / 10u);
        count++;
    }

    for (i = count; i > 0u; --i) {
        pipe_write_bytes(&digits[i - 1u], 1u);
    }
}

void pipe_write_u8_dec(uint8_t value)
{
    pipe_write_u16_dec_inner((uint16_t)value);
}

void pipe_write_u16_dec(uint16_t value)
{
    pipe_write_u16_dec_inner(value);
}

void pipe_write_u16_hex(uint16_t value)
{
    uint8_t buf[4];
    uint8_t i;

    for (i = 0u; i < 4u; ++i) {
        uint8_t nibble = (uint8_t)((value >> (uint8_t)((3u - i) * 4u)) & 0x0Fu);
        buf[i] = (nibble < 10u) ? (uint8_t)('0' + nibble) : (uint8_t)('A' + (nibble - 10u));
    }

    pipe_write_bytes((const uint8_t *)"0x", 2u);
    pipe_write_bytes(buf, 4u);
}

void pipe_newline(void)
{
    pipe_write_bytes(g_pipe_crlf, (uint8_t)(sizeof(g_pipe_crlf) - 1u));
}

void pipe_flush(void)
{
    while (zbus_tty_tx_pending_current() != 0u) {
        CPU_HALT();
    }
}
