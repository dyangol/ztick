#include <stdint.h>

#include "common.h"
#include "io.h"
#include "zbus.h"
#include "zlink.h"
#include "zlink_diag.h"
#include "../bootstrap/rtos.h"

#pragma codeseg CODE

#define ZBUS_TTY_DETACHED 0u
#define ZBUS_TTY_ATTACHED 1u
#define ZBUS_KERNEL_CTRL_CMD_GET_STATS 0x01u
#define ZBUS_KERNEL_CTRL_CMD_GET_TASK_INFO 0x02u
#define ZBUS_KERNEL_CTRL_CMD_GET_TASK_LIST 0x03u
#define ZBUS_KERNEL_CTRL_CMD_GET_STACK_WM 0x07u
#define ZBUS_KERNEL_CTRL_RSP_STATS 0x81u
#define ZBUS_KERNEL_CTRL_RSP_TASK_INFO 0x82u
#define ZBUS_KERNEL_CTRL_RSP_TASK_LIST 0x83u
#define ZBUS_KERNEL_CTRL_RSP_STACK_WM 0x87u
#define ZBUS_KERNEL_CTRL_STATUS_OK 0x00u
#define ZBUS_KERNEL_CTRL_STATUS_MORE 0x80u
#define ZBUS_KERNEL_CTRL_STATUS_BAD_CMD 0x01u
#define ZBUS_KERNEL_CTRL_STATUS_BAD_LEN 0x02u
#define ZBUS_KERNEL_CTRL_STATUS_BAD_TASK 0x03u
#define ZBUS_KERNEL_CTRL_REPLY_STATS_LEN 18u
#define ZBUS_KERNEL_CTRL_REPLY_TASKINFO_LEN 16u
#define ZBUS_KERNEL_CTRL_REPLY_TASKLIST_MAX_LEN 64u
#define ZBUS_KERNEL_CTRL_TASKLIST_ENTRY_LEN 11u
#define ZBUS_KERNEL_CTRL_REPLY_STACK_WM_LEN 10u

typedef struct zbus_tty_entry {
    uint8_t state;
    uint8_t owner_task_id;
    uint8_t io_port;
    uint8_t rx_polling_enabled;
} zbus_tty_entry_t;

typedef struct zbus_tx_frame {
    uint8_t len;
    uint8_t data[ZBUS_FRAME_MAX_LEN];
} zbus_tx_frame_t;

static volatile zbus_tty_entry_t g_zbus_tty_table[ZBUS_MAX_TTY];
static volatile zbus_tx_frame_t g_zbus_tx_queue[ZBUS_MAX_TTY][ZBUS_TX_QUEUE_SIZE];
static volatile uint8_t g_zbus_tx_head[ZBUS_MAX_TTY];
static volatile uint8_t g_zbus_tx_tail[ZBUS_MAX_TTY];
static volatile uint8_t g_zbus_tx_count[ZBUS_MAX_TTY];

static volatile uint8_t g_zbus_rx_buf[ZBUS_MAX_TTY][ZBUS_BUFFER_SIZE];
static volatile uint8_t g_zbus_rx_head[ZBUS_MAX_TTY];
static volatile uint8_t g_zbus_rx_tail[ZBUS_MAX_TTY];
static volatile uint8_t g_zbus_rx_count[ZBUS_MAX_TTY];

static volatile uint8_t g_zbus_legacy_tty_by_task[MAX_TASKS];
static volatile uint8_t g_zbus_tx_rr_cursor;

static volatile uint16_t g_zbus_stat_tx_drop;
static volatile uint16_t g_zbus_stat_rx_overflow;
static volatile uint16_t g_zbus_stat_attach_fail;

static uint8_t zbus_is_valid_tty(uint8_t tty_id)
{
    return (tty_id < ZBUS_MAX_TTY) ? 1u : 0u;
}

static uint8_t zbus_tty_is_attached(uint8_t tty_id)
{
    return (g_zbus_tty_table[tty_id].state == ZBUS_TTY_ATTACHED) ? 1u : 0u;
}

static uint8_t zbus_tty_match_owner(uint8_t tty_id, uint8_t owner_task_id)
{
    return (zbus_tty_is_attached(tty_id) != 0u) && (g_zbus_tty_table[tty_id].owner_task_id == owner_task_id);
}


static uint8_t zbus_ring_advance(uint8_t index, uint8_t size)
{
    index++;
    if (index >= size) {
        index = 0u;
    }
    return index;
}

