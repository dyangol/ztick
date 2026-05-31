#include <stdint.h>

#include "pipe.h"
#include "sprint.h"

#pragma codeseg CODE

static uint8_t sprint_append_raw(sprint_t *sp, uint8_t ch)
{
    if ((sp == (sprint_t *)0) || (sp->buf == (uint8_t *)0) || (sp->cap == 0u)) {
        return 0u;
    }

    if ((sp->ok == 0u) || (sp->len >= (uint8_t)(sp->cap - 1u))) {
        sp->ok = 0u;
        return 0u;
    }

    sp->buf[sp->len] = ch;
    sp->len = (uint8_t)(sp->len + 1u);
    sp->buf[sp->len] = 0u;
    return 1u;
}

static uint8_t sprint_hex_digit(uint8_t nibble)
{
    nibble &= 0x0Fu;
    return (nibble < 10u) ? (uint8_t)('0' + nibble) : (uint8_t)('A' + (nibble - 10u));
}

void sprint_begin(sprint_t *sp, uint8_t *buf, uint8_t cap)
{
    if (sp == (sprint_t *)0) {
        return;
    }

    sp->buf = buf;
    sp->cap = cap;
    sp->len = 0u;
    sp->ok = ((buf != (uint8_t *)0) && (cap > 0u)) ? 1u : 0u;
    if (sp->ok != 0u) {
        sp->buf[0] = 0u;
    }
}

uint8_t sprint_ok(const sprint_t *sp)
{
    if (sp == (const sprint_t *)0) {
        return 0u;
    }
    return sp->ok;
}

uint8_t sprint_char(sprint_t *sp, uint8_t ch)
{
    return sprint_append_raw(sp, ch);
}

uint8_t sprint_cstr(sprint_t *sp, const uint8_t *text)
{
    if ((sp == (sprint_t *)0) || (text == (const uint8_t *)0)) {
        return 0u;
    }

    while (text[0] != 0u) {
        if (sprint_append_raw(sp, text[0]) == 0u) {
            return 0u;
        }
        text++;
    }

    return 1u;
}

uint8_t sprint_hex8(sprint_t *sp, uint8_t value)
{
    if (sprint_append_raw(sp, sprint_hex_digit((uint8_t)(value >> 4))) == 0u) {
        return 0u;
    }
    return sprint_append_raw(sp, sprint_hex_digit(value));
}

uint8_t sprint_hex16(sprint_t *sp, uint16_t value)
{
    if (sprint_append_raw(sp, sprint_hex_digit((uint8_t)(value >> 12))) == 0u) {
        return 0u;
    }
    if (sprint_append_raw(sp, sprint_hex_digit((uint8_t)(value >> 8))) == 0u) {
        return 0u;
    }
    if (sprint_append_raw(sp, sprint_hex_digit((uint8_t)(value >> 4))) == 0u) {
        return 0u;
    }
    return sprint_append_raw(sp, sprint_hex_digit((uint8_t)value));
}

uint8_t sprint_u16_dec(sprint_t *sp, uint16_t value)
{
    uint8_t digits[5];
    uint8_t count = 0u;
    uint8_t i;

    if (value == 0u) {
        return sprint_append_raw(sp, (uint8_t)'0');
    }

    while (value > 0u) {
        digits[count] = (uint8_t)((value % 10u) + (uint8_t)'0');
        value = (uint16_t)(value / 10u);
        count++;
    }

    for (i = count; i > 0u; --i) {
        if (sprint_append_raw(sp, digits[i - 1u]) == 0u) {
            return 0u;
        }
    }

    return 1u;
}

uint8_t sprint_u8_dec(sprint_t *sp, uint8_t value)
{
    return sprint_u16_dec(sp, (uint16_t)value);
}

void sprint_emit_line(const sprint_t *sp)
{
    if ((sp == (const sprint_t *)0) || (sp->buf == (uint8_t *)0)) {
        return;
    }

    pipe_write_cstr(sp->buf);
    pipe_newline();
}

void sprint_cstr_line(const uint8_t *text)
{
    pipe_write_cstr(text);
    pipe_newline();
}
