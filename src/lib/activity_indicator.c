#include <stdint.h>

#include "../common/common.h"
#include "../drivers/vdp.h"
#include "activity_indicator.h"

static const uint8_t g_activity_frame_0[8] = {
    0x01u, 0x02u, 0x04u, 0x08u,
    0x10u, 0x20u, 0x40u, 0x00u
};

static const uint8_t g_activity_frame_1[8] = {
    0x00u, 0x00u, 0x00u, 0x7Eu,
    0x00u, 0x00u, 0x00u, 0x00u
};

static const uint8_t g_activity_frame_2[8] = {
    0x40u, 0x20u, 0x10u, 0x08u,
    0x04u, 0x02u, 0x01u, 0x00u
};

static const uint8_t g_activity_frame_3[8] = {
    0x18u, 0x18u, 0x18u, 0x18u,
    0x18u, 0x18u, 0x18u, 0x00u
};

static const uint8_t g_activity_bracket_l[8] = {
    0x1Cu, 0x10u, 0x10u, 0x10u,
    0x10u, 0x10u, 0x1Cu, 0x00u
};

static const uint8_t g_activity_bracket_r[8] = {
    0x38u, 0x08u, 0x08u, 0x08u,
    0x08u, 0x08u, 0x38u, 0x00u
};

void activity_indicator_define_label(uint8_t glyph_code, const uint8_t *pattern, uint8_t color)
{
    CPU_DI();
    vdp_define_char(glyph_code, pattern, color);
    CPU_EI();
}

void activity_indicator_define_glyphs(uint8_t glyph_base, uint8_t color)
{
    CPU_DI();
    vdp_define_char((uint8_t)(glyph_base + 0u), g_activity_frame_0, color);
    vdp_define_char((uint8_t)(glyph_base + 1u), g_activity_frame_1, color);
    vdp_define_char((uint8_t)(glyph_base + 2u), g_activity_frame_2, color);
    vdp_define_char((uint8_t)(glyph_base + 3u), g_activity_frame_3, color);
    vdp_define_char((uint8_t)(glyph_base + ACTIVITY_INDICATOR_LBRACKET_OFFSET), g_activity_bracket_l, color);
    vdp_define_char((uint8_t)(glyph_base + ACTIVITY_INDICATOR_RBRACKET_OFFSET), g_activity_bracket_r, color);
    CPU_EI();
}

void activity_indicator_draw_phase(uint8_t label_glyph, uint8_t glyph_base, uint8_t col, uint8_t row, uint8_t phase)
{
    CPU_DI();
    vdp_putc(col, row, label_glyph);
    vdp_putc((uint8_t)(col + 1u), row, 0u);
    vdp_putc((uint8_t)(col + 2u), row, (uint8_t)(glyph_base + ACTIVITY_INDICATOR_LBRACKET_OFFSET));
    vdp_putc((uint8_t)(col + 3u), row, (uint8_t)(glyph_base + (phase & 0x03u)));
    vdp_putc((uint8_t)(col + 4u), row, (uint8_t)(glyph_base + ACTIVITY_INDICATOR_RBRACKET_OFFSET));
    CPU_EI();
}

uint8_t activity_indicator_next_phase(uint8_t phase)
{
    return (uint8_t)((phase + 1u) & 0x03u);
}
