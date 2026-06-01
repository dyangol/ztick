#ifndef START_ARGS_RCHK_H
#define START_ARGS_RCHK_H

#include <stdint.h>

uint8_t rchk_start_configure(uint8_t argc, uint8_t *argv[]);
void rchk_start_reset(void);
uint8_t rchk_safe_mode_resolve(uint8_t default_mode);

#endif
