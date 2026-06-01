#include <stdint.h>

#include "../common/common.h"
#include "../bootstrap/rtos.h"
#include "../drivers/zbus.h"
#include "target_autostart.h"
#include "../lib/ipc_demo.h"
#include "../lib/task.h"
#include "xsh.h"
#include "xsh_cmd.h"

#pragma codeseg CODE

#define XSH_CMD_BOOTCFG_TTY 0u
#define XSH_CMD_LINE_CAP ((uint8_t)(ZBUS_FRAME_MAX_LEN - 2u))

#define XSH_CMD_STR2(x) #x
#define XSH_CMD_STR(x) XSH_CMD_STR2(x)

static const uint8_t g_boot_cfg_line_0[] = "cfg max_tasks=" XSH_CMD_STR(MAX_TASKS) "\n";
static const uint8_t g_boot_cfg_line_1[] = "cfg task_heap=" XSH_CMD_STR(TASK_HEAP_SIZE) "\n";
static const uint8_t g_boot_cfg_line_2[] = "cfg zbus tty=" XSH_CMD_STR(ZBUS_MAX_TTY) " rx=" XSH_CMD_STR(ZBUS_BUFFER_SIZE) " txq=" XSH_CMD_STR(ZBUS_TX_QUEUE_SIZE) "\n";
static const uint8_t g_boot_cfg_line_3[] = "cfg ipc sem_queue=1\n";
static const uint8_t g_boot_cfg_line_4[] = "cfg autostart=" TARGET_AUTOSTART_RAW "\n";
static const uint8_t g_xsh_cmd_banner[] = "Z-Tick xsh\n";
static const uint8_t g_xsh_cmd_prompt[] = "ztick> ";
static const uint8_t g_xsh_cmd_state_unused[] = "unused";
static const uint8_t g_xsh_cmd_state_ready[] = "ready";
static const uint8_t g_xsh_cmd_state_wait_sem[] = "wait_sem";
static const uint8_t g_xsh_cmd_state_wait_q_send[] = "wait_q_send";
static const uint8_t g_xsh_cmd_state_wait_q_recv[] = "wait_q_recv";
static const uint8_t g_xsh_cmd_txt_unknown[] = "unknown command";
static const uint8_t g_xsh_cmd_txt_bad_task[] = "undefined task id";
static const uint8_t g_xsh_cmd_usage_help[] = "help";
static const uint8_t g_xsh_cmd_usage_cfg[] = "cfg";
static const uint8_t g_xsh_cmd_usage_tasks[] = "usage: tasks [task_id]";
static const uint8_t g_xsh_cmd_usage_start[] = "usage: start <task_name> [weight|w=<1..3>] [t=<0..9>|tty=<0..9>|t=auto] [args...]";
static const uint8_t g_xsh_cmd_usage_stop[] = "usage: stop <task_name>";
static const uint8_t g_xsh_cmd_usage_weight[] = "usage: weight <task_id> <1..3>";
static const uint8_t g_xsh_cmd_usage_heap[] = "usage: heap [task_id]";
static const uint8_t g_xsh_cmd_usage_stack[] = "usage: stack [task_id]";
static const uint8_t g_xsh_cmd_usage_stats[] = "stats";
static const uint8_t g_xsh_cmd_name_help[] = "help";
static const uint8_t g_xsh_cmd_name_cfg[] = "cfg";
static const uint8_t g_xsh_cmd_name_tasks[] = "tasks";
static const uint8_t g_xsh_cmd_name_start[] = "start";
static const uint8_t g_xsh_cmd_name_stop[] = "stop";
static const uint8_t g_xsh_cmd_name_weight[] = "weight";
static const uint8_t g_xsh_cmd_name_heap[] = "heap";
static const uint8_t g_xsh_cmd_name_stack[] = "stack";
static const uint8_t g_xsh_cmd_name_stats[] = "stats";

static void xsh_cmd_write_task_tty(xsh_t *sh, uint8_t slot)
{
    uint8_t tty_id;

    xsh_write_cstr(sh, (const uint8_t *)" tty=");
    if (zbus_tty_get_for_task(slot, &tty_id) != 0u) {
        xsh_write_u8_dec(sh, tty_id);
    } else {
        xsh_write_cstr(sh, (const uint8_t *)"-");
    }
}

static const uint8_t *xsh_cmd_task_state_name(uint8_t state)
{
    if (state == TASK_READY) {
        return g_xsh_cmd_state_ready;
    }
    if (state == TASK_UNUSED) {
        return g_xsh_cmd_state_unused;
    }
    if (state == TASK_WAIT_SEM) {
        return g_xsh_cmd_state_wait_sem;
    }
    if (state == TASK_WAIT_Q_SEND) {
        return g_xsh_cmd_state_wait_q_send;
    }
    if (state == TASK_WAIT_Q_RECV) {
        return g_xsh_cmd_state_wait_q_recv;
    }
    return (const uint8_t *)"other";
}

