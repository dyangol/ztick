;; src/lib/mem_probe.s
.module mem_probe

.globl _mem_probe_run
.globl _slot_probe_run

.globl _ram_exec_run
.globl _ram_exec_return
.globl _g_ram_exec_src
.globl _g_ram_exec_len
.globl _g_ram_exec_addr

.globl _g_slot_probe_psr_old
.globl _g_slot_probe_psr_new
.globl _g_slot_probe_psr_port
.globl _g_slot_probe_length
.globl _g_slot_probe_value
.globl _g_slot_probe_safe_mode
.globl _g_slot_probe_base_addr
.globl _g_slot_probe_safe_sp
.globl _g_slot_probe_exec_addr
.globl _g_slot_probe_saved_sp
.globl _g_slot_probe_fail
.globl _g_slot_probe_fail_addr
.globl _g_slot_probe_read_value

.area _CODE

_mem_probe_run::
_slot_probe_run::
    ; Copy probe stub to the caller-provided RAM trampoline address and execute it.
    ld hl, #_mem_probe_exec_stub
    ld (_g_ram_exec_src), hl
    ld hl, #(_mem_probe_exec_stub_end - _mem_probe_exec_stub)
    ld (_g_ram_exec_len), hl
    ld hl, (_g_slot_probe_exec_addr)
    ld (_g_ram_exec_addr), hl
    call _ram_exec_run
    ret

_mem_probe_exec_stub:
    ; Critical section starts: preserve stack and remap target page via PSR.
    di
    ld (_g_slot_probe_saved_sp), sp
    ld hl, (_g_slot_probe_safe_sp)
    ld sp, hl

    ld hl, (_g_slot_probe_base_addr)
    ld a, (_g_slot_probe_length)
    ld b, a
    ld a, (_g_slot_probe_value)
    ld d, a
    ld a, (_g_slot_probe_safe_mode)
    ld e, a

    ld a, (_g_slot_probe_psr_port)
    ld c, a
    ld a, (_g_slot_probe_psr_old)
    push af
    ld a, (_g_slot_probe_psr_new)
    out (c), a

_mem_probe_loop:
    ld a, b
    or a
    jr z, _mem_probe_restore_success

    ld a, e
    or a
    jr z, _mem_probe_fast

    ld a, (hl)
    push af
    ld a, d
    ld (hl), a
    ld a, (hl)
    cp d
    jr nz, _mem_probe_restore_fail_safe
    pop af
    ld (hl), a
    inc hl
    djnz _mem_probe_loop
    jr _mem_probe_restore_success

_mem_probe_fast:
    ; Unsafe mode: write/read verify only, no restore of original byte.
    ld a, d
    ld (hl), a
    ld a, (hl)
    cp d
    jr nz, _mem_probe_restore_fail_unsafe
    inc hl
    djnz _mem_probe_loop
    jr _mem_probe_restore_success

_mem_probe_restore_fail_safe:
    ld d, a
    pop af
    ld (hl), a
    ld a, d

_mem_probe_restore_fail_unsafe:
    ld d, a
    ld e, #1
    jr _mem_probe_restore

_mem_probe_restore:
    ; Always restore PSR + stack before publishing result and returning.
    pop af
    out (c), a
    ld sp, (_g_slot_probe_saved_sp)
    ei

    ld a, e
    or a
    jr z, _mem_probe_store_success

    ld a, #1
    ld (_g_slot_probe_fail), a
    ld a, d
    ld (_g_slot_probe_read_value), a
    ld (_g_slot_probe_fail_addr), hl
    jp _ram_exec_return

_mem_probe_restore_success:
    xor a
    ld e, a
    jr _mem_probe_restore

_mem_probe_store_success:
    xor a
    ld (_g_slot_probe_fail), a
    ld a, (_g_slot_probe_value)
    ld (_g_slot_probe_read_value), a
    jp _ram_exec_return

_mem_probe_exec_stub_end:
