# Target manifest: Philips VG-8010
TARGET_NAME = Philips VG-8010
OPENMSX_MACHINE_NAME = Philips_VG-8010
OPENMSX_MACHINE_XML = Philips_VG-8010.xml
ROM_IMAGE_NAME = msx-philips-vg8010_bootstrap.rom
IMAGE_LAYOUT = flash2x64
GEN_COMPACT_IMAGE = no

# Link/load memory layout
ADDR_CODE  = 0x0040
ADDR_DATA  = 0x8000
ADDR_STACK = 0xF7FF

# Default application I/O port for io_write()/zbus
IO_DEFAULT_PORT = 0x38

# MSX -> zbridge FT245 RESET# trigger port (see zbridge/zbrc.py)
IO_RESET_PORT = 0x35

# Boot-time PPI/PSR configuration
PPI_CTRL_PORT = 0xAB
PPI_PSR_PORT = 0xA8
PPI_CTRL_VALUE = 0x82
BOOT_PSR_VALUE = 0xAA
BOOT_MARKER_VALUE = 0x5A

# Boot-time autostart list: <task_name>:<weight>
BOOT_AUTOSTART = xsh:2
BOOT_AUTOSTART_STRICT = 1

# RCHK (RAM check) memory-switch probe configuration (target-defined, not user-defined).
# VG-8010 internal RAM (slot 0) covers pages 2 and 3, but only page 3 is
# swept here. A page-2 entry has reliably corrupted or hung the running
# system within a few chunks, across every reasonable trampoline placement
# tried: code+stack together on page 3 (slot 0 committed, slot 2
# uncommitted, as primary anchor or as a pass-2 relocation target), and code
# on page 3 with the stack deliberately left at its original page-2 address
# (0xBFF0, sweep narrowed to 0x0000-0x3FDF to protect it) -- that last one
# hung with no output at all, rather than the garbled-then-reboot pattern of
# the others, consistent with a corrupted psr_old landing in the PSR port
# and remapping page 0 (the running code) out from under itself. Page 3 has
# been remapped this way for a long time without issue; page 2 never had
# been until these attempts. This points to a hardware-level issue specific
# to page 2 (e.g. the expansion board's own decode/tri-state logic for that
# page, which until now had never been asked to release the bus) rather
# than anything fixable in software -- needs hardware-side investigation
# before a page-2 entry is retried here.
# page:slot:allowed_start:allowed_end:offset:length, one entry per internal-RAM page to sweep.
RCHK_TESTS = 3:0:0x0000:0x3FFF:0x0000:0x4000
RCHK_VALUE = 0xA5
RCHK_SAFE_MODE = safe
RCHK_SAFE_SP = 0xBFF0
RCHK_EXEC_ADDR = 0x8100