static uint8_t line_append_u16_hex(uint8_t *buf, uint8_t max, uint8_t *io_len, uint16_t value)
{
    uint8_t i;

    if ((buf == (uint8_t *)0) || (io_len == (uint8_t *)0)) {
        return 0u;
    }

    for (i = 0u; i < 4u; ++i) {
        uint8_t nibble = (uint8_t)((value >> (uint8_t)((3u - i) * 4u)) & 0x0Fu);
        uint8_t digit = (nibble < 10u) ? (uint8_t)('0' + nibble) : (uint8_t)('A' + (nibble - 10u));
        if (*io_len >= max) {
            return 0u;
        }
        buf[*io_len] = digit;
        *io_len = (uint8_t)(*io_len + 1u);
    }

    return 1u;
}

static void xsh_cmd_emit_buffer_line(xsh_t *sh, uint8_t *line, uint8_t len)
{
    line[len] = (uint8_t)'\r';
    line[(uint8_t)(len + 1u)] = (uint8_t)'\n';
    xsh_write_bytes(sh, line, (uint8_t)(len + 2u));
}

static void xsh_cmd_emit_line(xsh_t *sh, const uint8_t *text)
{
    uint8_t line[ZBUS_FRAME_MAX_LEN];
    uint8_t len = 0u;
    uint8_t ok;

    ok = xsh_line_append_cstr(line, (uint8_t)XSH_CMD_LINE_CAP, &len, text);
    if (ok != 0u) {
        xsh_cmd_emit_buffer_line(sh, line, len);
    } else {
        xsh_write_cstr(sh, text);
        xsh_newline(sh);
    }
}

static void xsh_cmd_emit_usage(xsh_t *sh, const uint8_t *usage)
{
    xsh_write_cstr(sh, usage);
    xsh_newline(sh);
}

static void xsh_cmd_emit_task_name_choices(xsh_t *sh)
{
    uint8_t i;
    uint8_t count = task_registry_count();
    uint8_t emitted = 0u;

    for (i = 0u; i < count; ++i) {
        const task_spec_t *spec = task_registry_get(i);

        if (spec == (const task_spec_t *)0) {
            continue;
        }
        if (emitted != 0u) {
            xsh_write_bytes(sh, (const uint8_t *)"|", 1u);
        }
        xsh_write_cstr(sh, spec->name);
        emitted = 1u;
    }
}

static void xsh_cmd_emit_start_usage(xsh_t *sh)
{
    xsh_write_cstr(sh, (const uint8_t *)"usage: start ");
    xsh_cmd_emit_task_name_choices(sh);
    xsh_write_cstr(sh, (const uint8_t *)" [weight|w=<1..3>] [t=<0..9>|tty=<0..9>|t=auto] [args...]");
    xsh_newline(sh);
}

static void xsh_cmd_emit_start_usage_for_spec(xsh_t *sh, const task_spec_t *spec)
{
    if (sh == (xsh_t *)0) {
        return;
    }

    if ((spec == (const task_spec_t *)0) || (spec->name == (const uint8_t *)0)) {
        xsh_cmd_emit_start_usage(sh);
        return;
    }

    xsh_write_cstr(sh, (const uint8_t *)"usage: start ");
    xsh_write_cstr(sh, spec->name);
    xsh_write_cstr(sh, (const uint8_t *)" [weight|w=<1..3>] [t=<0..9>|tty=<0..9>|t=auto]");
    if (spec->start_args_usage != (const uint8_t *)0) {
        xsh_write_cstr(sh, (const uint8_t *)" [");
        xsh_write_cstr(sh, spec->start_args_usage);
        xsh_write_cstr(sh, (const uint8_t *)"]");
    } else {
        xsh_write_cstr(sh, (const uint8_t *)" [args...]");
    }
    xsh_newline(sh);
}

static void xsh_cmd_emit_stop_usage(xsh_t *sh)
{
    xsh_write_cstr(sh, (const uint8_t *)"usage: stop ");
    xsh_cmd_emit_task_name_choices(sh);
    xsh_newline(sh);
}

static uint8_t cmd_help(xsh_t *sh, uint8_t argc, uint8_t *argv[])
{
    UNUSED(argc);
    UNUSED(argv);
    xsh_cmd_emit_line(sh, (const uint8_t *)"commands: help cfg tasks start stop weight heap stack stats");
    return 1u;
}

static uint8_t cmd_cfg(xsh_t *sh, uint8_t argc, uint8_t *argv[])
{
    UNUSED(argc);
    UNUSED(argv);
    xsh_write_bytes(sh, g_boot_cfg_line_0, (uint8_t)(sizeof(g_boot_cfg_line_0) - 1u));
    xsh_write_bytes(sh, g_boot_cfg_line_1, (uint8_t)(sizeof(g_boot_cfg_line_1) - 1u));
    xsh_write_bytes(sh, g_boot_cfg_line_2, (uint8_t)(sizeof(g_boot_cfg_line_2) - 1u));
    xsh_write_bytes(sh, g_boot_cfg_line_3, (uint8_t)(sizeof(g_boot_cfg_line_3) - 1u));
    xsh_write_bytes(sh, g_boot_cfg_line_4, (uint8_t)(sizeof(g_boot_cfg_line_4) - 1u));
    return 1u;
}

