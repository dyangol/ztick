#include <stdint.h>

#include "args_gchk.h"

#pragma codeseg CODE

static volatile uint8_t g_gchk_start_valid = 0u;
static volatile uint8_t g_gchk_start_seconds = 0u;

/* Small local decimal parser (like xsh's own xsh_parse_u8, src/xsh/xsh.c)
 * -- kept local rather than reused from xsh, since task start-arg parsing
 * (src/lib/args.h) is deliberately independent of the xsh module. */
static uint8_t gchk_parse_seconds(const uint8_t *text, uint8_t *out_seconds)
{
    uint16_t value = 0u;
    uint8_t i = 0u;

    if ((text == (const uint8_t *)0) || (out_seconds == (uint8_t *)0) || (text[0] == 0u)) {
        return 0u;
    }

    while (text[i] != 0u) {
        uint8_t ch = text[i];
        if ((ch < (uint8_t)'0') || (ch > (uint8_t)'9')) {
            return 0u;
        }
        value = (uint16_t)(value * 10u + (uint16_t)(ch - (uint8_t)'0'));
        if (value > 255u) {
            return 0u;
        }
        i++;
    }

    *out_seconds = (uint8_t)value;
    return 1u;
}

uint8_t task_gchk_start_configure(uint8_t argc, uint8_t *argv[])
{
    uint8_t seconds;

    /* No argument means "use the build default"; reset any previously
     * latched value so the next start is deterministic. */
    if (argc == 0u) {
        task_gchk_start_reset();
        return 1u;
    }
    if ((argc != 1u) || (argv == (uint8_t **)0)) {
        return 0u;
    }
    if ((gchk_parse_seconds(argv[0], &seconds) == 0u) || (seconds == 0u)) {
        return 0u;
    }

    g_gchk_start_seconds = seconds;
    g_gchk_start_valid = 1u;
    return 1u;
}

void task_gchk_start_reset(void)
{
    g_gchk_start_valid = 0u;
    g_gchk_start_seconds = 0u;
}

uint8_t task_gchk_seconds_resolve(uint8_t default_seconds)
{
    if (g_gchk_start_valid != 0u) {
        return g_gchk_start_seconds;
    }
    return default_seconds;
}
