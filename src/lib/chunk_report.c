#include <stdint.h>

#include "chunk_report.h"

#pragma codeseg CODE

void chunk_report_append_progress(sprint_t *out, uint16_t chunk_start, uint16_t chunk_end,
                                   uint16_t bytes_failed)
{
    (void)sprint_cstr(out, (const uint8_t *)" 0x");
    (void)sprint_hex16(out, chunk_start);
    (void)sprint_cstr(out, (const uint8_t *)"-0x");
    (void)sprint_hex16(out, chunk_end);
    (void)sprint_cstr(out, (const uint8_t *)" failed=");
    (void)sprint_u16_dec(out, bytes_failed);
}

void chunk_report_append_result(sprint_t *out, uint16_t total_chunks, uint16_t failed_chunks)
{
    (void)sprint_cstr(out, (const uint8_t *)" chunks=");
    (void)sprint_u16_dec(out, total_chunks);
    (void)sprint_cstr(out, (const uint8_t *)" failed=");
    (void)sprint_u16_dec(out, failed_chunks);
}