static uint8_t zbus_rr_tty(uint8_t start, uint8_t offset)
{
    uint8_t tty_id = (uint8_t)(start + offset);
    if (tty_id >= ZBUS_MAX_TTY) {
        tty_id = (uint8_t)(tty_id - ZBUS_MAX_TTY);
    }
    return tty_id;
}

static void zbus_reset_rx_queue(uint8_t tty_id)
{
    g_zbus_rx_head[tty_id] = 0u;
    g_zbus_rx_tail[tty_id] = 0u;
    g_zbus_rx_count[tty_id] = 0u;
}

static void zbus_reset_tx_queue(uint8_t tty_id)
{
    g_zbus_tx_head[tty_id] = 0u;
    g_zbus_tx_tail[tty_id] = 0u;
    g_zbus_tx_count[tty_id] = 0u;
}

static void zbus_tx_pop_frame(uint8_t tty_id)
{
    g_zbus_tx_tail[tty_id] = zbus_ring_advance(g_zbus_tx_tail[tty_id], (uint8_t)ZBUS_TX_QUEUE_SIZE);
    g_zbus_tx_count[tty_id]--;
    g_zbus_tx_rr_cursor = zbus_ring_advance(tty_id, (uint8_t)ZBUS_MAX_TTY);
}

static void zbus_set_tty_detached(uint8_t tty_id)
{
    g_zbus_tty_table[tty_id].state = ZBUS_TTY_DETACHED;
    g_zbus_tty_table[tty_id].owner_task_id = (uint8_t)ZBUS_INVALID_TTY_ID;
    g_zbus_tty_table[tty_id].io_port = (uint8_t)IO_DEFAULT_PORT;
    g_zbus_tty_table[tty_id].rx_polling_enabled = 0u;
    zbus_reset_tx_queue(tty_id);
    zbus_reset_rx_queue(tty_id);
}

static void zbus_set_tty_attached(uint8_t tty_id, uint8_t owner_task_id, uint8_t io_port)
{
    g_zbus_tty_table[tty_id].state = ZBUS_TTY_ATTACHED;
    g_zbus_tty_table[tty_id].owner_task_id = owner_task_id;
    g_zbus_tty_table[tty_id].io_port = io_port;
    g_zbus_tty_table[tty_id].rx_polling_enabled = 0u;
    zbus_reset_tx_queue(tty_id);
    zbus_reset_rx_queue(tty_id);
}

static uint8_t zbus_text_append_cstr(uint8_t *buf, uint8_t *io_len, uint8_t max_len, const uint8_t *text)
{
    uint8_t len = *io_len;

    if ((buf == (uint8_t *)0) || (io_len == (uint8_t *)0) || (text == (const uint8_t *)0)) {
        return 0u;
    }

    while (*text != 0u) {
        if (len >= max_len) {
            return 0u;
        }
        buf[len] = *text;
        len++;
        text++;
    }

    *io_len = len;
    return 1u;
}

static uint8_t zbus_text_append_u8_dec(uint8_t *buf, uint8_t *io_len, uint8_t max_len, uint8_t value)
{
    uint8_t tmp[3];
    uint8_t digits = 0u;
    uint8_t i;
    uint8_t len = *io_len;

    if ((buf == (uint8_t *)0) || (io_len == (uint8_t *)0)) {
        return 0u;
    }

    if (value == 0u) {
        if (len >= max_len) {
            return 0u;
        }
        buf[len] = (uint8_t)'0';
        *io_len = (uint8_t)(len + 1u);
        return 1u;
    }

    while (value > 0u) {
        tmp[digits] = (uint8_t)('0' + (value % 10u));
        digits++;
        value = (uint8_t)(value / 10u);
    }

    for (i = digits; i > 0u; --i) {
        if (len >= max_len) {
            return 0u;
        }
        buf[len] = tmp[(uint8_t)(i - 1u)];
        len++;
    }

    *io_len = len;
    return 1u;
}

static void zbus_kernel_emit_tty_fallback(uint8_t owner_task_id, uint8_t requested_tty, uint8_t assigned_tty)
{
    uint8_t payload[64];
    uint8_t len = 0u;
    uint8_t ok = 1u;

    ok = zbus_text_append_cstr(payload, &len, (uint8_t)sizeof(payload), (const uint8_t *)"kern tty-fallback task=");
    if (ok != 0u) {
        ok = zbus_text_append_u8_dec(payload, &len, (uint8_t)sizeof(payload), owner_task_id);
    }
    if (ok != 0u) {
        ok = zbus_text_append_cstr(payload, &len, (uint8_t)sizeof(payload), (const uint8_t *)" req=");
    }
    if (ok != 0u) {
        ok = zbus_text_append_u8_dec(payload, &len, (uint8_t)sizeof(payload), requested_tty);
    }
    if (ok != 0u) {
        ok = zbus_text_append_cstr(payload, &len, (uint8_t)sizeof(payload), (const uint8_t *)" got=");
    }
    if (ok != 0u) {
        ok = zbus_text_append_u8_dec(payload, &len, (uint8_t)sizeof(payload), assigned_tty);
    }

    if ((ok != 0u) && (len > 0u)) {
        if (zlink_send_data((uint8_t)ZBUS_KERNEL_TTY_ID, payload, len) == 0u) {
            COUNTER_INC16_SAT(&g_zbus_stat_tx_drop);
        }
    }
}

