#ifndef ARGS_GCHK_H
#define ARGS_GCHK_H

#include <stdint.h>

uint8_t task_gchk_start_configure(uint8_t argc, uint8_t *argv[]);
void task_gchk_start_reset(void);
uint8_t task_gchk_seconds_resolve(uint8_t default_seconds);

#endif
