PROJECT := stm32-hello
BUILD_DIR := build
TARGET_ELF := $(BUILD_DIR)/$(PROJECT).elf
TARGET_BIN := $(BUILD_DIR)/$(PROJECT).bin
TARGET_MAP := $(BUILD_DIR)/$(PROJECT).map

CC = arm-none-eabi-gcc
OBJCOPY = arm-none-eabi-objcopy
OPENOCD ?= openocd
OPENOCD_INTERFACE ?= interface/stlink.cfg
OPENOCD_TARGET ?= target/stm32f1x.cfg

CPU_FLAGS := -mcpu=cortex-m3 -mthumb
CFLAGS := -std=c11 -Wall -Wextra -Werror -Os -ffreestanding -fdata-sections -ffunction-sections $(CPU_FLAGS)
LDFLAGS := $(CPU_FLAGS) -nostdlib -Wl,--gc-sections -Wl,-Map,$(TARGET_MAP) -T linker.ld

ASM_SRCS := startup_stm32f1xx.s
C_SRCS := main.c
OBJS := $(ASM_SRCS:%.s=$(BUILD_DIR)/%.o) $(C_SRCS:%.c=$(BUILD_DIR)/%.o)

.PHONY: all flash flash-bin debug reset clean

REQUIRED_TOOLS := arm-none-eabi-gcc arm-none-eabi-objcopy openocd

check-tools:
	@set -e; \
	for tool in $(REQUIRED_TOOLS); do \
		if ! command -v $$tool >/dev/null 2>&1; then \
			echo "Missing tool: $$tool"; \
			echo "Install an ARM bare-metal toolchain, for example: gcc-arm-none-eabi binutils-arm-none-eabi"; \
			exit 1; \
		fi; \
	done

all: check-tools $(TARGET_ELF) $(TARGET_BIN)

flash: check-tools $(TARGET_ELF)
	$(OPENOCD) -f $(OPENOCD_INTERFACE) -f $(OPENOCD_TARGET) -c "program $(abspath $(TARGET_ELF)) verify reset exit"

flash-bin: check-tools $(TARGET_BIN)
	$(OPENOCD) -f $(OPENOCD_INTERFACE) -f $(OPENOCD_TARGET) -c "program $(abspath $(TARGET_BIN)) 0x08000000 verify reset exit"

debug:
	$(OPENOCD) -f $(OPENOCD_INTERFACE) -f $(OPENOCD_TARGET)

reset:
	$(OPENOCD) -f $(OPENOCD_INTERFACE) -f $(OPENOCD_TARGET) -c "init; reset run; exit"

$(TARGET_ELF): $(OBJS) linker.ld | $(BUILD_DIR)
	$(CC) $(OBJS) $(LDFLAGS) -o $@

$(TARGET_BIN): $(TARGET_ELF)
	$(OBJCOPY) -O binary $< $@

$(BUILD_DIR):
	mkdir -p $@

$(BUILD_DIR)/%.o: %.s | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: %.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR)