static uint8_t xsh_cmd_name_len(const uint8_t *name)
{
    uint8_t len = 0u;
    if (name == (const uint8_t *)0) {
        return 0u;
    }
    while ((len < TASK_NAME_MAX) && (name[len] != 0u)) {
        len++;
    }
    return len;
}

static uint8_t xsh_cmd_find_task_by_name(const uint8_t *name, uint8_t *out_slot)
{
    uint8_t slot;
    uint8_t name_len = xsh_cmd_name_len(name);

    if ((name_len == 0u) || (out_slot == (uint8_t *)0)) {
        return 0u;
    }

    for (slot = 0u; slot < MAX_TASKS; ++slot) {
        uint8_t i;

        if (g_tasks[slot].state == TASK_UNUSED) {
            continue;
        }
        if (g_tasks[slot].name_len != name_len) {
            continue;
        }

        for (i = 0u; i < name_len; ++i) {
            if (g_tasks[slot].name[i] != name[i]) {
                break;
            }
        }
        if (i == name_len) {
            *out_slot = slot;
            return 1u;
        }
    }

    return 0u;
}

static uint8_t xsh_cmd_parse_weight(const uint8_t *text, uint8_t *out_weight)
{
    uint8_t weight;

    if ((out_weight == (uint8_t *)0) || (xsh_parse_u8(text, &weight) == 0u)) {
        return 0u;
    }
    if ((weight < TASK_WEIGHT_MIN) || (weight > TASK_WEIGHT_MAX)) {
        return 0u;
    }

    *out_weight = weight;
    return 1u;
}

static uint8_t xsh_cmd_parse_weight_assignment(const uint8_t *text, uint8_t *out_weight)
{
    if ((text == (const uint8_t *)0) || (out_weight == (uint8_t *)0)) {
        return 0u;
    }

    if ((text[0] != (uint8_t)'w') || (text[1] != (uint8_t)'=')) {
        return 0u;
    }

    return xsh_cmd_parse_weight(&text[2], out_weight);
}

static uint8_t xsh_cmd_parse_tty_value(const uint8_t *text, uint8_t *out_tty)
{
    uint8_t tty_id;

    if ((text == (const uint8_t *)0) || (out_tty == (uint8_t *)0)) {
        return 0u;
    }

    if ((text[0] == (uint8_t)'a') && (text[1] == (uint8_t)'u') && (text[2] == (uint8_t)'t')
        && (text[3] == (uint8_t)'o') && (text[4] == 0u)) {
        *out_tty = (uint8_t)TASK_TTY_AUTO;
        return 1u;
    }

    if (xsh_parse_u8(text, &tty_id) == 0u) {
        return 0u;
    }
    if (tty_id >= ZBUS_MAX_TTY) {
        return 0u;
    }

    *out_tty = tty_id;
    return 1u;
}

static uint8_t xsh_cmd_parse_tty_assignment(const uint8_t *text, uint8_t *out_tty)
{
    if ((text == (const uint8_t *)0) || (out_tty == (uint8_t *)0)) {
        return 0u;
    }

    if ((text[0] == (uint8_t)'t') && (text[1] == (uint8_t)'=')) {
        return xsh_cmd_parse_tty_value(&text[2], out_tty);
    }
    if ((text[0] == (uint8_t)'t') && (text[1] == (uint8_t)'t') && (text[2] == (uint8_t)'y') && (text[3] == (uint8_t)'=')) {
        return xsh_cmd_parse_tty_value(&text[4], out_tty);
    }

    return 0u;
}

static uint8_t xsh_cmd_start_task_spec(xsh_t *sh, const task_spec_t *spec, uint8_t weight, uint8_t requested_tty)
{
    uint8_t slot;
    task_spec_t start_spec;

    if (spec == (const task_spec_t *)0) {
        return 0u;
    }

    if (xsh_cmd_find_task_by_name(spec->name, &slot) != 0u) {
        xsh_write_cstr(sh, (const uint8_t *)"already running task_id=");
        xsh_write_u8_dec(sh, slot);
        xsh_newline(sh);
        return 1u;
    }

    start_spec = *spec;
    start_spec.requested_tty = requested_tty;

    if (rtos_task_register_spec(&start_spec, weight, &slot) == 0u) {
        xsh_write_cstr(sh, (const uint8_t *)"start failed");
        xsh_newline(sh);
        return 0u;
    }

    xsh_write_cstr(sh, (const uint8_t *)"started ");
    xsh_write_cstr(sh, spec->name);
    xsh_write_cstr(sh, (const uint8_t *)" task_id=");
    xsh_write_u8_dec(sh, slot);
    xsh_cmd_write_task_tty(sh, slot);
    xsh_write_cstr(sh, (const uint8_t *)" weight=");
    xsh_write_u8_dec(sh, weight);
    xsh_newline(sh);
    return 1u;
}

