;; src/common/ram.s
.module ram

.globl _ram_exec_run
.globl _ram_exec_return

.globl _g_ram_exec_src
.globl _g_ram_exec_len
.globl _g_ram_exec_addr

.area _CODE

_ram_exec_run::
    push ix
    ld ix, (_g_ram_exec_src)
    ld hl, (_g_ram_exec_addr)
    ld de, (_g_ram_exec_len)

_ram_copy_loop:
    ld a, d
    or e
    jr z, _ram_copy_done
    ld a, (ix)
    ld (hl), a
    inc ix
    inc hl
    dec de
    jr _ram_copy_loop

_ram_copy_done:
    ld hl, (_g_ram_exec_addr)
    jp (hl)

_ram_exec_return::
    pop ix
    ret

.area _DATA

_g_ram_exec_src::
    .ds 2
_g_ram_exec_len::
    .ds 2
_g_ram_exec_addr::
    .ds 2
