#ifndef SPRINT_H
#define SPRINT_H

#include <stdint.h>

typedef struct sprint {
    uint8_t *buf;
    uint8_t cap;
    uint8_t len;
    uint8_t ok;
} sprint_t;

void sprint_begin(sprint_t *sp, uint8_t *buf, uint8_t cap);
uint8_t sprint_ok(const sprint_t *sp);
uint8_t sprint_cstr(sprint_t *sp, const uint8_t *text);
uint8_t sprint_char(sprint_t *sp, uint8_t ch);
uint8_t sprint_hex8(sprint_t *sp, uint8_t value);
uint8_t sprint_hex16(sprint_t *sp, uint16_t value);
uint8_t sprint_u8_dec(sprint_t *sp, uint8_t value);
uint8_t sprint_u16_dec(sprint_t *sp, uint16_t value);
void sprint_emit_line(const sprint_t *sp);
void sprint_cstr_line(const uint8_t *text);

#endif
