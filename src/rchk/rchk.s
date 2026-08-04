;; src/rchk/rchk.s
.module rchk

.globl _rchk_run

.globl _ram_exec_run
.globl _ram_exec_return
.globl _g_ram_exec_src
.globl _g_ram_exec_len
.globl _g_ram_exec_addr

.globl _g_rchk_psr_old
.globl _g_rchk_psr_new
.globl _g_rchk_psr_port
.globl _g_rchk_length
.globl _g_rchk_value
.globl _g_rchk_safe_mode
.globl _g_rchk_base_addr
.globl _g_rchk_safe_sp
.globl _g_rchk_exec_addr
.globl _g_rchk_saved_sp
.globl _g_rchk_fail
.globl _g_rchk_fail_addr
.globl _g_rchk_read_value
.globl _g_rchk_fail_count

.area _CODE

_rchk_run::
    ; Copy the rchk stub to the caller-provided RAM trampoline address and execute it.
    ld hl, #_rchk_exec_stub
    ld (_g_ram_exec_src), hl
    ld hl, #(_rchk_exec_stub_end - _rchk_exec_stub)
    ld (_g_ram_exec_len), hl
    ld hl, (_g_rchk_exec_addr)
    ld (_g_ram_exec_addr), hl
    call _ram_exec_run
    ret

_rchk_exec_stub:
    ; Critical section starts: preserve stack and remap target page via PSR.
    di
    ld (_g_rchk_saved_sp), sp
    ld hl, (_g_rchk_safe_sp)
    ld sp, hl

    ld hl, (_g_rchk_base_addr)
    ld a, (_g_rchk_length)
    ld b, a
    ld a, (_g_rchk_value)
    ld d, a
    ld a, (_g_rchk_safe_mode)
    ld e, a

    ld a, (_g_rchk_psr_port)
    ld c, a
    ld a, (_g_rchk_psr_old)
    push af
    ld a, (_g_rchk_psr_new)
    out (c), a

    ; Every byte in the chunk is tested regardless of earlier failures --
    ; g_rchk_fail_count (reset by rchk_prepare_chunk()) tallies them all,
    ; while g_rchk_fail_addr/g_rchk_read_value latch only the first one
    ; (see _rchk_record_fail). Unlike the old stop-at-first-failure version,
    ; `c` (the PSR port) is no longer live across the whole loop -- it's
    ; reused below as scratch for the original byte, and reloaded from
    ; g_rchk_psr_port at _rchk_done before the final `out`.
_rchk_loop:
    ld a, b
    or a
    jr z, _rchk_done

    ld a, e
    or a
    jr z, _rchk_fast

    ; Safe mode: save original in c, write/verify, restore original either way.
    ld a, (hl)
    ld c, a
    ld a, d
    ld (hl), a
    ld a, (hl)
    cp d
    jr z, _rchk_safe_restore
    call _rchk_record_fail
_rchk_safe_restore:
    ld a, c
    ld (hl), a
    jr _rchk_next

_rchk_fast:
    ; Unsafe mode: write/read verify only, no restore of original byte.
    ld a, d
    ld (hl), a
    ld a, (hl)
    cp d
    jr z, _rchk_next
    call _rchk_record_fail

_rchk_next:
    inc hl
    djnz _rchk_loop

_rchk_done:
    ; Restore PSR (c reloaded -- it was repurposed as scratch above) + stack.
    ld a, (_g_rchk_psr_port)
    ld c, a
    pop af
    out (c), a
    ld sp, (_g_rchk_saved_sp)
    ei

    ld a, (_g_rchk_fail_count)
    or a
    jr z, _rchk_store_success

    ld a, #1
    ld (_g_rchk_fail), a
    jp _ram_exec_return

_rchk_store_success:
    ; Success path reports the original test value as the observed byte.
    xor a
    ld (_g_rchk_fail), a
    ld a, (_g_rchk_value)
    ld (_g_rchk_read_value), a
    jp _ram_exec_return

    ; Records one mismatched byte: always increments g_rchk_fail_count;
    ; latches g_rchk_fail_addr/g_rchk_read_value only the first time this
    ; chunk sees a failure. In: a = readback (mismatched) value, hl =
    ; address. Preserves hl/b/c/d/e; uses the shadow AF' (not the stack,
    ; which stays untouched here on purpose -- no push/pop to keep balanced
    ; across the two call sites above) to hold the readback value while
    ; checking/updating g_rchk_fail_count.
_rchk_record_fail:
    ex af, af'
    ld a, (_g_rchk_fail_count)
    or a
    jr nz, _rchk_record_fail_tail
    ld (_g_rchk_fail_addr), hl
    ex af, af'
    ld (_g_rchk_read_value), a
_rchk_record_fail_tail:
    ld a, (_g_rchk_fail_count)
    inc a
    ld (_g_rchk_fail_count), a
    ret

_rchk_exec_stub_end:
