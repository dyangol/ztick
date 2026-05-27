#ifndef ZLINK_H
#define ZLINK_H

#include <stdint.h>

#ifndef ZLINK_MAX_PAYLOAD
#define ZLINK_MAX_PAYLOAD 64u
#endif

typedef enum zlink_type {
    ZLINK_TYPE_POLL = 0u,
    ZLINK_TYPE_EMPTY = 1u,
    ZLINK_TYPE_DATA = 2u,
    ZLINK_TYPE_ACK = 3u,
    ZLINK_TYPE_NACK = 4u
} zlink_type_t;

typedef struct zlink_data_frame {
    uint8_t tty;
    uint8_t seq;
    uint8_t len;
    uint8_t payload[ZLINK_MAX_PAYLOAD];
} zlink_data_frame_t;

void zlink_init(uint8_t io_port);
uint8_t zlink_poll_once(zlink_data_frame_t *out);
uint8_t zlink_send_data(uint8_t tty, const uint8_t *payload, uint8_t len);

#endif
