#ifndef ZBUS_H
#define ZBUS_H

#include <stdint.h>

#ifndef ZBUS_BUFFER_SIZE
#define ZBUS_BUFFER_SIZE 96u
#endif

#ifndef ZBUS_MAX_TTY
#define ZBUS_MAX_TTY 10u
#endif

#ifndef ZBUS_TX_QUEUE_SIZE
#define ZBUS_TX_QUEUE_SIZE 8u
#endif

#ifndef ZBUS_FRAME_MAX_LEN
#define ZBUS_FRAME_MAX_LEN 64u
#endif

#ifndef ZBUS_TASK_SLOTS
#define ZBUS_TASK_SLOTS 6u
#endif

#ifndef ZBUS_INVALID_TTY_ID
#define ZBUS_INVALID_TTY_ID 0xFFu
#endif

#ifndef ZBUS_KERNEL_TTY_ID
#define ZBUS_KERNEL_TTY_ID 15u
#endif

#ifndef ZBUS_RX_CHUNK
#define ZBUS_RX_CHUNK 1u
#endif

#ifndef ZBUS_TX_CHUNK
#define ZBUS_TX_CHUNK 2u
#endif

typedef struct zbus_stats {
    uint16_t tx_drop;
    uint16_t rx_overflow;
    uint16_t attach_fail;
    uint16_t zlink_rx_frames_ok;
    uint16_t zlink_rx_crc_err;
    uint16_t zlink_rx_dup;
    uint16_t zlink_rx_type_err;
    uint16_t zlink_rx_len_err;
} zbus_stats_t;

void zbus_init(void);
void zbus_tick(void);
uint8_t zbus_tty_attach(uint8_t io_port, uint8_t *out_tty_id);
uint8_t zbus_tty_attach_for_task(uint8_t owner_task_id, uint8_t io_port, uint8_t enable_rx_polling, uint8_t *out_tty_id);
uint8_t zbus_tty_get_for_task(uint8_t owner_task_id, uint8_t *out_tty_id);
uint8_t zbus_tty_get_current(uint8_t *out_tty_id);
uint8_t zbus_tty_detach(uint8_t tty_id);
/* Caller controls IRQ masking; this path is used by kernel-owned lifecycle code. */
uint8_t zbus_tty_detach_for_task(uint8_t owner_task_id);
uint8_t zbus_tty_set_rx_polling(uint8_t tty_id, uint8_t enabled);
uint8_t zbus_write_tty(uint8_t tty_id, const uint8_t *data, uint8_t len);
uint8_t zbus_read_tty(uint8_t tty_id, uint8_t *out, uint8_t maxlen);
uint8_t zbus_tty_tx_pending(uint8_t tty_id);
uint8_t zbus_tty_tx_pending_current(void);
void zbus_stats_snapshot(zbus_stats_t *out);
void zbus_stats_reset(void);
uint8_t zbus_write(const uint8_t *data, uint8_t len);
uint8_t zbus_read(uint8_t *out, uint8_t maxlen);

#endif
