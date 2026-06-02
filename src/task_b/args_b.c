#include <stdint.h>

#include "../lib/args.h"
#include "args_b.h"

#pragma codeseg CODE

#define TASK_B_MASK_FAST   0x000Fu
#define TASK_B_MASK_NORMAL 0x003Fu
#define TASK_B_MASK_SLOW   0x00FFu

static volatile uint8_t g_task_b_start_valid = 0u;
static volatile uint16_t g_task_b_start_anim_mask = 0u;

static uint8_t task_b_parse_speed(const uint8_t *text, uint16_t *out_mask)
{
    if ((text == (const uint8_t *)0) || (out_mask == (uint16_t *)0)) {
        return 0u;
    }

    /* Accept the short form used in the shell plus an explicit key=value alias. */
    if ((task_start_arg_equals(text, (const uint8_t *)"fast") != 0u)
        || (task_start_arg_equals(text, (const uint8_t *)"speed=fast") != 0u)) {
        *out_mask = (uint16_t)TASK_B_MASK_FAST;
        return 1u;
    }
    if ((task_start_arg_equals(text, (const uint8_t *)"normal") != 0u)
        || (task_start_arg_equals(text, (const uint8_t *)"speed=normal") != 0u)) {
        *out_mask = (uint16_t)TASK_B_MASK_NORMAL;
        return 1u;
    }
    if ((task_start_arg_equals(text, (const uint8_t *)"slow") != 0u)
        || (task_start_arg_equals(text, (const uint8_t *)"speed=slow") != 0u)) {
        *out_mask = (uint16_t)TASK_B_MASK_SLOW;
        return 1u;
    }

    return 0u;
}

uint8_t task_b_start_configure(uint8_t argc, uint8_t *argv[])
{
    uint16_t mask;

    /* No start arg means "use the build default"; clear any previous override. */
    if (argc == 0u) {
        task_b_start_reset();
        return 1u;
    }
    /* The start parser only expects one selector token. */
    if ((argc != 1u) || (argv == (uint8_t **)0)) {
        return 0u;
    }

    if (task_b_parse_speed(argv[0], &mask) == 0u) {
        return 0u;
    }

    g_task_b_start_anim_mask = mask;
    g_task_b_start_valid = 1u;
    return 1u;
}

void task_b_start_reset(void)
{
    /* Reset the latched selector between runs so a fresh start is deterministic. */
    g_task_b_start_valid = 0u;
    g_task_b_start_anim_mask = 0u;
}

uint16_t task_b_anim_mask_resolve(uint16_t default_mask)
{
    /* A valid override wins; otherwise fall back to the compile-time default mask. */
    if (g_task_b_start_valid != 0u) {
        if ((g_task_b_start_anim_mask == (uint16_t)TASK_B_MASK_FAST)
            || (g_task_b_start_anim_mask == (uint16_t)TASK_B_MASK_NORMAL)
            || (g_task_b_start_anim_mask == (uint16_t)TASK_B_MASK_SLOW)) {
            return g_task_b_start_anim_mask;
        }
    }

    return default_mask;
}
