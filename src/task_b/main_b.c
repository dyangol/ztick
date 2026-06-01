#include <stdint.h>

#include "common.h"
#include "../bootstrap/rtos.h"
#include "../lib/activity_indicator.h"
#include "../lib/ipc_demo.h"
#include "../lib/pipe.h"
#include "../lib/sprint.h"
#include "args_b.h"

#pragma codeseg CODE

volatile uint16_t counter_b;

#define MAIN_B_COL 0u
#define MAIN_B_ROW 0u
#define MAIN_B_GLYPH_BASE 40u
#define MAIN_B_LABEL_GLYPH 46u
#define MAIN_B_GLYPH_COLOR 0xF4u
#define MAIN_B_ANIM_MASK_DEFAULT 0x003Fu

static const uint8_t g_label_b[8] = {
    0x7Cu, 0x42u, 0x42u, 0x7Cu,
    0x42u, 0x42u, 0x7Cu, 0x00u
};
static const uint8_t g_hello_b[] = "I'm B. Hello World!";
static const uint8_t g_goodbye_b[] = "I was B. Gooodbye!";

void main_b(void)
{
    uint8_t phase = 0u;
    uint16_t tx_seq = 0u;
    uint16_t anim_mask = task_b_anim_mask_resolve((uint16_t)MAIN_B_ANIM_MASK_DEFAULT);

    counter_b = 0u;
    ipc_demo_init_once();

    activity_indicator_define_label((uint8_t)MAIN_B_LABEL_GLYPH, g_label_b, (uint8_t)MAIN_B_GLYPH_COLOR);
    activity_indicator_define_glyphs((uint8_t)MAIN_B_GLYPH_BASE, (uint8_t)MAIN_B_GLYPH_COLOR);
    activity_indicator_draw_phase((uint8_t)MAIN_B_LABEL_GLYPH, (uint8_t)MAIN_B_GLYPH_BASE, (uint8_t)MAIN_B_COL, (uint8_t)MAIN_B_ROW, phase);
    sprint_cstr_line(g_hello_b);

    while (1) {
        if (rtos_task_stop_requested() != 0u) {
            break;
        }
        counter_b++;
        if ((counter_b & anim_mask) == 0u) {
            tx_seq++;
            (void)ipc_demo_send_u16(tx_seq);
            phase = activity_indicator_next_phase(phase);
            activity_indicator_draw_phase((uint8_t)MAIN_B_LABEL_GLYPH, (uint8_t)MAIN_B_GLYPH_BASE, (uint8_t)MAIN_B_COL, (uint8_t)MAIN_B_ROW, phase);
        }
        CPU_NOP();
        CPU_NOP();
    }

    sprint_cstr_line(g_goodbye_b);
    pipe_flush();
    task_exit();
}