static uint8_t zbus_tty_attach_exact_internal(uint8_t owner_task_id, uint8_t tty_id, uint8_t io_port, uint8_t enable_rx_polling, uint8_t *out_tty_id)
{
    if ((out_tty_id == (uint8_t *)0) || (zbus_is_valid_tty(tty_id) == 0u)) {
        return 0u;
    }
    if (g_zbus_tty_table[tty_id].state != ZBUS_TTY_DETACHED) {
        return 0u;
    }

    zbus_set_tty_attached(tty_id, owner_task_id, io_port);
    g_zbus_tty_table[tty_id].rx_polling_enabled = (enable_rx_polling != 0u) ? 1u : 0u;
    if (owner_task_id < MAX_TASKS) {
        g_zbus_legacy_tty_by_task[owner_task_id] = tty_id;
    }
    *out_tty_id = tty_id;
    return 1u;
}

static void zbus_stats_clear(void)
{
    g_zbus_stat_tx_drop = 0u;
    g_zbus_stat_rx_overflow = 0u;
    g_zbus_stat_attach_fail = 0u;
}

static void zbus_stats_snapshot_raw(zbus_stats_t *out)
{
    zlink_diag_stats_t zlink_stats;

    zlink_diag_snapshot_raw(&zlink_stats);
    out->tx_drop = g_zbus_stat_tx_drop;
    out->rx_overflow = g_zbus_stat_rx_overflow;
    out->attach_fail = g_zbus_stat_attach_fail;
    out->zlink_rx_frames_ok = zlink_stats.rx_frames_ok;
    out->zlink_rx_crc_err = zlink_stats.rx_crc_err;
    out->zlink_rx_dup = zlink_stats.rx_dup;
    out->zlink_rx_type_err = zlink_stats.rx_type_err;
    out->zlink_rx_len_err = zlink_stats.rx_len_err;
}

static void zbus_kernel_send_payload(const uint8_t *payload, uint8_t len)
{
    if (zlink_send_data((uint8_t)ZBUS_KERNEL_TTY_ID, payload, len) == 0u) {
        COUNTER_INC16_SAT(&g_zbus_stat_tx_drop);
    }
}

static void zbus_kernel_send_stats_reply(uint8_t status)
{
    uint8_t payload[ZBUS_KERNEL_CTRL_REPLY_STATS_LEN];
    zbus_stats_t stats;

    payload[0] = (uint8_t)ZBUS_KERNEL_CTRL_RSP_STATS;
    payload[1] = status;

    if (status == (uint8_t)ZBUS_KERNEL_CTRL_STATUS_OK) {
        zbus_stats_snapshot_raw(&stats);
        STORE_U16_LE(payload, 2u, stats.tx_drop);
        STORE_U16_LE(payload, 4u, stats.rx_overflow);
        STORE_U16_LE(payload, 6u, stats.attach_fail);
        STORE_U16_LE(payload, 8u, stats.zlink_rx_frames_ok);
        STORE_U16_LE(payload, 10u, stats.zlink_rx_crc_err);
        STORE_U16_LE(payload, 12u, stats.zlink_rx_dup);
        STORE_U16_LE(payload, 14u, stats.zlink_rx_type_err);
        STORE_U16_LE(payload, 16u, stats.zlink_rx_len_err);
    } else {
        uint8_t i;
        for (i = 2u; i < (uint8_t)ZBUS_KERNEL_CTRL_REPLY_STATS_LEN; ++i) {
            payload[i] = 0u;
        }
    }

    zbus_kernel_send_payload(payload, (uint8_t)ZBUS_KERNEL_CTRL_REPLY_STATS_LEN);
}

