# Target manifest: Z-Tick
TARGET_NAME = Z-Tick
OPENMSX_MACHINE_NAME = Z-Tick
OPENMSX_MACHINE_XML = Z-Tick.xml
ROM_IMAGE_NAME = ztick_bootstrap.rom
IMAGE_LAYOUT = flat64
GEN_COMPACT_IMAGE = no

# Link/load memory layout
ADDR_CODE  = 0x0040
ADDR_DATA  = 0x8000
ADDR_STACK = 0xF7FF

# Default application I/O port for io_write()/zbus
IO_DEFAULT_PORT = 0x3A

# MSX -> zbridge FT245 RESET# trigger port (see zbridge/zbrc.py)
IO_RESET_PORT = 0x35

# Boot-time PPI/PSR configuration
PPI_CTRL_PORT = 0xAB
PPI_PSR_PORT = 0xA8
PPI_CTRL_VALUE = 0x82
BOOT_PSR_VALUE = 0x50
BOOT_MARKER_VALUE = 0x5A

# Boot-time autostart list: <task_name>:<weight>
BOOT_AUTOSTART = xsh:2 b:1 c:3
BOOT_AUTOSTART_STRICT = 1

# RCHK (RAM check) memory-switch probe configuration (target-defined, not user-defined).
# page:slot:allowed_start:allowed_end:offset:length, one entry per internal-RAM page to sweep.
RCHK_TESTS = 2:1:0x0100:0x3EFF:0x0100:0x0040
RCHK_VALUE = 0xA5
RCHK_SAFE_MODE = safe
RCHK_SAFE_SP = 0xF7F0
RCHK_EXEC_ADDR = 0xE000
