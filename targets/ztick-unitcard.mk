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
BOOT_PSR_TRANSITION = 0x50
BOOT_PSR_VALUE = 0x55
BOOT_MARKER_VALUE = 0x5A
STARTUP_ENTRY = 0x0000

# Boot-time autostart list: <task_name>:<weight>
BOOT_AUTOSTART = xsh:2 b:1
BOOT_AUTOSTART_STRICT = 1

# Additional ROM artifacts required by the openMSX machine profile.
OPENMSX_EXTRA_ROM_FILES = bootloader_slot01.rom startup_slot01.rom

# Task D memory-switch probe configuration (target-defined, not user-defined).
TASK_D_PAGE = 2
TASK_D_SLOT = 1
TASK_D_OFFSET = 0x0100
TASK_D_LENGTH = 64
TASK_D_VALUE = 0xA5
TASK_D_SAFE_SP = 0xF7F0
TASK_D_EXEC_ADDR = 0xE000