static uint8_t cmd_start(xsh_t *sh, uint8_t argc, uint8_t *argv[])
{
    const task_spec_t *spec;
    uint8_t weight;
    uint8_t requested_tty;
    uint8_t idx;
    uint8_t has_weight = 0u;
    uint8_t has_tty = 0u;
    uint8_t task_argv[XSH_ARGV_MAX];
    uint8_t task_argc = 0u;
    uint8_t running_slot;

    if ((argc < 2u) || (argc > XSH_ARGV_MAX)) {
        xsh_cmd_emit_start_usage(sh);
        return 0u;
    }

    spec = task_registry_find(argv[1]);
    if (spec == (const task_spec_t *)0) {
        xsh_cmd_emit_start_usage(sh);
        return 0u;
    }

    weight = spec->default_weight;
    requested_tty = spec->requested_tty;

    for (idx = 2u; idx < argc; ++idx) {
        uint8_t parsed_weight;
        uint8_t parsed_tty;

        if ((idx == 2u) && (xsh_cmd_parse_weight(argv[idx], &parsed_weight) != 0u) && (has_weight == 0u)) {
            weight = parsed_weight;
            has_weight = 1u;
            continue;
        }

        if ((xsh_cmd_parse_weight_assignment(argv[idx], &parsed_weight) != 0u) && (has_weight == 0u)) {
            weight = parsed_weight;
            has_weight = 1u;
            continue;
        }
        if (xsh_cmd_parse_tty_assignment(argv[idx], &parsed_tty) != 0u) {
            if (has_tty != 0u) {
                xsh_cmd_emit_start_usage_for_spec(sh, spec);
                return 0u;
            }
            requested_tty = parsed_tty;
            has_tty = 1u;
            continue;
        }

        if (task_argc < (uint8_t)ARRAY_LEN(task_argv)) {
            task_argv[task_argc] = idx;
            task_argc++;
        } else {
            xsh_cmd_emit_start_usage_for_spec(sh, spec);
            return 0u;
        }
    }

    if (xsh_cmd_find_task_by_name(spec->name, &running_slot) != 0u) {
        return xsh_cmd_start_task_spec(sh, spec, weight, requested_tty);
    }

    if (task_argc != 0u) {
        uint8_t arg_idx;
        uint8_t *task_argv_ptrs[XSH_ARGV_MAX];

        for (arg_idx = 0u; arg_idx < task_argc; ++arg_idx) {
            task_argv_ptrs[arg_idx] = argv[task_argv[arg_idx]];
        }

        if (task_registry_start_configure(spec, task_argc, task_argv_ptrs) == 0u) {
            task_registry_start_reset(spec);
            xsh_cmd_emit_start_usage_for_spec(sh, spec);
            return 0u;
        }
    } else if (task_registry_start_configure(spec, 0u, (uint8_t **)0) == 0u) {
        task_registry_start_reset(spec);
        xsh_cmd_emit_start_usage_for_spec(sh, spec);
        return 0u;
    }

    if (xsh_cmd_start_task_spec(sh, spec, weight, requested_tty) == 0u) {
        task_registry_start_reset(spec);
        return 0u;
    }

    return 1u;
}

static uint8_t xsh_cmd_stop_task_spec(xsh_t *sh, const task_spec_t *spec)
{
    uint8_t slot;

    if (spec == (const task_spec_t *)0) {
        return 0u;
    }

    if (xsh_cmd_find_task_by_name(spec->name, &slot) == 0u) {
        xsh_write_cstr(sh, (const uint8_t *)"not running");
        xsh_newline(sh);
        return 0u;
    }

    if (rtos_task_request_stop(slot) == 0u) {
        xsh_write_cstr(sh, (const uint8_t *)"stop failed");
        xsh_newline(sh);
        return 0u;
    }

    xsh_write_cstr(sh, (const uint8_t *)"stop requested ");
    xsh_write_cstr(sh, spec->name);
    xsh_write_cstr(sh, (const uint8_t *)" task_id=");
    xsh_write_u8_dec(sh, slot);
    xsh_newline(sh);
    return 1u;
}

static uint8_t cmd_stop(xsh_t *sh, uint8_t argc, uint8_t *argv[])
{
    const task_spec_t *spec;

    if (argc != 2u) {
        xsh_cmd_emit_stop_usage(sh);
        return 0u;
    }

    spec = task_registry_find(argv[1]);
    if (spec != (const task_spec_t *)0) {
        return xsh_cmd_stop_task_spec(sh, spec);
    }

    xsh_cmd_emit_stop_usage(sh);
    return 0u;
}

static uint8_t cmd_weight(xsh_t *sh, uint8_t argc, uint8_t *argv[])
{
    uint8_t task_id;
    uint8_t weight;

    if (argc != 3u) {
        xsh_cmd_emit_usage(sh, g_xsh_cmd_usage_weight);
        return 0u;
    }

    if ((xsh_parse_u8(argv[1], &task_id) == 0u) || (xsh_cmd_parse_weight(argv[2], &weight) == 0u)) {
        xsh_cmd_emit_usage(sh, g_xsh_cmd_usage_weight);
        return 0u;
    }

    if (rtos_task_set_weight(task_id, weight) == 0u) {
        xsh_write_cstr(sh, g_xsh_cmd_txt_bad_task);
        xsh_newline(sh);
        return 0u;
    }

    xsh_write_cstr(sh, (const uint8_t *)"weight task_id=");
    xsh_write_u8_dec(sh, task_id);
    xsh_write_cstr(sh, (const uint8_t *)" set=");
    xsh_write_u8_dec(sh, weight);
    xsh_newline(sh);
    return 1u;
}

