#ifndef PIPE_H
#define PIPE_H

#include <stdint.h>

void pipe_write_bytes(const uint8_t *data, uint8_t len);
void pipe_write_cstr(const uint8_t *text);
void pipe_write_u8_dec(uint8_t value);
void pipe_write_u16_dec(uint16_t value);
void pipe_write_u16_hex(uint16_t value);
void pipe_newline(void);
void pipe_flush(void);

#endif
