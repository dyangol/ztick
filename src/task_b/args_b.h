#ifndef START_ARGS_B_H
#define START_ARGS_B_H

#include <stdint.h>

uint8_t task_b_start_configure(uint8_t argc, uint8_t *argv[]);
void task_b_start_reset(void);
uint16_t task_b_anim_mask_resolve(uint16_t default_mask);

#endif
