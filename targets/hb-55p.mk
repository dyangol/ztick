# Target manifest: Sony HB-55P
TARGET_NAME = Sony HB-55P
OPENMSX_MACHINE_NAME = Sony_HB-55P
OPENMSX_MACHINE_XML = Sony_HB-55P.xml
ROM_IMAGE_NAME = msx-sony-hb55p_bootstrap.rom
IMAGE_LAYOUT = flash2x64
GEN_COMPACT_IMAGE = yes

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
# HB-55P only has native RAM in slot 0 page 3 (pages 0-2 are ROM) -- there's
# no other internal-RAM page to add to this sweep.
# page:slot:allowed_start:allowed_end:offset:length, one entry per internal-RAM page to sweep.
RCHK_TESTS = 3:0:0x0000:0x3FFF:0x0000:0x4000
RCHK_VALUE = 0xA5
RCHK_SAFE_MODE = safe
RCHK_SAFE_SP = 0xBFF0
RCHK_EXEC_ADDR = 0x8100
