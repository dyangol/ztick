# Target manifest: Sony HB-75P
TARGET_NAME = Sony HB-75P
OPENMSX_MACHINE_NAME = Sony_HB-75P
OPENMSX_MACHINE_XML = Sony_HB-75P.xml
ROM_IMAGE_NAME = msx-sony-hb75p_bootstrap.rom
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
BOOT_PSR_VALUE = 0x55
BOOT_MARKER_VALUE = 0x5A

# Boot-time autostart list: <task_name>:<weight>
BOOT_AUTOSTART = xsh:2 b:1 c:3
BOOT_AUTOSTART_STRICT = 1

# RCHK (RAM check) memory-switch probe configuration (target-defined, not user-defined).
# HB-75P internal RAM is in slot 2 page 3.
RCHK_PAGE = 3
RCHK_SLOT = 2
RCHK_ALLOWED_START = 0x0000
RCHK_ALLOWED_END = 0x3FFF
RCHK_OFFSET = 0x0000
RCHK_LENGTH = 0x4000
RCHK_VALUE = 0xA5
RCHK_SAFE_MODE = safe
RCHK_SAFE_SP = 0xDFF0
RCHK_EXEC_ADDR = 0x8100
