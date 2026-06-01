#include <stdint.h>

#include "common.h"
#include "../bootstrap/rtos.h"
#include "../drivers/zbus.h"
#include "../lib/task.h"
#include "../lib/pipe.h"
#include "../lib/sprint.h"
#include "xsh.h"
#include "xsh_cmd.h"

#pragma codeseg CODE

volatile uint16_t counter_a;

static xsh_t g_xsh;
static const uint8_t g_task_name_xsh[] = "xsh";
static const uint8_t g_goodbye_xsh[] = "I was XSH. Goodbye!";

void main_xsh(void);

const task_spec_t g_task_spec_xsh = {
    g_task_name_xsh,
    main_xsh,
    2u,
    0u,
    (const uint8_t *)0,
    (task_start_args_configure_t)0,
    (task_start_args_reset_t)0
};

void main_xsh(void)
{
    uint8_t tty_id;

    counter_a = 0u;

    xsh_cmd_init_xsh(&g_xsh);
    xsh_cmd_boot_cfg_emit_once();

    while (1) {
        counter_a++;

        if (rtos_task_stop_requested() != 0u) {
            sprint_cstr_line(g_goodbye_xsh);
            pipe_flush();
            break;
        }

        if (zbus_tty_get_current(&tty_id) != 0u) {
            xsh_set_tty(&g_xsh, tty_id);
        }
        xsh_tick(&g_xsh);

        if (rtos_task_stop_requested() != 0u) {
            sprint_cstr_line(g_goodbye_xsh);
            pipe_flush();
            break;
        }

        CPU_NOP();
        CPU_NOP();
    }

    pipe_flush();
    task_exit();
}
