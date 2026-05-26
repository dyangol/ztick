#ifndef ZLINK_DIAG_H
#define ZLINK_DIAG_H

#include <stdint.h>

typedef struct zlink_diag_stats {
    uint16_t rx_frames_ok;
    uint16_t rx_crc_err;
    uint16_t rx_dup;
    uint16_t rx_type_err;
    uint16_t rx_len_err;
} zlink_diag_stats_t;

void zlink_diag_snapshot(zlink_diag_stats_t *out);
void zlink_diag_snapshot_raw(zlink_diag_stats_t *out);
void zlink_diag_reset(void);

#endif
