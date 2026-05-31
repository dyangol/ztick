#ifndef START_ARGS_C_H
#define START_ARGS_C_H

#include <stdint.h>

#define TASK_C_FILTER_EVEN 0u
#define TASK_C_FILTER_ODD  1u
#define TASK_C_FILTER_ALL  2u

uint8_t task_c_start_configure(uint8_t argc, uint8_t *argv[]);
void task_c_start_reset(void);
uint8_t task_c_filter_resolve(uint8_t default_filter);

#endif
