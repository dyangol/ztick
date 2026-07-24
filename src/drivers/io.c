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

void io_reset_zbridge(void)
{
    /* zbrc.py's FSM triggers RESET on any OUT to this port, for the
     * duration of the Z80 bus cycle, regardless of the data byte -- see
     * zbridge/zbrc.py's module docstring. */
    io_write_port(0u, (uint8_t)IO_RESET_PORT);
}