static void zbus_kernel_send_task_info_reply(uint8_t status, uint8_t task_id)
{
    uint8_t payload[ZBUS_KERNEL_CTRL_REPLY_TASKINFO_LEN];
    uint8_t tty_id = (uint8_t)ZBUS_INVALID_TTY_ID;
    uint8_t i;

    payload[0] = (uint8_t)ZBUS_KERNEL_CTRL_RSP_TASK_INFO;
    payload[1] = status;
    payload[2] = task_id;

    for (i = 3u; i < (uint8_t)ZBUS_KERNEL_CTRL_REPLY_TASKINFO_LEN; ++i) {
        payload[i] = 0u;
    }

    if ((status == (uint8_t)ZBUS_KERNEL_CTRL_STATUS_OK) && (task_id < MAX_TASKS)) {
        uint8_t name_len = g_tasks[task_id].name_len;
        payload[3] = g_tasks[task_id].state;
        if (zbus_tty_get_for_task(task_id, &tty_id) == 0u) {
            tty_id = (uint8_t)ZBUS_INVALID_TTY_ID;
        }
        payload[4] = tty_id;
        STORE_U16_LE(payload, 5u, g_tasks[task_id].sp);
        if (name_len > TASK_NAME_MAX) {
            name_len = TASK_NAME_MAX;
        }
        payload[7] = name_len;
        for (i = 0u; i < TASK_NAME_MAX; ++i) {
            payload[(uint8_t)(8u + i)] = g_tasks[task_id].name[i];
        }
    }

    zbus_kernel_send_payload(payload, (uint8_t)ZBUS_KERNEL_CTRL_REPLY_TASKINFO_LEN);
}

static void zbus_kernel_send_task_list_reply(uint8_t status)
{
    uint8_t payload[ZBUS_KERNEL_CTRL_REPLY_TASKLIST_MAX_LEN];
    uint8_t len = 3u;
    uint8_t count = 0u;
    uint8_t task_id;

    payload[0] = (uint8_t)ZBUS_KERNEL_CTRL_RSP_TASK_LIST;
    payload[1] = status;
    payload[2] = 0u;

    if (status != (uint8_t)ZBUS_KERNEL_CTRL_STATUS_OK) {
        zbus_kernel_send_payload(payload, len);
        return;
    }

    for (task_id = 0u; task_id < MAX_TASKS; ++task_id) {
        uint8_t name_len;
        uint8_t tty_id = (uint8_t)ZBUS_INVALID_TTY_ID;
        uint8_t i;

        if (g_tasks[task_id].state == TASK_UNUSED) {
            continue;
        }

        if ((uint8_t)(len + ZBUS_KERNEL_CTRL_TASKLIST_ENTRY_LEN) > (uint8_t)ZBUS_KERNEL_CTRL_REPLY_TASKLIST_MAX_LEN) {
            payload[1] = (uint8_t)(ZBUS_KERNEL_CTRL_STATUS_OK | ZBUS_KERNEL_CTRL_STATUS_MORE);
            payload[2] = count;
            zbus_kernel_send_payload(payload, len);

            len = 3u;
            count = 0u;
            payload[0] = (uint8_t)ZBUS_KERNEL_CTRL_RSP_TASK_LIST;
            payload[1] = (uint8_t)ZBUS_KERNEL_CTRL_STATUS_OK;
            payload[2] = 0u;
        }

        name_len = g_tasks[task_id].name_len;
        if (name_len > TASK_NAME_MAX) {
            name_len = TASK_NAME_MAX;
        }
        if (zbus_tty_get_for_task(task_id, &tty_id) == 0u) {
            tty_id = (uint8_t)ZBUS_INVALID_TTY_ID;
        }

        payload[len] = task_id;
        payload[(uint8_t)(len + 1u)] = tty_id;
        payload[(uint8_t)(len + 2u)] = name_len;
        for (i = 0u; i < TASK_NAME_MAX; ++i) {
            payload[(uint8_t)(len + 3u + i)] = g_tasks[task_id].name[i];
        }

        len = (uint8_t)(len + ZBUS_KERNEL_CTRL_TASKLIST_ENTRY_LEN);
        count++;
    }

    payload[2] = count;
    zbus_kernel_send_payload(payload, len);
}

