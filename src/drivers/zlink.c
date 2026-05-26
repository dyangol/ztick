#include <stdint.h>

#include "common.h"
#include "io.h"
#include "zlink.h"
#include "zlink_diag.h"

#pragma codeseg CODE

#define ZLINK_SOF5 0x15u
#define ZLINK_MAX_TTY 16u
#define ZLINK_TYPE_MASK 0x07u

typedef struct zlink_frame {
    uint8_t type;
    uint8_t tty;
    uint8_t seq;
    uint8_t len;
    uint8_t payload[ZLINK_MAX_PAYLOAD];
} zlink_frame_t;

static volatile uint8_t g_zlink_io_port;
static volatile uint8_t g_zlink_next_poll_seq;
static volatile uint8_t g_zlink_next_tx_seq[ZLINK_MAX_TTY];
static volatile uint8_t g_zlink_last_seq[ZLINK_MAX_TTY];
static volatile uint8_t g_zlink_last_seq_valid[ZLINK_MAX_TTY];

static volatile uint16_t g_zlink_rx_frames_ok;
static volatile uint16_t g_zlink_rx_crc_err;
static volatile uint16_t g_zlink_rx_dup;
static volatile uint16_t g_zlink_rx_type_err;
static volatile uint16_t g_zlink_rx_len_err;

static uint8_t zlink_crc8(const uint8_t *data, uint8_t len)
{
    uint8_t crc = 0x00u;
    uint8_t i;

    while (len > 0u) {
        crc ^= *data;
        data++;
        len--;
        for (i = 0u; i < 8u; ++i) {
            if ((crc & 0x80u) != 0u) {
                crc = (uint8_t)((crc << 1) ^ 0x07u);
            } else {
                crc <<= 1;
            }
        }
    }

    return crc;
}

static uint8_t zlink_compose_b0(uint8_t type)
{
    return (uint8_t)((ZLINK_SOF5 << 3) | (type & ZLINK_TYPE_MASK));
}

static uint8_t zlink_type_is_control(uint8_t type)
{
    return ((type == (uint8_t)ZLINK_TYPE_POLL) || (type == (uint8_t)ZLINK_TYPE_EMPTY)
        || (type == (uint8_t)ZLINK_TYPE_ACK) || (type == (uint8_t)ZLINK_TYPE_NACK)) ? 1u : 0u;
}

static uint8_t zlink_type_is_valid(uint8_t type)
{
    return (zlink_type_is_control(type) != 0u) || (type == (uint8_t)ZLINK_TYPE_DATA);
}

static uint8_t zlink_send_frame(uint8_t type, uint8_t seq, uint8_t tty, const uint8_t *payload, uint8_t len)
{
    uint8_t frame_no_crc[4u + ZLINK_MAX_PAYLOAD];
    uint8_t crc;
    uint8_t i;

    if (len > ZLINK_MAX_PAYLOAD) {
        return 0u;
    }

    frame_no_crc[0] = zlink_compose_b0(type);
    frame_no_crc[1] = (uint8_t)(tty & 0x0Fu);
    frame_no_crc[2] = len;
    frame_no_crc[3] = seq;
    for (i = 0u; i < len; ++i) {
        frame_no_crc[(uint8_t)(4u + i)] = payload[i];
    }
    crc = zlink_crc8(frame_no_crc, (uint8_t)(4u + len));

    io_write_port(frame_no_crc[0], g_zlink_io_port);
    io_write_port(frame_no_crc[1], g_zlink_io_port);
    io_write_port(frame_no_crc[2], g_zlink_io_port);
    io_write_port(frame_no_crc[3], g_zlink_io_port);
    for (i = 0u; i < len; ++i) {
        io_write_port(payload[i], g_zlink_io_port);
    }
    io_write_port(crc, g_zlink_io_port);

    return 1u;
}

static uint8_t zlink_read_frame(zlink_frame_t *out)
{
    uint8_t b0;
    uint8_t b1;
    uint8_t b2;
    uint8_t b3;
    uint8_t crc_rx;
    uint8_t crc;
    uint8_t type;
    uint8_t len;
    uint8_t i;

    b0 = io_read_port(g_zlink_io_port);
    b1 = io_read_port(g_zlink_io_port);
    b2 = io_read_port(g_zlink_io_port);
    b3 = io_read_port(g_zlink_io_port);

    if ((b0 >> 3) != ZLINK_SOF5) {
        return 0u;
    }

    type = (uint8_t)(b0 & ZLINK_TYPE_MASK);
    len = b2;

    out->type = type;
    out->tty = b1;
    out->seq = b3;
    out->len = len;

    if (len > ZLINK_MAX_PAYLOAD) {
        for (i = 0u; i < len; ++i) {
            (void)io_read_port(g_zlink_io_port);
        }
        (void)io_read_port(g_zlink_io_port);
        COUNTER_INC16_SAT(&g_zlink_rx_len_err);
        return 0u;
    }

    for (i = 0u; i < len; ++i) {
        out->payload[i] = io_read_port(g_zlink_io_port);
    }
    crc_rx = io_read_port(g_zlink_io_port);

    {
        uint8_t crc_buf[4u + ZLINK_MAX_PAYLOAD];
        crc_buf[0] = b0;
        crc_buf[1] = b1;
        crc_buf[2] = b2;
        crc_buf[3] = b3;
        for (i = 0u; i < len; ++i) {
            crc_buf[(uint8_t)(4u + i)] = out->payload[i];
        }
        crc = zlink_crc8(crc_buf, (uint8_t)(4u + len));
    }

    if (crc != crc_rx) {
        COUNTER_INC16_SAT(&g_zlink_rx_crc_err);
        return 0u;
    }

    if (zlink_type_is_control(type) != 0u) {
        if (len != 0u) {
            COUNTER_INC16_SAT(&g_zlink_rx_len_err);
            return 0u;
        }
    } else if (type == (uint8_t)ZLINK_TYPE_DATA) {
        if (len == 0u) {
            COUNTER_INC16_SAT(&g_zlink_rx_len_err);
            return 0u;
        }
    } else if (zlink_type_is_valid(type) == 0u) {
        COUNTER_INC16_SAT(&g_zlink_rx_type_err);
        return 0u;
    }

    return 1u;
}

