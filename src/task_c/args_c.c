#include <stdint.h>

#include "../lib/args.h"
#include "args_c.h"

#pragma codeseg CODE

static volatile uint8_t g_task_c_start_valid = 0u;
static volatile uint8_t g_task_c_start_filter = 0u;

static uint8_t task_c_parse_filter(const uint8_t *text, uint8_t *out_filter)
{
    if ((text == (const uint8_t *)0) || (out_filter == (uint8_t *)0)) {
        return 0u;
    }

    /* Accept the short form used in the shell plus an explicit key=value alias. */
    if ((task_start_arg_equals(text, (const uint8_t *)"even") != 0u)
        || (task_start_arg_equals(text, (const uint8_t *)"filter=even") != 0u)) {
        *out_filter = (uint8_t)TASK_C_FILTER_EVEN;
        return 1u;
    }
    if ((task_start_arg_equals(text, (const uint8_t *)"odd") != 0u)
        || (task_start_arg_equals(text, (const uint8_t *)"filter=odd") != 0u)) {
        *out_filter = (uint8_t)TASK_C_FILTER_ODD;
        return 1u;
    }
    if ((task_start_arg_equals(text, (const uint8_t *)"all") != 0u)
        || (task_start_arg_equals(text, (const uint8_t *)"filter=all") != 0u)) {
        *out_filter = (uint8_t)TASK_C_FILTER_ALL;
        return 1u;
    }

    return 0u;
}

uint8_t task_c_start_configure(uint8_t argc, uint8_t *argv[])
{
    uint8_t filter;

    /* No start arg means "use the build default"; clear any previous override. */
    if (argc == 0u) {
        task_c_start_reset();
        return 1u;
    }
    /* The start parser only expects one selector token. */
    if ((argc != 1u) || (argv == (uint8_t **)0)) {
        return 0u;
    }

    if (task_c_parse_filter(argv[0], &filter) == 0u) {
        return 0u;
    }

    g_task_c_start_filter = filter;
    g_task_c_start_valid = 1u;
    return 1u;
}

void task_c_start_reset(void)
{
    /* Reset the latched selector between runs so a fresh start is deterministic. */
    g_task_c_start_valid = 0u;
    g_task_c_start_filter = 0u;
}

uint8_t task_c_filter_resolve(uint8_t default_filter)
{
    /* A valid override wins; otherwise fall back to the compile-time default filter. */
    if (g_task_c_start_valid != 0u) {
        return g_task_c_start_filter;
    }

    return default_filter;
}
