;; src/bootstrap/startup.s
.include "target_boot.inc"
.area _HEADER (ABS)
.globl __STACK_START
.globl _main_boot
.globl _z80_tick_isr
.globl _g_boot_marker
.globl _g_boot_slot_value
.globl s__DATA
.globl l__DATA

.org 0x0000
_reset_vector:
    di
    im 1
    ;; Optionally configure PPI/PSR here (single-image targets).
    ;; flash2x64 targets typically do this in bootloader.s.
.if STARTUP_CONFIGURE_PSR
    ld a, #PPI_CTRL_VALUE
    out (PPI_CTRL_PORT), a
    ld a, #BOOT_PSR_TRANSITION
    out (PPI_PSR_PORT), a
    ld a, #BOOT_PSR_VALUE
    out (PPI_PSR_PORT), a
.endif
    ld sp, #__STACK_START
    ;; Clear global DATA/BSS so C globals start from a deterministic zero state.
    ld hl, #s__DATA
    ld de, #(s__DATA + 1)
    ld bc, #l__DATA
    ld a, b
    or c
    jr z, _bss_clear_done
    xor a
    ld (hl), a
    dec bc
    ld a, b
    or c
    jr z, _bss_clear_done
    ldir
_bss_clear_done:
    ld a, #BOOT_MARKER_VALUE
    ld (_g_boot_marker), a
    ld a, #BOOT_PSR_VALUE
    ld (_g_boot_slot_value), a
    jp _main_boot

.org 0x0038
_im1_vector:
    jp _z80_tick_isr
