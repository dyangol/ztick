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

static void pipe_write_bytes_retry(const uint8_t *data, uint8_t len)
{
    uint8_t retries = PIPE_WRITE_RETRY_MAX;

    if ((data == (const uint8_t *)0) || (len == 0u)) {
        return;
    }

    while (retries > 0u) {
        if (zbus_write(data, len) == len) {
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

    pipe_write_bytes_retry(text, len);
}

void pipe_newline(void)
{
    pipe_write_bytes_retry(g_pipe_crlf, (uint8_t)(sizeof(g_pipe_crlf) - 1u));
}

void pipe_flush(void)
{
    while (zbus_tty_tx_pending_current() != 0u) {
        CPU_HALT();
    }
}
