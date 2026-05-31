# Target manifest: Sony HB-55P
TARGET_NAME = Sony HB-55P
OPENMSX_MACHINE_NAME = Sony_HB-55P
OPENMSX_MACHINE_XML = Sony_HB-55P.xml
ROM_IMAGE_NAME = msx-sony-hb55p_bootstrap.rom
IMAGE_LAYOUT = flash2x64

# Link/load memory layout
ADDR_CODE  = 0x0040
ADDR_DATA  = 0x8000
ADDR_STACK = 0xF7FF

# Default application I/O port for io_write()/zbus
IO_DEFAULT_PORT = 0x38

# Boot-time PPI/PSR configuration
PPI_CTRL_PORT = 0xAB
PPI_PSR_PORT = 0xA8
PPI_CTRL_VALUE = 0x82
BOOT_PSR_TRANSITION = 0x54
BOOT_PSR_VALUE = 0x55
BOOT_MARKER_VALUE = 0x5A
STARTUP_ENTRY = 0x0000

# Boot-time autostart list: <task_name>:<weight>
BOOT_AUTOSTART = xsh:2 b:1
BOOT_AUTOSTART_STRICT = 1

# Task D memory-switch probe configuration (target-defined, not user-defined).
# HB-55P internal RAM is in slot 0 page 3.
TASK_D_PAGE = 3
TASK_D_SLOT = 0
TASK_D_ALLOWED_START = 0x0100
TASK_D_ALLOWED_END = 0x3EFF
TASK_D_OFFSET = 0x0100
TASK_D_LENGTH = 64
TASK_D_VALUE = 0xA5
TASK_D_SAFE_MODE = safe
TASK_D_SAFE_SP = 0xDFF0
TASK_D_EXEC_ADDR = 0x8100
