#ifndef VDP_H
#define VDP_H

#include <stdint.h>
#define VDP_NAME_TABLE_ADDR     0x1800u
#define VDP_COLOR_TABLE_ADDR    0x2000u
#define VDP_PATTERN_TABLE_ADDR  0x0000u
#define VDP_SPRITE_ATTR_ADDR    0x1B00u
#define VDP_SPRITE_PATTERN_ADDR 0x3800u

void vdp_init_screen1(void);
void vdp_write_vram(uint16_t address, uint8_t value);
void vdp_fill_vram(uint16_t address, uint16_t length, uint8_t value);
void vdp_write_block(uint16_t address, const uint8_t *data, uint16_t length);
void vdp_define_char(uint8_t code, const uint8_t *pattern, uint8_t color);
void vdp_putc(uint8_t x, uint8_t y, uint8_t code);

void vdp_write_register(uint8_t value, uint8_t reg);
void vdp_set_write_address(uint16_t address);
void vdp_write_data(uint8_t value);

#endif
