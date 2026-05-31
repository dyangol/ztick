#include <stdint.h>

#include "args.h"

#pragma codeseg CODE

uint8_t task_start_arg_equals(const uint8_t *text, const uint8_t *lit)
{
    uint8_t i = 0u;

    if ((text == (const uint8_t *)0) || (lit == (const uint8_t *)0)) {
        return 0u;
    }

    while (lit[i] != 0u) {
        if (text[i] != lit[i]) {
            return 0u;
        }
        i++;
    }

    return (text[i] == 0u) ? 1u : 0u;
}
