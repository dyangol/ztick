#include <stdint.h>

#include "vdp.h"

#pragma codeseg CODE

#define VDP_SCREEN1_R0 0x00u
#define VDP_SCREEN1_R1 0xE0u
#define VDP_SCREEN1_R2 0x06u
#define VDP_SCREEN1_R3 0x80u
#define VDP_SCREEN1_R4 0x00u
#define VDP_SCREEN1_R5 0x36u
#define VDP_SCREEN1_R6 0x07u
#define VDP_SCREEN1_R7 0xF4u

void vdp_write_vram(uint16_t address, uint8_t value)
{
    vdp_set_write_address(address);
    vdp_write_data(value);
}

uint8_t vdp_read_vram(uint16_t address)
{
    vdp_set_read_address(address);
    return vdp_read_data();
}

void vdp_fill_vram(uint16_t address, uint16_t length, uint8_t value)
{
    vdp_set_write_address(address);

    while (length > 0u) {
        vdp_write_data(value);
        length--;
    }
}

void vdp_write_block(uint16_t address, const uint8_t *data, uint16_t length)
{
    vdp_set_write_address(address);

    while (length > 0u) {
        vdp_write_data(*data);
        data++;
        length--;
    }
}

void vdp_define_char(uint8_t code, const uint8_t *pattern, uint8_t color)
{
    vdp_write_block(VDP_PATTERN_TABLE_ADDR + ((uint16_t)code << 3), pattern, 8u);
    vdp_write_vram(VDP_COLOR_TABLE_ADDR + (code >> 3), color);
}

void vdp_putc(uint8_t x, uint8_t y, uint8_t code)
{
    vdp_write_vram(VDP_NAME_TABLE_ADDR + ((uint16_t)y << 5) + x, code);
}

void vdp_init_screen1(void)
{
    vdp_write_register(VDP_SCREEN1_R0, 0u);
    vdp_write_register(VDP_SCREEN1_R1, 1u);
    vdp_write_register(VDP_SCREEN1_R2, 2u);
    vdp_write_register(VDP_SCREEN1_R3, 3u);
    vdp_write_register(VDP_SCREEN1_R4, 4u);
    vdp_write_register(VDP_SCREEN1_R5, 5u);
    vdp_write_register(VDP_SCREEN1_R6, 6u);
    vdp_write_register(VDP_SCREEN1_R7, 7u);

    vdp_fill_vram(VDP_PATTERN_TABLE_ADDR, 0x0800u, 0x00u);
    vdp_fill_vram(VDP_NAME_TABLE_ADDR, 0x0300u, 0x00u);
    vdp_fill_vram(VDP_COLOR_TABLE_ADDR, 0x0020u, 0xF4u);
    vdp_fill_vram(VDP_SPRITE_ATTR_ADDR, 0x0080u, 0xD0u);
    vdp_fill_vram(VDP_SPRITE_PATTERN_ADDR, 0x0800u, 0x00u);
}