static void zbus_kernel_send_stack_wm_reply(uint8_t status, uint8_t task_id)
{
    uint8_t payload[ZBUS_KERNEL_CTRL_REPLY_STACK_WM_LEN];
    uint16_t peak_used = 0u;
    uint16_t current_used = 0u;
    uint16_t stack_size = 0u;

    payload[0] = (uint8_t)ZBUS_KERNEL_CTRL_RSP_STACK_WM;
    payload[1] = status;
    payload[2] = task_id;
    payload[3] = 0u;
    STORE_U16_LE(payload, 4u, 0u);
    STORE_U16_LE(payload, 6u, 0u);
    STORE_U16_LE(payload, 8u, 0u);

    if ((status == (uint8_t)ZBUS_KERNEL_CTRL_STATUS_OK) && (task_id < MAX_TASKS)) {
        if (rtos_stack_watermark_isr(task_id, &peak_used, &current_used, &stack_size) == 0u) {
            payload[1] = (uint8_t)ZBUS_KERNEL_CTRL_STATUS_BAD_TASK;
        } else {
            payload[3] = g_tasks[task_id].state;
            STORE_U16_LE(payload, 4u, stack_size);
            STORE_U16_LE(payload, 6u, peak_used);
            STORE_U16_LE(payload, 8u, current_used);
        }
    }

    zbus_kernel_send_payload(payload, (uint8_t)ZBUS_KERNEL_CTRL_REPLY_STACK_WM_LEN);
}

static void zbus_kernel_handle_control(const zlink_data_frame_t *rx_frame)
{
    uint8_t cmd;

    if ((rx_frame == (const zlink_data_frame_t *)0) || (rx_frame->len == 0u)) {
        zbus_kernel_send_stats_reply((uint8_t)ZBUS_KERNEL_CTRL_STATUS_BAD_LEN);
        return;
    }

    cmd = rx_frame->payload[0];
    if (cmd == (uint8_t)ZBUS_KERNEL_CTRL_CMD_GET_STATS) {
        if (rx_frame->len != 1u) {
            zbus_kernel_send_stats_reply((uint8_t)ZBUS_KERNEL_CTRL_STATUS_BAD_LEN);
            return;
        }
        zbus_kernel_send_stats_reply((uint8_t)ZBUS_KERNEL_CTRL_STATUS_OK);
    } else if (cmd == (uint8_t)ZBUS_KERNEL_CTRL_CMD_GET_TASK_INFO) {
        uint8_t task_id;
        if (rx_frame->len > 2u) {
            zbus_kernel_send_task_info_reply((uint8_t)ZBUS_KERNEL_CTRL_STATUS_BAD_LEN, g_current_task);
            return;
        }
        task_id = (rx_frame->len == 2u) ? rx_frame->payload[1] : g_current_task;
        if (task_id >= MAX_TASKS) {
            zbus_kernel_send_task_info_reply((uint8_t)ZBUS_KERNEL_CTRL_STATUS_BAD_TASK, task_id);
            return;
        }
        zbus_kernel_send_task_info_reply((uint8_t)ZBUS_KERNEL_CTRL_STATUS_OK, task_id);
    } else if (cmd == (uint8_t)ZBUS_KERNEL_CTRL_CMD_GET_TASK_LIST) {
        if (rx_frame->len != 1u) {
            zbus_kernel_send_task_list_reply((uint8_t)ZBUS_KERNEL_CTRL_STATUS_BAD_LEN);
            return;
        }
        zbus_kernel_send_task_list_reply((uint8_t)ZBUS_KERNEL_CTRL_STATUS_OK);
    } else if (cmd == (uint8_t)ZBUS_KERNEL_CTRL_CMD_GET_STACK_WM) {
        uint8_t task_id;
        if (rx_frame->len > 2u) {
            zbus_kernel_send_stack_wm_reply((uint8_t)ZBUS_KERNEL_CTRL_STATUS_BAD_LEN, g_current_task);
            return;
        }
        task_id = (rx_frame->len == 2u) ? rx_frame->payload[1] : g_current_task;
        if (task_id >= MAX_TASKS) {
            zbus_kernel_send_stack_wm_reply((uint8_t)ZBUS_KERNEL_CTRL_STATUS_BAD_TASK, task_id);
            return;
        }
        zbus_kernel_send_stack_wm_reply((uint8_t)ZBUS_KERNEL_CTRL_STATUS_OK, task_id);
    } else {
        zbus_kernel_send_stats_reply((uint8_t)ZBUS_KERNEL_CTRL_STATUS_BAD_CMD);
    }
}

static void zbus_rx_enqueue_payload(uint8_t tty_id, const zlink_data_frame_t *rx_frame)
{
    uint8_t payload_index;
    uint8_t head;

    for (payload_index = 0u; payload_index < rx_frame->len; ++payload_index) {
        if (g_zbus_rx_count[tty_id] >= ZBUS_BUFFER_SIZE) {
            COUNTER_INC16_SAT(&g_zbus_stat_rx_overflow);
            break;
        }

        head = g_zbus_rx_head[tty_id];
        g_zbus_rx_buf[tty_id][head] = rx_frame->payload[payload_index];
        head = zbus_ring_advance(head, (uint8_t)ZBUS_BUFFER_SIZE);
        g_zbus_rx_head[tty_id] = head;
        g_zbus_rx_count[tty_id]++;
    }
}

