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

_rchk_loop:
    ld a, b
    or a
    jr z, _rchk_restore_success

    ld a, e
    or a
    jr z, _rchk_fast

    ld a, (hl)
    push af
    ld a, d
    ld (hl), a
    ld a, (hl)
    cp d
    jr nz, _rchk_restore_fail_safe
    pop af
    ld (hl), a
    inc hl
    djnz _rchk_loop
    jr _rchk_restore_success

_rchk_fast:
    ; Unsafe mode: write/read verify only, no restore of original byte.
    ld a, d
    ld (hl), a
    ld a, (hl)
    cp d
    jr nz, _rchk_restore_fail_unsafe
    inc hl
    djnz _rchk_loop
    jr _rchk_restore_success

_rchk_restore_fail_safe:
    ld d, a
    pop af
    ld (hl), a
    ld a, d

_rchk_restore_fail_unsafe:
    ld d, a
    ld e, #1
    jr _rchk_restore

_rchk_restore:
    ; Always restore PSR + stack before publishing result and returning.
    pop af
    out (c), a
    ld sp, (_g_rchk_saved_sp)
    ei

    ld a, e
    or a
    jr z, _rchk_store_success

    ld a, #1
    ld (_g_rchk_fail), a
    ld a, d
    ld (_g_rchk_read_value), a
    ld (_g_rchk_fail_addr), hl
    jp _ram_exec_return

_rchk_restore_success:
    xor a
    ld e, a
    jr _rchk_restore

_rchk_store_success:
    xor a
    ld (_g_rchk_fail), a
    ld a, (_g_rchk_value)
    ld (_g_rchk_read_value), a
    jp _ram_exec_return

_rchk_exec_stub_end:
