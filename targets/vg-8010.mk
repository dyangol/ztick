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
BOOT_AUTOSTART = xsh:2 b:1 c:3
BOOT_AUTOSTART_STRICT = 1

# RCHK (RAM check) memory-switch probe configuration (target-defined, not user-defined).
# VG-8010 internal RAM is in slot 0 page 3. (Slot 0 page 2 is also internal
# RAM, but RCHK_EXEC_ADDR/RCHK_SAFE_SP below currently live in page 2's
# external RAM, so it can't be added to this sweep without relocating them
# first.)
# page:slot:allowed_start:allowed_end:offset:length, one entry per internal-RAM page to sweep.
RCHK_TESTS = 3:0:0x0000:0x3FFF:0x0000:0x4000
RCHK_VALUE = 0xA5
RCHK_SAFE_MODE = safe
RCHK_SAFE_SP = 0xBFF0
RCHK_EXEC_ADDR = 0x8100
