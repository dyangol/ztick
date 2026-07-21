#include <stdint.h>

#include "../common/common.h"
#include "../bootstrap/rtos.h"
#include "../drivers/zbus.h"
#include "../lib/ipc_demo.h"
#include "xsh.h"
#include "xsh_cmd_emit.h"
#include "xsh_cmd_report.h"

#pragma codeseg CODE

const uint8_t g_xsh_cmd_name_tasks[] = "tasks";
const uint8_t g_xsh_cmd_usage_tasks[] = "usage: tasks [task_id]";
const uint8_t g_xsh_cmd_name_heap[] = "heap";
const uint8_t g_xsh_cmd_usage_heap[] = "usage: heap [task_id]";
const uint8_t g_xsh_cmd_name_stack[] = "stack";
const uint8_t g_xsh_cmd_usage_stack[] = "usage: stack [task_id]";
const uint8_t g_xsh_cmd_name_stats[] = "stats";

static void xsh_cmd_emit_tasks_header(xsh_t *sh)
{
    xsh_cmd_emit_begin(sh);
    xsh_cmd_emit_cstr_padded((const uint8_t *)"slot", 5u);
    xsh_cmd_emit_cstr_padded((const uint8_t *)"tty", 4u);
    xsh_cmd_emit_cstr_padded((const uint8_t *)"w", 2u);
    xsh_cmd_emit_cstr((const uint8_t *)"name");
    xsh_cmd_emit_finish();
}

static uint8_t xsh_cmd_emit_task_name_line(xsh_t *sh, uint8_t slot)
{
    uint8_t name_len = xsh_cmd_task_name_len(slot);

    xsh_cmd_emit_begin(sh);
    xsh_cmd_emit_u8_dec_padded(slot, 5u);
    xsh_cmd_emit_tty_padded(slot, 4u);
    xsh_cmd_emit_u8_dec_padded(g_tasks[slot].weight, 2u);
    xsh_cmd_emit_name(g_tasks[slot].name, name_len);
    xsh_cmd_emit_finish();
    return 1u;
}

static void xsh_cmd_emit_tasks_summary(xsh_t *sh, uint8_t active)
{
    xsh_cmd_emit_begin(sh);
    xsh_cmd_emit_cstr((const uint8_t *)"tasks active=");
    xsh_cmd_emit_u8_dec(active);
    xsh_cmd_emit_finish();
}

uint8_t cmd_tasks(xsh_t *sh, uint8_t argc, uint8_t *argv[])
{
    uint8_t task_filter = 0u;
    uint8_t use_filter = 0u;
    uint8_t slot;

    if (argc == 2u) {
        if (xsh_parse_u8(argv[1], &task_filter) == 0u) {
            xsh_write_cstr(sh, g_xsh_cmd_usage_tasks);
            xsh_newline(sh);
            return 0u;
        }
        use_filter = 1u;
    } else if (argc > 2u) {
        xsh_write_cstr(sh, g_xsh_cmd_usage_tasks);
        xsh_newline(sh);
        return 0u;
    }

    if (use_filter == 0u) {
        uint8_t active = 0u;

        for (slot = 0u; slot < MAX_TASKS; ++slot) {
            if (g_tasks[slot].state != TASK_UNUSED) {
                active++;
            }
        }

        xsh_cmd_emit_tasks_summary(sh, active);
        xsh_cmd_emit_tasks_header(sh);

        for (slot = 0u; slot < MAX_TASKS; ++slot) {
            if (g_tasks[slot].state == TASK_UNUSED) {
                continue;
            }
            xsh_cmd_emit_task_name_line(sh, slot);
        }

        return 1u;
    }

    if (task_filter >= MAX_TASKS) {
        xsh_write_cstr(sh, g_xsh_cmd_txt_bad_task);
        xsh_newline(sh);
        return 0u;
    }

    if (g_tasks[task_filter].state == TASK_UNUSED) {
        xsh_write_cstr(sh, g_xsh_cmd_txt_bad_task);
        xsh_newline(sh);
        return 0u;
    }

    for (slot = 0u; slot < MAX_TASKS; ++slot) {
        uint8_t state = g_tasks[slot].state;
        uint8_t name_len = xsh_cmd_task_name_len(slot);

        if ((use_filter != 0u) && (slot != task_filter)) {
            continue;
        }

        if (state == TASK_UNUSED) {
            continue;
        }

        xsh_cmd_emit_begin(sh);
        xsh_cmd_emit_cstr((const uint8_t *)"task ");
        xsh_cmd_emit_u8_dec(slot);
        xsh_cmd_emit_cstr((const uint8_t *)" state=");
        xsh_cmd_emit_cstr(xsh_cmd_task_state_name(state));
        xsh_cmd_emit_tty(slot);
        xsh_cmd_emit_cstr((const uint8_t *)" w=");
        xsh_cmd_emit_u8_dec(g_tasks[slot].weight);
        xsh_cmd_emit_cstr((const uint8_t *)" b=");
        xsh_cmd_emit_u8_dec(g_tasks[slot].budget);
        xsh_cmd_emit_cstr((const uint8_t *)" sp=0x");
        xsh_cmd_emit_hex16(g_tasks[slot].sp);
        xsh_cmd_emit_cstr((const uint8_t *)" name=");
        xsh_cmd_emit_name(g_tasks[slot].name, name_len);
        xsh_cmd_emit_finish();
    }

    return 1u;
}

