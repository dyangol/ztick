# Target manifest: Z-Tick
TARGET_NAME = Z-Tick
OPENMSX_MACHINE_NAME = Z-Tick
OPENMSX_MACHINE_XML = Z-Tick.xml
ROM_IMAGE_NAME = ztick_bootstrap.rom
IMAGE_LAYOUT = flat64

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
BOOT_PSR_VALUE = 0x50
BOOT_MARKER_VALUE = 0x5A

# Boot-time autostart list: <task_name>:<weight>
BOOT_AUTOSTART = xsh:2 b:1
BOOT_AUTOSTART_STRICT = 1

# Task D memory-switch probe configuration (target-defined, not user-defined).
TASK_D_PAGE = 2
TASK_D_SLOT = 1
TASK_D_ALLOWED_START = 0x0100
TASK_D_ALLOWED_END = 0x3EFF
TASK_D_OFFSET = 0x0100
TASK_D_LENGTH = 64
TASK_D_VALUE = 0xA5
TASK_D_SAFE_MODE = safe
TASK_D_SAFE_SP = 0xF7F0
TASK_D_EXEC_ADDR = 0xE000