static uint8_t zbus_legacy_get_or_attach(void)
{
    uint8_t task_id = g_current_task;
    uint8_t tty_id;

    if (task_id >= MAX_TASKS) {
        return (uint8_t)ZBUS_INVALID_TTY_ID;
    }

    tty_id = g_zbus_legacy_tty_by_task[task_id];
    if ((tty_id != (uint8_t)ZBUS_INVALID_TTY_ID)
        && (zbus_is_valid_tty(tty_id) != 0u)
        && (zbus_tty_match_owner(tty_id, task_id) != 0u)) {
        return tty_id;
    }

    if (zbus_tty_attach((uint8_t)IO_DEFAULT_PORT, &tty_id) == 0u) {
        return (uint8_t)ZBUS_INVALID_TTY_ID;
    }

    g_zbus_legacy_tty_by_task[task_id] = tty_id;
    return tty_id;
}

static uint8_t zbus_tty_attach_internal(uint8_t owner_task_id, uint8_t io_port, uint8_t enable_rx_polling, uint8_t *out_tty_id)
{
    uint8_t i;

    if (out_tty_id == (uint8_t *)0) {
        COUNTER_INC16_SAT(&g_zbus_stat_attach_fail);
        return 0u;
    }

    for (i = 0u; i < ZBUS_MAX_TTY; ++i) {
        if (g_zbus_tty_table[i].state == ZBUS_TTY_DETACHED) {
            zbus_set_tty_attached(i, owner_task_id, io_port);
            g_zbus_tty_table[i].rx_polling_enabled = (enable_rx_polling != 0u) ? 1u : 0u;
            if (owner_task_id < MAX_TASKS) {
                g_zbus_legacy_tty_by_task[owner_task_id] = i;
            }
            *out_tty_id = i;
            return 1u;
        }
    }

    COUNTER_INC16_SAT(&g_zbus_stat_attach_fail);
    return 0u;
}

void zbus_init(void)
{
    uint8_t i;

    g_zbus_tx_rr_cursor = 0u;
    zbus_stats_clear();
    zlink_init((uint8_t)IO_DEFAULT_PORT);

    for (i = 0u; i < ZBUS_MAX_TTY; ++i) {
        zbus_set_tty_detached(i);
    }

    for (i = 0u; i < MAX_TASKS; ++i) {
        g_zbus_legacy_tty_by_task[i] = (uint8_t)ZBUS_INVALID_TTY_ID;
    }
}

uint8_t zbus_tty_attach(uint8_t io_port, uint8_t *out_tty_id)
{
    CPU_DI();
    {
        uint8_t ok = zbus_tty_attach_internal(g_current_task, io_port, 0u, out_tty_id);
        CPU_EI();
        return ok;
    }
}

uint8_t zbus_tty_attach_for_task(uint8_t owner_task_id, uint8_t io_port, uint8_t enable_rx_polling, uint8_t *out_tty_id)
{
    return zbus_tty_attach_for_task_preferred(owner_task_id, (uint8_t)ZBUS_TTY_AUTO, io_port, enable_rx_polling, out_tty_id, (uint8_t *)0);
}

uint8_t zbus_tty_attach_for_task_preferred(uint8_t owner_task_id, uint8_t preferred_tty, uint8_t io_port, uint8_t enable_rx_polling, uint8_t *out_tty_id, uint8_t *out_fallback)
{
    uint8_t had_preference = (preferred_tty != (uint8_t)ZBUS_TTY_AUTO) ? 1u : 0u;

    if (out_fallback != (uint8_t *)0) {
        *out_fallback = 0u;
    }
    if (owner_task_id >= MAX_TASKS) {
        return 0u;
    }

    /* Caller controls IRQ masking; this path is used by kernel-owned setup. */
    if ((had_preference != 0u) && (preferred_tty < ZBUS_MAX_TTY)) {
        if (zbus_tty_attach_exact_internal(owner_task_id, preferred_tty, io_port, enable_rx_polling, out_tty_id) != 0u) {
            return 1u;
        }
    }

    if (zbus_tty_attach_internal(owner_task_id, io_port, enable_rx_polling, out_tty_id) == 0u) {
        return 0u;
    }

    if ((had_preference != 0u) && (preferred_tty < ZBUS_MAX_TTY) && (out_tty_id != (uint8_t *)0)) {
        if (out_fallback != (uint8_t *)0) {
            *out_fallback = 1u;
        }
        zbus_kernel_emit_tty_fallback(owner_task_id, preferred_tty, *out_tty_id);
    }
    return 1u;
}