static void xsh_cmd_emit_heap_one(xsh_t *sh, uint8_t slot)
{
    heap_stats_t heap_stats;

    if (rtos_heap_stats(slot, &heap_stats) == 0u) {
        xsh_write_cstr(sh, g_xsh_cmd_txt_bad_task);
        xsh_newline(sh);
        return;
    }

    xsh_write_cstr(sh, (const uint8_t *)"heap ");
    xsh_write_u8_dec(sh, slot);
    xsh_write_cstr(sh, (const uint8_t *)" free=");
    xsh_write_u16_dec(sh, heap_stats.free_bytes);
    xsh_write_cstr(sh, (const uint8_t *)" free_blocks=");
    xsh_write_u8_dec(sh, heap_stats.free_blocks);
    xsh_write_cstr(sh, (const uint8_t *)" used_blocks=");
    xsh_write_u8_dec(sh, heap_stats.used_blocks);
    xsh_newline(sh);
}

static void xsh_cmd_emit_heap_header(xsh_t *sh)
{
    xsh_cmd_emit_begin(sh);
    xsh_cmd_emit_cstr_padded((const uint8_t *)"slot", 5u);
    xsh_cmd_emit_cstr_padded((const uint8_t *)"free", 5u);
    xsh_cmd_emit_cstr_padded((const uint8_t *)"free_blk", 9u);
    xsh_cmd_emit_cstr_padded((const uint8_t *)"used_blk", 9u);
    xsh_cmd_emit_cstr((const uint8_t *)"name");
    xsh_cmd_emit_finish();
}

/* Table row for the unfiltered ("all slots") listing -- a separate
 * function from xsh_cmd_emit_heap_one (kept as the plain key=value line
 * for `heap <task_id>`, which doesn't need a header to be readable). */
static void xsh_cmd_emit_heap_row(xsh_t *sh, uint8_t slot)
{
    heap_stats_t heap_stats;
    uint8_t name_len;

    if (rtos_heap_stats(slot, &heap_stats) == 0u) {
        return;
    }

    name_len = xsh_cmd_task_name_len(slot);

    xsh_cmd_emit_begin(sh);
    xsh_cmd_emit_u8_dec_padded(slot, 5u);
    xsh_cmd_emit_u16_dec_padded(heap_stats.free_bytes, 5u);
    xsh_cmd_emit_u8_dec_padded(heap_stats.free_blocks, 9u);
    xsh_cmd_emit_u8_dec_padded(heap_stats.used_blocks, 9u);
    xsh_cmd_emit_name(g_tasks[slot].name, name_len);
    xsh_cmd_emit_finish();
}

