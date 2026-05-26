#include <stdint.h>

#include "io.h"

#pragma codeseg CODE

void io_write(uint8_t value)
{
    io_write_port(value, (uint8_t)IO_DEFAULT_PORT);
}

void io_write_port(uint8_t value, uint8_t port)
{
    io_write_port_raw(value, port);
}

uint8_t io_read_port(uint8_t port)
{
    return io_read_port_raw(port);
}
