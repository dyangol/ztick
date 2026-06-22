# Target manifest: Z-Tick UnitCard simulation
TARGET_NAME = Z-Tick UnitCard
OPENMSX_MACHINE_NAME = Z-Tick-UnitCard
OPENMSX_MACHINE_XML = Z-Tick-UnitCard.xml
ROM_IMAGE_NAME = ztick_unitcard_flash.rom
IMAGE_LAYOUT = flash2x64

# Link/load memory layout
ADDR_CODE  = 0x0040
ADDR_DATA  = 0x8000
ADDR_STACK = 0xF7FF

# Default application I/O port for io_write()/zbus
IO_DEFAULT_PORT = 0x3A

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
OPENMSX_EXTRA_ROM_FILES = bootloader_slot01.rom startup_slot01.rom

# RCHK (RAM check) memory-switch probe configuration (target-defined, not user-defined).
RCHK_PAGE = 2
RCHK_SLOT = 0
RCHK_ALLOWED_START = 0x0000
RCHK_ALLOWED_END = 0x3FFF
RCHK_OFFSET = 0x0000
RCHK_LENGTH = 0x4000
RCHK_VALUE = 0xA5
RCHK_SAFE_MODE = safe
RCHK_SAFE_SP = 0xF7F0
RCHK_EXEC_ADDR = 0xE000