void zlink_init(uint8_t io_port)
{
    uint8_t i;

    g_zlink_io_port = io_port;
    g_zlink_next_poll_seq = 0u;
    zlink_diag_reset();

    for (i = 0u; i < ZLINK_MAX_TTY; ++i) {
        g_zlink_next_tx_seq[i] = 0u;
        g_zlink_last_seq[i] = 0u;
        g_zlink_last_seq_valid[i] = 0u;
    }
}

uint8_t zlink_send_data(uint8_t tty, const uint8_t *payload, uint8_t len)
{
    uint8_t seq;

    if ((payload == (const uint8_t *)0) || (tty >= ZLINK_MAX_TTY) || (len == 0u) || (len > ZLINK_MAX_PAYLOAD)) {
        return 0u;
    }

    seq = g_zlink_next_tx_seq[tty];
    g_zlink_next_tx_seq[tty] = (uint8_t)(seq + 1u);

    return zlink_send_frame((uint8_t)ZLINK_TYPE_DATA, seq, tty, payload, len);
}

uint8_t zlink_send_ack(uint8_t seq, uint8_t tty)
{
    return zlink_send_frame((uint8_t)ZLINK_TYPE_ACK, seq, tty, (const uint8_t *)0, 0u);
}

uint8_t zlink_send_nack(uint8_t seq, uint8_t tty)
{
    return zlink_send_frame((uint8_t)ZLINK_TYPE_NACK, seq, tty, (const uint8_t *)0, 0u);
}

uint8_t zlink_poll_once(zlink_data_frame_t *out)
{
    zlink_frame_t in;
    uint8_t frame_budget = 4u;

    if (out == (zlink_data_frame_t *)0) {
        return 0u;
    }

    zlink_send_frame((uint8_t)ZLINK_TYPE_POLL, g_zlink_next_poll_seq, 0u, (const uint8_t *)0, 0u);
    g_zlink_next_poll_seq++;

    while (frame_budget > 0u) {
        frame_budget--;

        if (zlink_read_frame(&in) == 0u) {
            return 0u;
        }

        if (in.type == (uint8_t)ZLINK_TYPE_EMPTY) {
            return 0u;
        }

        if (in.type != (uint8_t)ZLINK_TYPE_DATA) {
            /* Control-only frames are ignored in RX delivery path. */
            continue;
        }

        if ((in.tty >= ZLINK_MAX_TTY) || (in.len == 0u) || (in.len > ZLINK_MAX_PAYLOAD)) {
            zlink_send_nack(in.seq, in.tty);
            COUNTER_INC16_SAT(&g_zlink_rx_len_err);
            return 0u;
        }

        if ((g_zlink_last_seq_valid[in.tty] != 0u) && (g_zlink_last_seq[in.tty] == in.seq)) {
            zlink_send_ack(in.seq, in.tty);
            COUNTER_INC16_SAT(&g_zlink_rx_dup);
            return 0u;
        }

        g_zlink_last_seq[in.tty] = in.seq;
        g_zlink_last_seq_valid[in.tty] = 1u;

        out->tty = in.tty;
        out->seq = in.seq;
        out->len = in.len;
        {
            uint8_t i;
            for (i = 0u; i < in.len; ++i) {
                out->payload[i] = in.payload[i];
            }
        }

        zlink_send_ack(in.seq, in.tty);
        COUNTER_INC16_SAT(&g_zlink_rx_frames_ok);
        return 1u;
    }

    return 0u;
}

void zlink_diag_snapshot_raw(zlink_diag_stats_t *out)
{
    if (out == (zlink_diag_stats_t *)0) {
        return;
    }

    out->rx_frames_ok = g_zlink_rx_frames_ok;
    out->rx_crc_err = g_zlink_rx_crc_err;
    out->rx_dup = g_zlink_rx_dup;
    out->rx_type_err = g_zlink_rx_type_err;
    out->rx_len_err = g_zlink_rx_len_err;
}

void zlink_diag_snapshot(zlink_diag_stats_t *out)
{
    if (out == (zlink_diag_stats_t *)0) {
        return;
    }

    CPU_DI();
    zlink_diag_snapshot_raw(out);
    CPU_EI();
}

void zlink_diag_reset(void)
{
    CPU_DI();
    g_zlink_rx_frames_ok = 0u;
    g_zlink_rx_crc_err = 0u;
    g_zlink_rx_dup = 0u;
    g_zlink_rx_type_err = 0u;
    g_zlink_rx_len_err = 0u;
    CPU_EI();
}
