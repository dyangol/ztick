;; src/bootstrap/vdp.s
.module vdp

.globl _vdp_write_register
.globl _vdp_set_write_address
.globl _vdp_write_data
.globl _vdp_set_read_address
.globl _vdp_read_data
.globl _io_write_port_raw
.globl _io_read_port_raw

.area _CODE

;; value in A, register number in L
_vdp_write_register::
    ld e, l
    ld l, #0x99
    call _io_write_port_raw
    ld a, e
    or #0x80
    ld l, #0x99
    call _io_write_port_raw
    ret

;; address in HL
_vdp_set_write_address::
    ld a, l
    ld l, #0x99
    call _io_write_port_raw
    ld a, h
    and #0x3F
    or #0x40
    ld l, #0x99
    call _io_write_port_raw
    ret

;; value in A
_vdp_write_data::
    ld l, #0x98
    call _io_write_port_raw
    ret

;; address in HL -- same as _vdp_set_write_address but bit 6 of the high
;; address byte stays 0 (read mode) instead of being set (write mode).
_vdp_set_read_address::
    ld a, l
    ld l, #0x99
    call _io_write_port_raw
    ld a, h
    and #0x3F
    ld l, #0x99
    call _io_write_port_raw
    ret

;; returns value in A
_vdp_read_data::
    ld a, #0x98
    call _io_read_port_raw
    ret