uint8_t zbus_tty_get_for_task(uint8_t owner_task_id, uint8_t *out_tty_id)
{
    uint8_t tty_id;

    if ((out_tty_id == (uint8_t *)0) || (owner_task_id >= MAX_TASKS)) {
        return 0u;
    }

    tty_id = g_zbus_legacy_tty_by_task[owner_task_id];
    if (tty_id == (uint8_t)ZBUS_INVALID_TTY_ID) {
        return 0u;
    }

    if ((zbus_is_valid_tty(tty_id) == 0u) || (zbus_tty_match_owner(tty_id, owner_task_id) == 0u)) {
        return 0u;
    }

    *out_tty_id = tty_id;
    return 1u;
}

uint8_t zbus_tty_get_current(uint8_t *out_tty_id)
{
    return zbus_tty_get_for_task(g_current_task, out_tty_id);
}

uint8_t zbus_tty_detach(uint8_t tty_id)
{
    uint8_t task_id;

    if (zbus_is_valid_tty(tty_id) == 0u) {
        return 0u;
    }

    CPU_DI();

    if (zbus_tty_match_owner(tty_id, g_current_task) == 0u) {
        CPU_EI();
        return 0u;
    }

    zbus_set_tty_detached(tty_id);

    task_id = g_current_task;
    if ((task_id < MAX_TASKS) && (g_zbus_legacy_tty_by_task[task_id] == tty_id)) {
        g_zbus_legacy_tty_by_task[task_id] = (uint8_t)ZBUS_INVALID_TTY_ID;
    }

    CPU_EI();
    return 1u;
}

uint8_t zbus_tty_detach_for_task(uint8_t owner_task_id)
{
    uint8_t tty_id;
    uint8_t found = 0u;

    if (owner_task_id >= MAX_TASKS) {
        return 0u;
    }

    for (tty_id = 0u; tty_id < ZBUS_MAX_TTY; ++tty_id) {
        if ((zbus_tty_is_attached(tty_id) != 0u) && (g_zbus_tty_table[tty_id].owner_task_id == owner_task_id)) {
            zbus_set_tty_detached(tty_id);
            found = 1u;
        }
    }

    g_zbus_legacy_tty_by_task[owner_task_id] = (uint8_t)ZBUS_INVALID_TTY_ID;
    return found;
}

uint8_t zbus_tty_set_rx_polling(uint8_t tty_id, uint8_t enabled)
{
    if (zbus_is_valid_tty(tty_id) == 0u) {
        return 0u;
    }

    CPU_DI();

    if (zbus_tty_match_owner(tty_id, g_current_task) == 0u) {
        CPU_EI();
        return 0u;
    }

    g_zbus_tty_table[tty_id].rx_polling_enabled = (enabled != 0u) ? 1u : 0u;

    CPU_EI();
    return 1u;
}

uint8_t zbus_write_tty(uint8_t tty_id, const uint8_t *data, uint8_t len)
{
    uint8_t frame_index;
    uint8_t i;

    if ((data == (const uint8_t *)0) || (len == 0u) || (len > ZBUS_FRAME_MAX_LEN)) {
        return 0u;
    }

    if (zbus_is_valid_tty(tty_id) == 0u) {
        return 0u;
    }

    CPU_DI();

    if (zbus_tty_match_owner(tty_id, g_current_task) == 0u) {
        CPU_EI();
        return 0u;
    }

    if (g_zbus_tx_count[tty_id] >= ZBUS_TX_QUEUE_SIZE) {
        COUNTER_INC16_SAT(&g_zbus_stat_tx_drop);
        CPU_EI();
        return 0u;
    }

    frame_index = g_zbus_tx_head[tty_id];
    g_zbus_tx_queue[tty_id][frame_index].len = len;
    for (i = 0u; i < len; ++i) {
        g_zbus_tx_queue[tty_id][frame_index].data[i] = data[i];
    }

    g_zbus_tx_head[tty_id] = zbus_ring_advance(g_zbus_tx_head[tty_id], (uint8_t)ZBUS_TX_QUEUE_SIZE);
    g_zbus_tx_count[tty_id]++;

    CPU_EI();
    return len;
}