uint8_t cmd_heap(xsh_t *sh, uint8_t argc, uint8_t *argv[])
{
    if (argc == 1u) {
        uint8_t slot;
        xsh_cmd_emit_heap_header(sh);
        for (slot = 0u; slot < MAX_TASKS; ++slot) {
            if (g_tasks[slot].state != TASK_UNUSED) {
                xsh_cmd_emit_heap_row(sh, slot);
            }
        }
        return 1u;
    }

    if (argc == 2u) {
        uint8_t slot_id;
        if (xsh_parse_u8(argv[1], &slot_id) == 0u) {
            xsh_cmd_emit_usage(sh, g_xsh_cmd_usage_heap);
            return 0u;
        }
        xsh_cmd_emit_heap_one(sh, slot_id);
        return 1u;
    }

    xsh_cmd_emit_usage(sh, g_xsh_cmd_usage_heap);
    return 0u;
}

static void xsh_cmd_emit_stack_one(xsh_t *sh, uint8_t slot)
{
    uint16_t peak_used;
    uint16_t current_used;
    uint16_t stack_size;
    uint16_t free_peak;
    uint8_t name_len;

    if (rtos_stack_watermark(slot, &peak_used, &current_used, &stack_size) == 0u) {
        xsh_write_cstr(sh, g_xsh_cmd_txt_bad_task);
        xsh_newline(sh);
        return;
    }

    free_peak = (uint16_t)(stack_size - peak_used);
    name_len = xsh_cmd_task_name_len(slot);

    xsh_cmd_emit_begin(sh);
    xsh_cmd_emit_cstr((const uint8_t *)"stack ");
    xsh_cmd_emit_u8_dec(slot);
    xsh_cmd_emit_cstr((const uint8_t *)" state=");
    xsh_cmd_emit_cstr(xsh_cmd_task_state_name(g_tasks[slot].state));
    xsh_cmd_emit_cstr((const uint8_t *)" size=");
    xsh_cmd_emit_u16_dec(stack_size);
    xsh_cmd_emit_cstr((const uint8_t *)" peak=");
    xsh_cmd_emit_u16_dec(peak_used);
    xsh_cmd_emit_cstr((const uint8_t *)" free_peak=");
    xsh_cmd_emit_u16_dec(free_peak);
    xsh_cmd_emit_cstr((const uint8_t *)" current=");
    xsh_cmd_emit_u16_dec(current_used);
    xsh_cmd_emit_cstr((const uint8_t *)" name=");
    xsh_cmd_emit_name(g_tasks[slot].name, name_len);
    xsh_cmd_emit_finish();
}

static void xsh_cmd_emit_stack_header(xsh_t *sh)
{
    xsh_cmd_emit_begin(sh);
    xsh_cmd_emit_cstr_padded((const uint8_t *)"slot", 5u);
    xsh_cmd_emit_cstr_padded((const uint8_t *)"state", 12u);
    xsh_cmd_emit_cstr_padded((const uint8_t *)"size", 5u);
    xsh_cmd_emit_cstr_padded((const uint8_t *)"peak", 5u);
    xsh_cmd_emit_cstr_padded((const uint8_t *)"free_peak", 10u);
    xsh_cmd_emit_cstr_padded((const uint8_t *)"current", 8u);
    xsh_cmd_emit_cstr((const uint8_t *)"name");
    xsh_cmd_emit_finish();
}

/* Table row for the unfiltered ("all slots") listing -- a separate
 * function from xsh_cmd_emit_stack_one (kept as the plain key=value line
 * for `stack <task_id>`, which doesn't need a header to be readable). */