static uint8_t xsh_cmd_task_name_len(uint8_t slot)
{
    uint8_t name_len = g_tasks[slot].name_len;
    if (name_len > TASK_NAME_MAX) {
        name_len = TASK_NAME_MAX;
    }
    return name_len;
}

static uint8_t xsh_cmd_emit_task_name_line(xsh_t *sh, uint8_t slot)
{
    uint8_t i;
    uint8_t line[ZBUS_FRAME_MAX_LEN];
    uint8_t len = 0u;
    uint8_t ok = 1u;
    uint8_t name_len = xsh_cmd_task_name_len(slot);

    ok = xsh_line_append_cstr(line, (uint8_t)XSH_CMD_LINE_CAP, &len, (const uint8_t *)"task ");
    if (ok != 0u) {
        ok = xsh_line_append_u8_dec(line, (uint8_t)XSH_CMD_LINE_CAP, &len, slot);
    }
    if (ok != 0u) {
        ok = xsh_line_append_cstr(line, (uint8_t)XSH_CMD_LINE_CAP, &len, (const uint8_t *)" name=");
    }
    if (ok != 0u) {
        if (name_len == 0u) {
            ok = xsh_line_append_cstr(line, (uint8_t)XSH_CMD_LINE_CAP, &len, (const uint8_t *)"-");
        } else {
            for (i = 0u; i < name_len; ++i) {
                if (len >= (uint8_t)XSH_CMD_LINE_CAP) {
                    ok = 0u;
                    break;
                }
                line[len] = g_tasks[slot].name[i];
                len++;
            }
        }
    }

    if (ok != 0u) {
        xsh_cmd_emit_buffer_line(sh, line, len);
        return 1u;
    }

    xsh_write_cstr(sh, (const uint8_t *)"task ");
    xsh_write_u8_dec(sh, slot);
    xsh_cmd_write_task_tty(sh, slot);
    xsh_write_cstr(sh, (const uint8_t *)" name=");
    if (name_len == 0u) {
        xsh_write_cstr(sh, (const uint8_t *)"-");
    } else {
        for (i = 0u; i < name_len; ++i) {
            xsh_write_bytes(sh, &g_tasks[slot].name[i], 1u);
        }
    }
    xsh_newline(sh);
    return 1u;
}

