;; src/bootstrap/rtos.s
.module rtos

.globl _g_current_tcb
.globl _g_current_task
.globl _g_tasks
.globl _g_kernel_sp
.globl _scheduler_select_next
.globl _zbus_tick
.globl _timer_acknowledge
.globl _rtos_start_impl
.globl _z80_tick_isr

.area _CODE

_z80_tick_isr::
    di

    push af
    push bc
    push de
    push hl
    push ix
    push iy

    ex af, af'
    exx
    push af
    push bc
    push de
    push hl

    ld a, (_g_current_task)
    ld e, a
    ld d, #0x00
    ld l, e
    ld h, d
    add hl, hl
    add hl, hl
    ld b, h
    ld c, l
    add hl, hl
    add hl, hl
    add hl, bc
    add hl, bc
    ld de, #_g_tasks
    add hl, de
    ld (_g_current_tcb), hl
    ex de, hl
    ld hl, #0x0000
    add hl, sp
    ld a, l
    ld (de), a
    inc de
    ld a, h
    ld (de), a

    ld hl, (_g_kernel_sp)
    ld sp, hl

    call _timer_acknowledge
    call _zbus_tick
    call _scheduler_select_next

    ld a, (_g_current_task)
    ld e, a
    ld d, #0x00
    ld l, e
    ld h, d
    add hl, hl
    add hl, hl
    ld b, h
    ld c, l
    add hl, hl
    add hl, hl
    add hl, bc
    add hl, bc
    ld de, #_g_tasks
    add hl, de
    ld (_g_current_tcb), hl
    ld e, (hl)
    inc hl
    ld d, (hl)
    ex de, hl
    ld sp, hl

    pop hl
    pop de
    pop bc
    pop af
    exx
    ex af, af'

    pop iy
    pop ix
    pop hl
    pop de
    pop bc
    pop af

    ei
    reti

_rtos_start_impl::
    di
    ld a, (_g_current_task)
    ld e, a
    ld d, #0x00
    ld l, e
    ld h, d
    add hl, hl
    add hl, hl
    ld b, h
    ld c, l
    add hl, hl
    add hl, hl
    add hl, bc
    add hl, bc
    ld de, #_g_tasks
    add hl, de
    ld (_g_current_tcb), hl
    ld e, (hl)
    inc hl
    ld d, (hl)
    ex de, hl
    ld sp, hl

    pop hl
    pop de
    pop bc
    pop af
    exx
    ex af, af'

    pop iy
    pop ix
    pop hl
    pop de
    pop bc
    pop af

    ei
    ret
