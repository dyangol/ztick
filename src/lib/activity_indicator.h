#ifndef ACTIVITY_INDICATOR_H
#define ACTIVITY_INDICATOR_H

#include <stdint.h>

#define ACTIVITY_INDICATOR_FRAME_COUNT 4u
#define ACTIVITY_INDICATOR_LBRACKET_OFFSET 4u
#define ACTIVITY_INDICATOR_RBRACKET_OFFSET 5u

void activity_indicator_define_label(uint8_t glyph_code, const uint8_t *pattern, uint8_t color);
void activity_indicator_define_glyphs(uint8_t glyph_base, uint8_t color);
void activity_indicator_draw_phase(uint8_t label_glyph, uint8_t glyph_base, uint8_t col, uint8_t row, uint8_t phase);
uint8_t activity_indicator_next_phase(uint8_t phase);

#endif
