#ifndef CHUNK_REPORT_H
#define CHUNK_REPORT_H

#include <stdint.h>

#include "sprint.h"

/* Appends " 0x<chunk_start>-0x<chunk_end> failed=<bytes_failed>" to `out` --
 * the standard per-chunk progress suffix shared by every chunked
 * memory-check task (rchk, vchk). Caller has already sprint_begin()'d and
 * written its own task-specific prefix (e.g. "rchk PROGRESS page=3", "vchk
 * PROGRESS region=sprpat"). */
void chunk_report_append_progress(sprint_t *out, uint16_t chunk_start, uint16_t chunk_end,
                                   uint16_t bytes_failed);

/* Appends " chunks=<total> failed=<failed_chunks>" to `out` -- the standard
 * final-result suffix (failed_chunks counts chunks with at least one
 * mismatched byte, out of total_chunks tested). */
void chunk_report_append_result(sprint_t *out, uint16_t total_chunks, uint16_t failed_chunks);

#endif
