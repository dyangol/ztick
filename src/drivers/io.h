#ifndef IO_H
#define IO_H

#include <stdint.h>

#ifndef IO_DEFAULT_PORT
#define IO_DEFAULT_PORT 0x3Au
#endif

void io_write(uint8_t value);
void io_write_port(uint8_t value, uint8_t port);
void io_write_port_raw(uint8_t value, uint8_t port);
uint8_t io_read_port(uint8_t port);
uint8_t io_read_port_raw(uint8_t port);

#endif
