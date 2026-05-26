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
    ;; Startup bank selection (A16) is handled by hardware logic on the card.
    ;; No I/O port write is used for bank switching.
    ;; Final runtime PSR for the target.
    ld a, #BOOT_PSR_VALUE
    out (PPI_PSR_PORT), a
    jp STARTUP_ENTRY

.org 0x0038
_im1_vector:
    reti

.area _DATA
_bootloader_dummy_data::
    .ds 1
