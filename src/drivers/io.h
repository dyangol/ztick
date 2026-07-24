#ifndef IO_H
#define IO_H

#include <stdint.h>

#ifndef IO_DEFAULT_PORT
#define IO_DEFAULT_PORT 0x3Au
#endif

#ifndef IO_RESET_PORT
#define IO_RESET_PORT 0x35u
#endif

void io_write(uint8_t value);
void io_write_port(uint8_t value, uint8_t port);
void io_write_port_raw(uint8_t value, uint8_t port);
uint8_t io_read_port(uint8_t port);
uint8_t io_read_port_raw(uint8_t port);
void io_reset_zbridge(void);

#endif
