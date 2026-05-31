;; src/bootstrap/bootloader.s
.include "target_boot.inc"
.area _HEADER (ABS)
.globl __STACK_START

.org 0x0000
_reset_vector:
    di
    im 1
    ;; Configure the PPI before touching the primary slot register.
    ld a, #PPI_CTRL_VALUE
    out (PPI_CTRL_PORT), a
    ;; Transition mapping: keep fetch stable and expose control-card RAM.
    ld a, #BOOT_PSR_TRANSITION
    out (PPI_PSR_PORT), a
    ;; Stack is always on the control-card RAM region.
    ld sp, #__STACK_START
    ;; Build and execute a tiny trampoline in high RAM so changing the PSR
    ;; does not depend on instruction fetch from the old slot mapping.
    ld hl, #_slot_switch_stub
    ld de, #0xC000
    ld bc, #(_slot_switch_stub_end - _slot_switch_stub)
    ldir
    jp 0xC000

.org 0x0038
_im1_vector:
    reti

_slot_switch_stub:
    ld a, #BOOT_PSR_VALUE
    out (PPI_PSR_PORT), a
    ;; Startup bank selection (A16), when present, is handled by hardware.
    jp STARTUP_ENTRY
_slot_switch_stub_end:

.area _DATA
_bootloader_dummy_data::
    .ds 1
