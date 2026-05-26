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
