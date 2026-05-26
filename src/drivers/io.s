;; src/bootstrap/io.s
.module io

.globl _io_write_port_raw
.globl _io_read_port_raw

.area _CODE

;; value in A, port in L
_io_write_port_raw::
    ld c, l
    ld b, #0x00
    out (c), a
    ret

;; port in A, returns value in A
_io_read_port_raw::
    ld c, a
    ld b, #0x00
    in a, (c)
    ret
