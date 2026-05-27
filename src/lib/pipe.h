#ifndef PIPE_H
#define PIPE_H

#include <stdint.h>

void pipe_write_cstr(const uint8_t *text);
void pipe_newline(void);
void pipe_flush(void);

#endif
