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

/* zbus_write() rejects outright (returns 0, no partial send) anything
 * longer than one zlink frame's payload (ZBUS_FRAME_MAX_LEN=64) -- a line
 * built by sprint_t can easily exceed that once several fields are
 * concatenated (confirmed on real hardware: rchk's final result line, ~70
 * bytes, was silently dropped in full, leaving only its trailing CRLF).
 * Splitting here, transparently, means every caller (pipe_write_cstr and
 * everything built on it) can emit lines of any length without needing to
 * know about the wire's per-frame limit -- the receiving end just sees a
 * continuous byte stream either way, so chunking doesn't change what shows
 * up on screen, only how many zbus frames it takes to get there. */
static void pipe_write_bytes_retry(const uint8_t *data, uint8_t len)
{
    if ((data == (const uint8_t *)0) || (len == 0u)) {
        return;
    }

    while (len > 0u) {
        uint8_t chunk_len = (len > (uint8_t)ZBUS_FRAME_MAX_LEN) ? (uint8_t)ZBUS_FRAME_MAX_LEN : len;
        uint8_t retries = PIPE_WRITE_RETRY_MAX;
        uint8_t sent = 0u;

        while (retries > 0u) {
            if (zbus_write(data, chunk_len) == chunk_len) {
                sent = 1u;
                break;
            }
            retries--;
            CPU_NOP();
        }
        if (sent == 0u) {
            return;
        }

        data = data + chunk_len;
        len = (uint8_t)(len - chunk_len);
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
