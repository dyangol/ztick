#ifndef START_ARGS_D_H
#define START_ARGS_D_H

#include <stdint.h>

uint8_t task_d_start_configure(uint8_t argc, uint8_t *argv[]);
void task_d_start_reset(void);
uint8_t task_d_safe_mode_resolve(uint8_t default_mode);

#endif
