;; src/lib/mem_probe.s
.module mem_probe

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
.globl _g_slot_probe_base_addr
.globl _g_slot_probe_safe_sp
.globl _g_slot_probe_exec_addr
.globl _g_slot_probe_saved_sp
.globl _g_slot_probe_fail
.globl _g_slot_probe_fail_addr
.globl _g_slot_probe_read_value

.area _CODE

_slot_probe_run::
    ld hl, #_slot_probe_exec_stub
    ld (_g_ram_exec_src), hl
    ld hl, #(_slot_probe_exec_stub_end - _slot_probe_exec_stub)
    ld (_g_ram_exec_len), hl
    ld hl, (_g_slot_probe_exec_addr)
    ld (_g_ram_exec_addr), hl
    call _ram_exec_run
    ret

_slot_probe_exec_stub:
    di
    ld (_g_slot_probe_saved_sp), sp
    ld hl, (_g_slot_probe_safe_sp)
    ld sp, hl

    ld hl, (_g_slot_probe_base_addr)
    ld a, (_g_slot_probe_length)
    ld b, a
    ld a, (_g_slot_probe_value)
    ld d, a
    xor a
    ld e, a

    ld a, (_g_slot_probe_psr_port)
    ld c, a
    ld a, (_g_slot_probe_psr_new)
    out (c), a

_slot_probe_loop:
    ld a, b
    or a
    jr z, _slot_probe_restore_success
    ld a, d
    ld (hl), a
    ld a, (hl)
    cp d
    jr nz, _slot_probe_restore_fail
    inc hl
    djnz _slot_probe_loop
    jr _slot_probe_restore_success

_slot_probe_restore_fail:
    ld d, a
    ld e, #1

_slot_probe_restore:
    ld a, (_g_slot_probe_psr_old)
    out (c), a
    ld sp, (_g_slot_probe_saved_sp)
    ei

    ld a, e
    or a
    jr z, _slot_probe_store_success

    ld a, #1
    ld (_g_slot_probe_fail), a
    ld a, d
    ld (_g_slot_probe_read_value), a
    ld (_g_slot_probe_fail_addr), hl
    jp _ram_exec_return

_slot_probe_restore_success:
    xor a
    ld e, a
    jr _slot_probe_restore

_slot_probe_store_success:
    xor a
    ld (_g_slot_probe_fail), a
    ld a, (_g_slot_probe_value)
    ld (_g_slot_probe_read_value), a
    jp _ram_exec_return

_slot_probe_exec_stub_end:
