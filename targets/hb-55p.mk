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
