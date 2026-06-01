#include <stdint.h>

#include "../lib/mem_probe.h"
#include "../lib/args.h"
#include "args_rchk.h"

#pragma codeseg CODE

static volatile uint8_t g_rchk_start_valid = 0u;
static volatile uint8_t g_rchk_start_safe_mode = 0u;

static uint8_t rchk_parse_mode(const uint8_t *text, uint8_t *out_mode)
{
    if ((text == (const uint8_t *)0) || (out_mode == (uint8_t *)0)) {
        return 0u;
    }

    if ((task_start_arg_equals(text, (const uint8_t *)"safe") != 0u)
        || (task_start_arg_equals(text, (const uint8_t *)"mode=safe") != 0u)) {
        *out_mode = MEM_PROBE_MODE_SAFE;
        return 1u;
    }
    if ((task_start_arg_equals(text, (const uint8_t *)"unsafe") != 0u)
        || (task_start_arg_equals(text, (const uint8_t *)"mode=unsafe") != 0u)) {
        *out_mode = MEM_PROBE_MODE_UNSAFE;
        return 1u;
    }

    return 0u;
}

uint8_t rchk_start_configure(uint8_t argc, uint8_t *argv[])
{
    uint8_t mode;

    if (argc == 0u) {
        rchk_start_reset();
        return 1u;
    }
    if ((argc != 1u) || (argv == (uint8_t **)0)) {
        return 0u;
    }

    if (rchk_parse_mode(argv[0], &mode) == 0u) {
        return 0u;
    }

    g_rchk_start_safe_mode = (mode != 0u) ? 1u : 0u;
    g_rchk_start_valid = 1u;
    return 1u;
}

void rchk_start_reset(void)
{
    g_rchk_start_valid = 0u;
    g_rchk_start_safe_mode = 0u;
}

uint8_t rchk_safe_mode_resolve(uint8_t default_mode)
{
    if (g_rchk_start_valid != 0u) {
        return (g_rchk_start_safe_mode != 0u) ? 1u : 0u;
    }
    return (default_mode != 0u) ? 1u : 0u;
}