uint8_t zbus_read_tty(uint8_t tty_id, uint8_t *out, uint8_t maxlen)
{
    uint8_t read = 0u;
    uint8_t tail;

    if ((out == (uint8_t *)0) || (maxlen == 0u)) {
        return 0u;
    }

    if (zbus_is_valid_tty(tty_id) == 0u) {
        return 0u;
    }

    CPU_DI();

    if (zbus_tty_match_owner(tty_id, g_current_task) == 0u) {
        CPU_EI();
        return 0u;
    }

    while (read < maxlen) {
        if (g_zbus_rx_count[tty_id] == 0u) {
            break;
        }

        tail = g_zbus_rx_tail[tty_id];
        out[read] = g_zbus_rx_buf[tty_id][tail];
        tail = zbus_ring_advance(tail, (uint8_t)ZBUS_BUFFER_SIZE);
        g_zbus_rx_tail[tty_id] = tail;
        g_zbus_rx_count[tty_id]--;
        read++;
    }

    CPU_EI();
    return read;
}

uint8_t zbus_tty_tx_pending(uint8_t tty_id)
{
    if (zbus_is_valid_tty(tty_id) == 0u) {
        return 0u;
    }

    CPU_DI();
    if (zbus_tty_match_owner(tty_id, g_current_task) == 0u) {
        CPU_EI();
        return 0u;
    }

    {
        uint8_t pending = g_zbus_tx_count[tty_id];
        CPU_EI();
        return pending;
    }
}

uint8_t zbus_tty_tx_pending_current(void)
{
    uint8_t tty_id;

    if (zbus_tty_get_current(&tty_id) == 0u) {
        return 0u;
    }

    return zbus_tty_tx_pending(tty_id);
}

void zbus_stats_snapshot(zbus_stats_t *out)
{
    if (out == (zbus_stats_t *)0) {
        return;
    }

    CPU_DI();
    zbus_stats_snapshot_raw(out);
    CPU_EI();
}

void zbus_stats_reset(void)
{
    CPU_DI();
    zbus_stats_clear();
    CPU_EI();
    zlink_diag_reset();
}

void zbus_tick(void)
{
    uint8_t tx_remaining = (uint8_t)ZBUS_TX_CHUNK;
    uint8_t tx_start;
    uint8_t tx_scan;
    uint8_t tx_found;
    uint8_t tty_id;
    uint8_t len;
    uint8_t frame_index;
    uint8_t rx_frames_remaining;
    zlink_data_frame_t rx_frame;

    while (tx_remaining > 0u) {
        tx_found = 0u;
        tx_start = g_zbus_tx_rr_cursor;
        for (tx_scan = 0u; tx_scan < ZBUS_MAX_TTY; ++tx_scan) {
            tty_id = zbus_rr_tty(tx_start, tx_scan);

            if (zbus_tty_is_attached(tty_id) == 0u) {
                continue;
            }

            if (g_zbus_tx_count[tty_id] == 0u) {
                continue;
            }

            frame_index = g_zbus_tx_tail[tty_id];
            len = g_zbus_tx_queue[tty_id][frame_index].len;
            if (len > ZBUS_FRAME_MAX_LEN) {
                len = ZBUS_FRAME_MAX_LEN;
            }

            if (zlink_send_data(tty_id, &g_zbus_tx_queue[tty_id][frame_index].data[0], len) == 0u) {
                COUNTER_INC16_SAT(&g_zbus_stat_tx_drop);
            }

            zbus_tx_pop_frame(tty_id);
            tx_found = 1u;
            tx_remaining--;
            break;
        }
        if (tx_found == 0u) {
            break;
        }
    }

    rx_frames_remaining = (uint8_t)ZBUS_RX_CHUNK;
    while (rx_frames_remaining > 0u) {
        if (zlink_poll_once(&rx_frame) == 0u) {
            break;
        }

        tty_id = rx_frame.tty;
        if (tty_id == (uint8_t)ZBUS_KERNEL_TTY_ID) {
            zbus_kernel_handle_control(&rx_frame);
        } else if ((tty_id < ZBUS_MAX_TTY)
                   && (zbus_tty_is_attached(tty_id) != 0u)
                   && (g_zbus_tty_table[tty_id].rx_polling_enabled != 0u)) {
            zbus_rx_enqueue_payload(tty_id, &rx_frame);
        }

        rx_frames_remaining--;
    }

}

uint8_t zbus_write(const uint8_t *data, uint8_t len)
{
    uint8_t tty_id = zbus_legacy_get_or_attach();
    if (tty_id == (uint8_t)ZBUS_INVALID_TTY_ID) {
        return 0u;
    }
    return zbus_write_tty(tty_id, data, len);
}

uint8_t zbus_read(uint8_t *out, uint8_t maxlen)
{
    uint8_t tty_id = zbus_legacy_get_or_attach();
    if (tty_id == (uint8_t)ZBUS_INVALID_TTY_ID) {
        return 0u;
    }
    return zbus_read_tty(tty_id, out, maxlen);
}
