# Target manifest: Z-Tick UnitCard simulation
TARGET_NAME = Z-Tick UnitCard
OPENMSX_MACHINE_NAME = Z-Tick-UnitCard
OPENMSX_MACHINE_XML = Z-Tick-UnitCard.xml
ROM_IMAGE_NAME = ztick_unitcard_flash.rom
IMAGE_LAYOUT = flash2x64
GEN_COMPACT_IMAGE = yes

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
BOOT_PSR_VALUE = 0x55
BOOT_MARKER_VALUE = 0x5A

# Boot-time autostart list: <task_name>:<weight>
BOOT_AUTOSTART = xsh:2 b:1 c:3
BOOT_AUTOSTART_STRICT = 1

# Additional ROM artifacts required by the openMSX machine profile.
OPENMSX_EXTRA_ROM_FILES = startup_slot01.rom

# RCHK (RAM check) memory-switch probe configuration (target-defined, not user-defined).
# page:slot:allowed_start:allowed_end:offset:length, one entry per internal-RAM page to sweep.
RCHK_TESTS = 2:0:0x0000:0x3FFF:0x0000:0x4000
RCHK_VALUE = 0xA5
RCHK_SAFE_MODE = safe
RCHK_SAFE_SP = 0xF7F0
RCHK_EXEC_ADDR = 0xE000