static uint8_t cmd_tasks(xsh_t *sh, uint8_t argc, uint8_t *argv[])
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
        uint8_t line[ZBUS_FRAME_MAX_LEN];
        uint8_t ok;
        uint8_t active = 0u;
        uint8_t len = 0u;

        for (slot = 0u; slot < MAX_TASKS; ++slot) {
            if (g_tasks[slot].state != TASK_UNUSED) {
                active++;
            }
        }

        ok = xsh_line_append_cstr(line, (uint8_t)XSH_CMD_LINE_CAP, &len, (const uint8_t *)"tasks active=");
        if (ok != 0u) {
            ok = xsh_line_append_u8_dec(line, (uint8_t)XSH_CMD_LINE_CAP, &len, active);
        }
        if (ok != 0u) {
            xsh_cmd_emit_buffer_line(sh, line, len);
        } else {
            xsh_cmd_emit_line(sh, (const uint8_t *)"tasks");
        }

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
        uint8_t i;
        uint8_t line[ZBUS_FRAME_MAX_LEN];
        uint8_t len = 0u;
        uint8_t ok = 1u;
        uint8_t state = g_tasks[slot].state;
        uint8_t name_len = xsh_cmd_task_name_len(slot);

        if ((use_filter != 0u) && (slot != task_filter)) {
            continue;
        }

        if (state == TASK_UNUSED) {
            continue;
        }

        ok = xsh_line_append_cstr(line, (uint8_t)XSH_CMD_LINE_CAP, &len, (const uint8_t *)"task ");
        if (ok != 0u) {
            ok = xsh_line_append_u8_dec(line, (uint8_t)XSH_CMD_LINE_CAP, &len, slot);
        }
        if (ok != 0u) {
            ok = xsh_line_append_cstr(line, (uint8_t)XSH_CMD_LINE_CAP, &len, (const uint8_t *)" state=");
        }
        if (ok != 0u) {
            ok = xsh_line_append_cstr(line, (uint8_t)XSH_CMD_LINE_CAP, &len, xsh_cmd_task_state_name(state));
        }
        if (ok != 0u) {
            ok = xsh_line_append_cstr(line, (uint8_t)XSH_CMD_LINE_CAP, &len, (const uint8_t *)" tty=");
        }
        if (ok != 0u) {
            uint8_t tty_id;
            if (zbus_tty_get_for_task(slot, &tty_id) != 0u) {
                ok = xsh_line_append_u8_dec(line, (uint8_t)XSH_CMD_LINE_CAP, &len, tty_id);
            } else {
                ok = xsh_line_append_cstr(line, (uint8_t)XSH_CMD_LINE_CAP, &len, (const uint8_t *)"-");
            }
        }
        if (ok != 0u) {
            ok = xsh_line_append_cstr(line, (uint8_t)XSH_CMD_LINE_CAP, &len, (const uint8_t *)" w=");
        }
        if (ok != 0u) {
            ok = xsh_line_append_u8_dec(line, (uint8_t)XSH_CMD_LINE_CAP, &len, g_tasks[slot].weight);
        }
        if (ok != 0u) {
            ok = xsh_line_append_cstr(line, (uint8_t)XSH_CMD_LINE_CAP, &len, (const uint8_t *)" b=");
        }
        if (ok != 0u) {
            ok = xsh_line_append_u8_dec(line, (uint8_t)XSH_CMD_LINE_CAP, &len, g_tasks[slot].budget);
        }
        if (ok != 0u) {
            ok = xsh_line_append_cstr(line, (uint8_t)XSH_CMD_LINE_CAP, &len, (const uint8_t *)" sp=0x");
        }
        if (ok != 0u) {
            ok = line_append_u16_hex(line, (uint8_t)XSH_CMD_LINE_CAP, &len, g_tasks[slot].sp);
        }
        if (ok != 0u) {
            ok = xsh_line_append_cstr(line, (uint8_t)XSH_CMD_LINE_CAP, &len, (const uint8_t *)" name=");
        }

        if (ok != 0u) {
            if (name_len == 0u) {
                ok = xsh_line_append_cstr(line, (uint8_t)XSH_CMD_LINE_CAP, &len, (const uint8_t *)"-");
            } else {
                for (i = 0u; i < name_len; ++i) {
                    if (len >= (uint8_t)XSH_CMD_LINE_CAP) {
                        ok = 0u;
                        break;
                    }
                    line[len] = g_tasks[slot].name[i];
                    len++;
                }
            }
        }

        if (ok != 0u) {
            xsh_cmd_emit_buffer_line(sh, line, len);
            continue;
        }

        xsh_write_cstr(sh, (const uint8_t *)"task ");
        xsh_write_u8_dec(sh, slot);
        xsh_write_cstr(sh, (const uint8_t *)" state=");
        xsh_write_cstr(sh, xsh_cmd_task_state_name(state));
        xsh_cmd_write_task_tty(sh, slot);
        xsh_write_cstr(sh, (const uint8_t *)" w=");
        xsh_write_u8_dec(sh, g_tasks[slot].weight);
        xsh_write_cstr(sh, (const uint8_t *)" b=");
        xsh_write_u8_dec(sh, g_tasks[slot].budget);
        xsh_write_cstr(sh, (const uint8_t *)" sp=0x");
        xsh_write_u16_hex(sh, g_tasks[slot].sp);
        xsh_write_cstr(sh, (const uint8_t *)" name=");

        if (name_len == 0u) {
            xsh_write_cstr(sh, (const uint8_t *)"-");
        } else {
            for (i = 0u; i < name_len; ++i) {
                xsh_write_bytes(sh, &g_tasks[slot].name[i], 1u);
            }
        }

        xsh_newline(sh);
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

