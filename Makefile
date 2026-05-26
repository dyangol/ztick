# --- Toolchain Configuration ---
CC = sdcc
AS = sdasz80
FAMILY = -mz80
TARGET ?= ztick
TARGET_MANIFEST = targets/$(TARGET).mk

include $(TARGET_MANIFEST)

IMAGE_LAYOUT ?= flat64
BOOT_PSR_TRANSITION ?= $(BOOT_PSR_VALUE)
STARTUP_ENTRY ?= 0x0000
ifeq ($(IMAGE_LAYOUT),flash2x64)
STARTUP_CONFIGURE_PSR_DEFAULT = 0
else
STARTUP_CONFIGURE_PSR_DEFAULT = 1
endif
STARTUP_CONFIGURE_PSR ?= $(STARTUP_CONFIGURE_PSR_DEFAULT)

SRC_DIR        = src
BUILD_ROOT_DIR = build
BIN_ROOT_DIR   = bin
BUILD_DIR      = $(BUILD_ROOT_DIR)/$(TARGET)
BIN_DIR        = $(BIN_ROOT_DIR)/$(TARGET)

ROM_BANK_SIZE = 65536

GROUPS = bootstrap
FSM_SPEC ?= scripts/fsm/examples/ft245_msx_bridge.json
FSM_OUT_DIR ?= build/fsm
FSM_OUT_BIN ?= $(FSM_OUT_DIR)/ft245_fsm.bin
FSM_OUT_HEX ?= $(FSM_OUT_DIR)/ft245_fsm.hex
FSM_OUT_LOGISIM ?= $(FSM_OUT_DIR)/ft245_fsm_logisim.mem
FSM_OUT_BIN_128K ?= $(FSM_OUT_DIR)/ft245_fsm_39sf010a.bin

all: setup $(GROUPS)

setup:
	@mkdir -p $(BIN_DIR) $(BUILD_DIR)

COMMON_REL_OBJECTS = \
	$(BUILD_DIR)/rtos_asm.rel \
	$(BUILD_DIR)/vdp.rel \
	$(BUILD_DIR)/io_asm.rel \
	$(BUILD_DIR)/heap.rel \
	$(BUILD_DIR)/ipc.rel \
	$(BUILD_DIR)/zbus.rel \
	$(BUILD_DIR)/zlink.rel \
	$(BUILD_DIR)/activity_indicator.rel \
	$(BUILD_DIR)/ipc_demo.rel \
	$(BUILD_DIR)/task.rel \
	$(BUILD_DIR)/pipe.rel \
	$(BUILD_DIR)/xsh.rel \
	$(BUILD_DIR)/xsh_cmd.rel \
	$(BUILD_DIR)/boot.rel \
	$(BUILD_DIR)/rtos.rel \
	$(BUILD_DIR)/vdp_c.rel \
	$(BUILD_DIR)/io.rel \
	$(BUILD_DIR)/main_shell.rel \
	$(BUILD_DIR)/main_b.rel \
	$(BUILD_DIR)/main_c.rel

$(BUILD_DIR)/target_boot.inc: setup
	@printf ";; Auto-generated from %s\n" "$(TARGET_MANIFEST)" > $@
	@printf "PPI_CTRL_PORT = %s\n" "$(PPI_CTRL_PORT)" >> $@
	@printf "PPI_PSR_PORT = %s\n" "$(PPI_PSR_PORT)" >> $@
	@printf "PPI_CTRL_VALUE = %s\n" "$(PPI_CTRL_VALUE)" >> $@
	@printf "BOOT_PSR_TRANSITION = %s\n" "$(BOOT_PSR_TRANSITION)" >> $@
	@printf "BOOT_PSR_VALUE = %s\n" "$(BOOT_PSR_VALUE)" >> $@
	@printf "BOOT_MARKER_VALUE = %s\n" "$(BOOT_MARKER_VALUE)" >> $@
	@printf "STARTUP_ENTRY = %s\n" "$(STARTUP_ENTRY)" >> $@
	@printf "STARTUP_CONFIGURE_PSR = %s\n" "$(STARTUP_CONFIGURE_PSR)" >> $@