static void xsh_cmd_emit_stack_row(xsh_t *sh, uint8_t slot)
{
    uint16_t peak_used;
    uint16_t current_used;
    uint16_t stack_size;
    uint16_t free_peak;
    uint8_t name_len;

    if (rtos_stack_watermark(slot, &peak_used, &current_used, &stack_size) == 0u) {
        return;
    }

    free_peak = (uint16_t)(stack_size - peak_used);
    name_len = xsh_cmd_task_name_len(slot);

    xsh_cmd_emit_begin(sh);
    xsh_cmd_emit_u8_dec_padded(slot, 5u);
    xsh_cmd_emit_cstr_padded(xsh_cmd_task_state_name(g_tasks[slot].state), 12u);
    xsh_cmd_emit_u16_dec_padded(stack_size, 5u);
    xsh_cmd_emit_u16_dec_padded(peak_used, 5u);
    xsh_cmd_emit_u16_dec_padded(free_peak, 10u);
    xsh_cmd_emit_u16_dec_padded(current_used, 8u);
    xsh_cmd_emit_name(g_tasks[slot].name, name_len);
    xsh_cmd_emit_finish();
}

uint8_t cmd_stack(xsh_t *sh, uint8_t argc, uint8_t *argv[])
{
    if (argc == 1u) {
        uint8_t slot;
        xsh_cmd_emit_stack_header(sh);
        for (slot = 0u; slot < MAX_TASKS; ++slot) {
            if (g_tasks[slot].state != TASK_UNUSED) {
                xsh_cmd_emit_stack_row(sh, slot);
            }
        }
        return 1u;
    }

    if (argc == 2u) {
        uint8_t slot_id;
        if (xsh_parse_u8(argv[1], &slot_id) == 0u) {
            xsh_cmd_emit_usage(sh, g_xsh_cmd_usage_stack);
            return 0u;
        }
        xsh_cmd_emit_stack_one(sh, slot_id);
        return 1u;
    }

    xsh_cmd_emit_usage(sh, g_xsh_cmd_usage_stack);
    return 0u;
}

uint8_t cmd_stats(xsh_t *sh, uint8_t argc, uint8_t *argv[])
{
    zbus_stats_t stats;
    uint8_t q_used;
    uint8_t q_cap;

    UNUSED(argc);
    UNUSED(argv);

    zbus_stats_snapshot(&stats);

    xsh_cmd_emit_begin(sh);
    xsh_cmd_emit_cstr((const uint8_t *)"zbus tx_drop=");
    xsh_cmd_emit_u16_dec(stats.tx_drop);
    xsh_cmd_emit_cstr((const uint8_t *)" rx_overflow=");
    xsh_cmd_emit_u16_dec(stats.rx_overflow);
    xsh_cmd_emit_cstr((const uint8_t *)" attach_fail=");
    xsh_cmd_emit_u16_dec(stats.attach_fail);
    xsh_cmd_emit_finish();

    xsh_cmd_emit_begin(sh);
    xsh_cmd_emit_cstr((const uint8_t *)"zlink ok=");
    xsh_cmd_emit_u16_dec(stats.zlink_rx_frames_ok);
    xsh_cmd_emit_cstr((const uint8_t *)" crc=");
    xsh_cmd_emit_u16_dec(stats.zlink_rx_crc_err);
    xsh_cmd_emit_cstr((const uint8_t *)" dup=");
    xsh_cmd_emit_u16_dec(stats.zlink_rx_dup);
    xsh_cmd_emit_cstr((const uint8_t *)" type=");
    xsh_cmd_emit_u16_dec(stats.zlink_rx_type_err);
    xsh_cmd_emit_cstr((const uint8_t *)" len=");
    xsh_cmd_emit_u16_dec(stats.zlink_rx_len_err);
    xsh_cmd_emit_finish();

    q_used = ipc_demo_queue_used();
    q_cap = ipc_demo_queue_capacity();
    xsh_write_cstr(sh, (const uint8_t *)"ipc q_used=");
    xsh_write_u8_dec(sh, q_used);
    xsh_write_cstr(sh, (const uint8_t *)" q_cap=");
    xsh_write_u8_dec(sh, q_cap);
    xsh_newline(sh);

    return 1u;
}

