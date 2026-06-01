#include <stdint.h>

#include "common.h"
#include "../bootstrap/rtos.h"
#include "../lib/activity_indicator.h"
#include "../lib/ipc_demo.h"
#include "../lib/pipe.h"
#include "../lib/sprint.h"
#include "../lib/task.h"
#include "args_c.h"

#pragma codeseg CODE

volatile uint16_t counter_c;

#define MAIN_C_COL 0u
#define MAIN_C_ROW 1u
#define MAIN_C_GLYPH_BASE 48u
#define MAIN_C_LABEL_GLYPH 54u
#define MAIN_C_GLYPH_COLOR 0xF4u
static const uint8_t g_task_name_c[] = "c";
static const uint8_t g_task_c_start_args_usage[] = "even|odd|all";
static const uint8_t g_label_c[8] = {
    0x3Cu, 0x42u, 0x40u, 0x40u,
    0x40u, 0x42u, 0x3Cu, 0x00u
};
static const uint8_t g_hello_c[] = "I'm C. Hello World!";
static const uint8_t g_goodbye_c[] = "I was C. Gooodbye!";

void main_c(void);

const task_spec_t g_task_spec_c = {
    g_task_name_c,
    main_c,
    TASK_WEIGHT_MIN,
    g_task_c_start_args_usage,
    task_c_start_configure,
    task_c_start_reset
};

static uint8_t task_c_should_animate(uint16_t value, uint8_t filter)
{
    uint8_t parity = (uint8_t)(value & (uint16_t)0x0001u);

    if (filter == (uint8_t)TASK_C_FILTER_ALL) {
        return 1u;
    }
    if (filter == (uint8_t)TASK_C_FILTER_ODD) {
        return (parity != 0u) ? 1u : 0u;
    }

    return (parity == 0u) ? 1u : 0u;
}

void main_c(void)
{
    uint8_t phase = 0u;
    uint8_t filter = task_c_filter_resolve((uint8_t)TASK_C_FILTER_EVEN);
    uint16_t rx_value = 0u;

    counter_c = 0u;
    ipc_demo_init_once();

    activity_indicator_define_label((uint8_t)MAIN_C_LABEL_GLYPH, g_label_c, (uint8_t)MAIN_C_GLYPH_COLOR);
    activity_indicator_define_glyphs((uint8_t)MAIN_C_GLYPH_BASE, (uint8_t)MAIN_C_GLYPH_COLOR);
    activity_indicator_draw_phase((uint8_t)MAIN_C_LABEL_GLYPH, (uint8_t)MAIN_C_GLYPH_BASE, (uint8_t)MAIN_C_COL, (uint8_t)MAIN_C_ROW, phase);
    sprint_cstr_line(g_hello_c);

    while (1) {
        if (rtos_task_stop_requested() != 0u) {
            break;
        }
        if (ipc_demo_recv_u16(&rx_value) == 0u) {
            if (rtos_task_stop_requested() != 0u) {
                break;
            }
            continue;
        }
        counter_c = rx_value;
        if (task_c_should_animate(counter_c, filter) != 0u) {
            phase = activity_indicator_next_phase(phase);
            activity_indicator_draw_phase((uint8_t)MAIN_C_LABEL_GLYPH, (uint8_t)MAIN_C_GLYPH_BASE, (uint8_t)MAIN_C_COL, (uint8_t)MAIN_C_ROW, phase);
        }
    }

    sprint_cstr_line(g_goodbye_c);
    pipe_flush();
    task_exit();
}