static uint8_t cmd_heap(xsh_t *sh, uint8_t argc, uint8_t *argv[])
{
    if (argc == 1u) {
        uint8_t slot;
        for (slot = 0u; slot < MAX_TASKS; ++slot) {
            if (g_tasks[slot].state != TASK_UNUSED) {
                xsh_cmd_emit_heap_one(sh, slot);
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
    uint8_t line[ZBUS_FRAME_MAX_LEN];
    uint8_t len = 0u;
    uint16_t peak_used;
    uint16_t current_used;
    uint16_t stack_size;
    uint16_t free_peak;
    uint8_t ok;

    if (rtos_stack_watermark(slot, &peak_used, &current_used, &stack_size) == 0u) {
        xsh_write_cstr(sh, g_xsh_cmd_txt_bad_task);
        xsh_newline(sh);
        return;
    }

    free_peak = (uint16_t)(stack_size - peak_used);

    ok = xsh_line_append_cstr(line, (uint8_t)XSH_CMD_LINE_CAP, &len, (const uint8_t *)"stack ");
    if (ok != 0u) {
        ok = xsh_line_append_u8_dec(line, (uint8_t)XSH_CMD_LINE_CAP, &len, slot);
    }
    if (ok != 0u) {
        ok = xsh_line_append_cstr(line, (uint8_t)XSH_CMD_LINE_CAP, &len, (const uint8_t *)" size=");
    }
    if (ok != 0u) {
        ok = xsh_line_append_u16_dec(line, (uint8_t)XSH_CMD_LINE_CAP, &len, stack_size);
    }
    if (ok != 0u) {
        ok = xsh_line_append_cstr(line, (uint8_t)XSH_CMD_LINE_CAP, &len, (const uint8_t *)" peak=");
    }
    if (ok != 0u) {
        ok = xsh_line_append_u16_dec(line, (uint8_t)XSH_CMD_LINE_CAP, &len, peak_used);
    }
    if (ok != 0u) {
        ok = xsh_line_append_cstr(line, (uint8_t)XSH_CMD_LINE_CAP, &len, (const uint8_t *)" free_peak=");
    }
    if (ok != 0u) {
        ok = xsh_line_append_u16_dec(line, (uint8_t)XSH_CMD_LINE_CAP, &len, free_peak);
    }
    if (ok != 0u) {
        ok = xsh_line_append_cstr(line, (uint8_t)XSH_CMD_LINE_CAP, &len, (const uint8_t *)" current=");
    }
    if (ok != 0u) {
        ok = xsh_line_append_u16_dec(line, (uint8_t)XSH_CMD_LINE_CAP, &len, current_used);
    }

    if (ok != 0u) {
        xsh_cmd_emit_buffer_line(sh, line, len);
        return;
    }

    xsh_write_cstr(sh, (const uint8_t *)"stack ");
    xsh_write_u8_dec(sh, slot);
    xsh_write_cstr(sh, (const uint8_t *)" size=");
    xsh_write_u16_dec(sh, stack_size);
    xsh_write_cstr(sh, (const uint8_t *)" peak=");
    xsh_write_u16_dec(sh, peak_used);
    xsh_write_cstr(sh, (const uint8_t *)" free_peak=");
    xsh_write_u16_dec(sh, free_peak);
    xsh_write_cstr(sh, (const uint8_t *)" current=");
    xsh_write_u16_dec(sh, current_used);
    xsh_newline(sh);
}

static uint8_t cmd_stack(xsh_t *sh, uint8_t argc, uint8_t *argv[])
{
    if (argc == 1u) {
        uint8_t slot;
        for (slot = 0u; slot < MAX_TASKS; ++slot) {
            if (g_tasks[slot].state != TASK_UNUSED) {
                xsh_cmd_emit_stack_one(sh, slot);
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

static uint8_t cmd_stats(xsh_t *sh, uint8_t argc, uint8_t *argv[])
{
    zbus_stats_t stats;
    uint8_t line[ZBUS_FRAME_MAX_LEN];
    uint8_t len;
    uint8_t ok;
    uint8_t q_used;
    uint8_t q_cap;

    UNUSED(argc);
    UNUSED(argv);

    zbus_stats_snapshot(&stats);

    len = 0u;
    ok = xsh_line_append_cstr(line, (uint8_t)XSH_CMD_LINE_CAP, &len, (const uint8_t *)"zbus tx_drop=");
    if (ok != 0u) {
        ok = xsh_line_append_u16_dec(line, (uint8_t)XSH_CMD_LINE_CAP, &len, stats.tx_drop);
    }
    if (ok != 0u) {
        ok = xsh_line_append_cstr(line, (uint8_t)XSH_CMD_LINE_CAP, &len, (const uint8_t *)" rx_overflow=");
    }
    if (ok != 0u) {
        ok = xsh_line_append_u16_dec(line, (uint8_t)XSH_CMD_LINE_CAP, &len, stats.rx_overflow);
    }
    if (ok != 0u) {
        ok = xsh_line_append_cstr(line, (uint8_t)XSH_CMD_LINE_CAP, &len, (const uint8_t *)" attach_fail=");
    }
    if (ok != 0u) {
        ok = xsh_line_append_u16_dec(line, (uint8_t)XSH_CMD_LINE_CAP, &len, stats.attach_fail);
    }
    if (ok != 0u) {
        xsh_cmd_emit_buffer_line(sh, line, len);
    } else {
        xsh_write_cstr(sh, (const uint8_t *)"zbus tx_drop=");
        xsh_write_u16_dec(sh, stats.tx_drop);
        xsh_write_cstr(sh, (const uint8_t *)" rx_overflow=");
        xsh_write_u16_dec(sh, stats.rx_overflow);
        xsh_write_cstr(sh, (const uint8_t *)" attach_fail=");
        xsh_write_u16_dec(sh, stats.attach_fail);
        xsh_newline(sh);
    }

    len = 0u;
    ok = xsh_line_append_cstr(line, (uint8_t)XSH_CMD_LINE_CAP, &len, (const uint8_t *)"zlink ok=");
    if (ok != 0u) {
        ok = xsh_line_append_u16_dec(line, (uint8_t)XSH_CMD_LINE_CAP, &len, stats.zlink_rx_frames_ok);
    }
    if (ok != 0u) {
        ok = xsh_line_append_cstr(line, (uint8_t)XSH_CMD_LINE_CAP, &len, (const uint8_t *)" crc=");
    }
    if (ok != 0u) {
        ok = xsh_line_append_u16_dec(line, (uint8_t)XSH_CMD_LINE_CAP, &len, stats.zlink_rx_crc_err);
    }
    if (ok != 0u) {
        ok = xsh_line_append_cstr(line, (uint8_t)XSH_CMD_LINE_CAP, &len, (const uint8_t *)" dup=");
    }
    if (ok != 0u) {
        ok = xsh_line_append_u16_dec(line, (uint8_t)XSH_CMD_LINE_CAP, &len, stats.zlink_rx_dup);
    }
    if (ok != 0u) {
        ok = xsh_line_append_cstr(line, (uint8_t)XSH_CMD_LINE_CAP, &len, (const uint8_t *)" type=");
    }
    if (ok != 0u) {
        ok = xsh_line_append_u16_dec(line, (uint8_t)XSH_CMD_LINE_CAP, &len, stats.zlink_rx_type_err);
    }
    if (ok != 0u) {
        ok = xsh_line_append_cstr(line, (uint8_t)XSH_CMD_LINE_CAP, &len, (const uint8_t *)" len=");
    }
    if (ok != 0u) {
        ok = xsh_line_append_u16_dec(line, (uint8_t)XSH_CMD_LINE_CAP, &len, stats.zlink_rx_len_err);
    }
    if (ok != 0u) {
        xsh_cmd_emit_buffer_line(sh, line, len);
    } else {
        xsh_write_cstr(sh, (const uint8_t *)"zlink ok=");
        xsh_write_u16_dec(sh, stats.zlink_rx_frames_ok);
        xsh_write_cstr(sh, (const uint8_t *)" crc=");
        xsh_write_u16_dec(sh, stats.zlink_rx_crc_err);
        xsh_write_cstr(sh, (const uint8_t *)" dup=");
        xsh_write_u16_dec(sh, stats.zlink_rx_dup);
        xsh_write_cstr(sh, (const uint8_t *)" type=");
        xsh_write_u16_dec(sh, stats.zlink_rx_type_err);
        xsh_write_cstr(sh, (const uint8_t *)" len=");
        xsh_write_u16_dec(sh, stats.zlink_rx_len_err);
        xsh_newline(sh);
    }

    q_used = ipc_demo_queue_used();
    q_cap = ipc_demo_queue_capacity();
    xsh_write_cstr(sh, (const uint8_t *)"ipc q_used=");
    xsh_write_u8_dec(sh, q_used);
    xsh_write_cstr(sh, (const uint8_t *)" q_cap=");
    xsh_write_u8_dec(sh, q_cap);
    xsh_newline(sh);

    return 1u;
}

static const xsh_cmd_t g_xsh_cmds[] = {
    {g_xsh_cmd_name_help, g_xsh_cmd_usage_help, 0u, 0u, cmd_help},
    {g_xsh_cmd_name_cfg, g_xsh_cmd_usage_cfg, 0u, 0u, cmd_cfg},
    {g_xsh_cmd_name_tasks, g_xsh_cmd_usage_tasks, 0u, 1u, cmd_tasks},
    {g_xsh_cmd_name_start, g_xsh_cmd_usage_start, 1u, (uint8_t)(XSH_ARGV_MAX - 1u), cmd_start},
    {g_xsh_cmd_name_stop, g_xsh_cmd_usage_stop, 1u, 1u, cmd_stop},
    {g_xsh_cmd_name_weight, g_xsh_cmd_usage_weight, 2u, 2u, cmd_weight},
    {g_xsh_cmd_name_heap, g_xsh_cmd_usage_heap, 0u, 1u, cmd_heap},
    {g_xsh_cmd_name_stack, g_xsh_cmd_usage_stack, 0u, 1u, cmd_stack},
    {g_xsh_cmd_name_stats, g_xsh_cmd_usage_stats, 0u, 0u, cmd_stats}
};

void xsh_cmd_init_xsh(xsh_t *sh)
{
    if (sh == (xsh_t *)0) {
        return;
    }

    xsh_init(sh, g_xsh_cmds, (uint8_t)ARRAY_LEN(g_xsh_cmds));
    xsh_set_banner(sh, g_xsh_cmd_banner);
    xsh_set_prompt(sh, g_xsh_cmd_prompt);
    xsh_set_unknown_text(sh, g_xsh_cmd_txt_unknown);
}

void xsh_cmd_boot_cfg_emit_once(void)
{
    static uint8_t emitted = 0u;

    if (emitted != 0u) {
        return;
    }

    (void)zbus_write_tty((uint8_t)XSH_CMD_BOOTCFG_TTY, g_boot_cfg_line_0, (uint8_t)(sizeof(g_boot_cfg_line_0) - 1u));
    (void)zbus_write_tty((uint8_t)XSH_CMD_BOOTCFG_TTY, g_boot_cfg_line_1, (uint8_t)(sizeof(g_boot_cfg_line_1) - 1u));
    (void)zbus_write_tty((uint8_t)XSH_CMD_BOOTCFG_TTY, g_boot_cfg_line_2, (uint8_t)(sizeof(g_boot_cfg_line_2) - 1u));
    (void)zbus_write_tty((uint8_t)XSH_CMD_BOOTCFG_TTY, g_boot_cfg_line_3, (uint8_t)(sizeof(g_boot_cfg_line_3) - 1u));
    (void)zbus_write_tty((uint8_t)XSH_CMD_BOOTCFG_TTY, g_boot_cfg_line_4, (uint8_t)(sizeof(g_boot_cfg_line_4) - 1u));

    emitted = 1u;
}
