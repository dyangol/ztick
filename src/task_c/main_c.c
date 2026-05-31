#include <stdint.h>

#include "common.h"
#include "../bootstrap/rtos.h"
#include "../lib/activity_indicator.h"
#include "../lib/ipc_demo.h"
#include "../lib/pipe.h"
#include "../lib/sprint.h"

#pragma codeseg CODE

volatile uint16_t counter_c;

#define MAIN_C_COL 0u
#define MAIN_C_ROW 8u
#define MAIN_C_GLYPH_BASE 48u
#define MAIN_C_LABEL_GLYPH 54u
#define MAIN_C_GLYPH_COLOR 0xF4u
#define MAIN_C_ANIM_MASK 0x003Fu

static const uint8_t g_label_c[8] = {
    0x3Cu, 0x42u, 0x40u, 0x40u,
    0x40u, 0x42u, 0x3Cu, 0x00u
};
static const uint8_t g_hello_c[] = "I'm C. Hello World!";
static const uint8_t g_goodbye_c[] = "I was C. Gooodbye!";

void main_c(void)
{
    uint8_t phase = 0u;
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
        if ((counter_c & (uint16_t)0x0001u) == 0u) {
            phase = activity_indicator_next_phase(phase);
            activity_indicator_draw_phase((uint8_t)MAIN_C_LABEL_GLYPH, (uint8_t)MAIN_C_GLYPH_BASE, (uint8_t)MAIN_C_COL, (uint8_t)MAIN_C_ROW, phase);
        }
    }

    sprint_cstr_line(g_goodbye_c);
    pipe_flush();
    task_exit();
}