bootstrap: $(BUILD_DIR)/target_boot.inc
ifeq ($(IMAGE_LAYOUT),flash2x64)
	@echo ">> Building firmware: bootloader+startup (128KB flash image) [TARGET=$(TARGET)]"
	$(AS) -I$(BUILD_DIR) -o $(BUILD_DIR)/bootloader.rel $(SRC_DIR)/bootstrap/bootloader.s
	$(AS) -I$(BUILD_DIR) -o $(BUILD_DIR)/startup_reset.rel $(SRC_DIR)/bootstrap/startup.s
	$(AS) -I$(BUILD_DIR) -o $(BUILD_DIR)/rtos_asm.rel $(SRC_DIR)/bootstrap/rtos.s
	$(AS) -I$(BUILD_DIR) -o $(BUILD_DIR)/vdp.rel $(SRC_DIR)/drivers/vdp.s
	$(AS) -I$(BUILD_DIR) -o $(BUILD_DIR)/io_asm.rel $(SRC_DIR)/drivers/io.s
	@for file in $(SRC_DIR)/bootstrap/*.c $(SRC_DIR)/drivers/*.c $(SRC_DIR)/lib/*.c $(SRC_DIR)/task_shell/*.c $(SRC_DIR)/task_b/*.c $(SRC_DIR)/task_c/*.c; do \
		fname=$$(basename $$file .c); \
		if [ "$$fname" = "vdp" ]; then out="vdp_c"; else out="$$fname"; fi; \
		echo "   CC $$file"; \
		$(CC) $(FAMILY) -I$(SRC_DIR)/common -DIO_DEFAULT_PORT=$(IO_DEFAULT_PORT) -c $$file -o $(BUILD_DIR)/$$out.rel; \
	done
	$(CC) $(FAMILY) --no-std-crt0 \
		-Wl-b_CODE=$(ADDR_CODE) \
		--data-loc $(ADDR_DATA) \
		-Wl-g__STACK_START=$(ADDR_STACK) \
		$(BUILD_DIR)/startup_reset.rel \
		$(COMMON_REL_OBJECTS) \
		-o $(BIN_DIR)/startup.ihx
	makebin -s $(ROM_BANK_SIZE) $(BIN_DIR)/startup.ihx $(BIN_DIR)/startup.rom
	$(CC) $(FAMILY) --no-std-crt0 \
		-Wl-b_CODE=0x0000 \
		-Wl-g__STACK_START=$(ADDR_STACK) \
		$(BUILD_DIR)/bootloader.rel \
		-o $(BIN_DIR)/bootloader.ihx
	makebin -s $(ROM_BANK_SIZE) $(BIN_DIR)/bootloader.ihx $(BIN_DIR)/bootloader.rom
	cat $(BIN_DIR)/bootloader.rom $(BIN_DIR)/startup.rom > $(BIN_DIR)/$(ROM_IMAGE_NAME)
	@echo ">> Success: bootloader bank + startup bank generated"
else
	@echo ">> Building firmware: bootstrap (single 64KB image) [TARGET=$(TARGET)]"
	$(AS) -I$(BUILD_DIR) -o $(BUILD_DIR)/startup_reset.rel $(SRC_DIR)/bootstrap/startup.s
	$(AS) -I$(BUILD_DIR) -o $(BUILD_DIR)/rtos_asm.rel $(SRC_DIR)/bootstrap/rtos.s
	$(AS) -I$(BUILD_DIR) -o $(BUILD_DIR)/vdp.rel $(SRC_DIR)/drivers/vdp.s
	$(AS) -I$(BUILD_DIR) -o $(BUILD_DIR)/io_asm.rel $(SRC_DIR)/drivers/io.s
	@for file in $(SRC_DIR)/bootstrap/*.c $(SRC_DIR)/drivers/*.c $(SRC_DIR)/lib/*.c $(SRC_DIR)/task_shell/*.c $(SRC_DIR)/task_b/*.c $(SRC_DIR)/task_c/*.c; do \
		fname=$$(basename $$file .c); \
		if [ "$$fname" = "vdp" ]; then out="vdp_c"; else out="$$fname"; fi; \
		echo "   CC $$file"; \
		$(CC) $(FAMILY) -I$(SRC_DIR)/common -DIO_DEFAULT_PORT=$(IO_DEFAULT_PORT) -c $$file -o $(BUILD_DIR)/$$out.rel; \
	done
	$(CC) $(FAMILY) --no-std-crt0 \
		-Wl-b_CODE=$(ADDR_CODE) \
		--data-loc $(ADDR_DATA) \
		-Wl-g__STACK_START=$(ADDR_STACK) \
		$(BUILD_DIR)/startup_reset.rel \
		$(COMMON_REL_OBJECTS) \
		-o $(BIN_DIR)/bootstrap.ihx
	makebin -s $(ROM_BANK_SIZE) $(BIN_DIR)/bootstrap.ihx $(BIN_DIR)/$(ROM_IMAGE_NAME)
	@echo ">> Success: single-bank image generated"
endif

clean:
	@echo "Cleaning build and binary directories..."
	rm -rf $(BUILD_ROOT_DIR) $(BIN_ROOT_DIR)

fsm:
	@echo ">> Building FSM ROM image from $(FSM_SPEC)"
	@mkdir -p $(FSM_OUT_DIR)
	python3 scripts/fsm/compile_fsm.py \
		$(FSM_SPEC) \
		--out-bin $(FSM_OUT_BIN) \
		--out-hex $(FSM_OUT_HEX) \
		--out-logisim $(FSM_OUT_LOGISIM) \
		--mirror-39sf010a \
		--out-bin-128k $(FSM_OUT_BIN_128K)
	@echo ">> Success: FSM images written to $(FSM_OUT_DIR)"

.PHONY: all setup clean fsm $(GROUPS)
