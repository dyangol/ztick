# --- Toolchain Configuration ---
CC = sdcc
AS = sdasz80
FAMILY = -mz80
TARGET ?= ztick
TARGET_MANIFEST = targets/$(TARGET).mk

include $(TARGET_MANIFEST)

IMAGE_LAYOUT ?= flat64
GEN_COMPACT_IMAGE ?= no
BOOT_AUTOSTART ?=
BOOT_AUTOSTART_STRICT ?= 1

SRC_DIR        = src
BUILD_ROOT_DIR = build
BIN_ROOT_DIR   = bin
BUILD_DIR      = $(BUILD_ROOT_DIR)/$(TARGET)
BIN_DIR        = $(BIN_ROOT_DIR)/$(TARGET)

ROM_BANK_SIZE = 65536

GROUPS = bootstrap
FSM_OUT_DIR ?= zbridge/build
FSM_OUT_BIN ?= $(FSM_OUT_DIR)/zbridge_fsm.bin
FSM_OUT_HEX ?= $(FSM_OUT_DIR)/zbridge_fsm.hex
FSM_OUT_LOGISIM ?= $(FSM_OUT_DIR)/zbridge_fsm.img

all: setup $(GROUPS)

setup:
	@mkdir -p $(BIN_DIR) $(BUILD_DIR)

COMMON_REL_OBJECTS = \
	$(BUILD_DIR)/rtos_asm.rel \
	$(BUILD_DIR)/vdp.rel \
	$(BUILD_DIR)/io_asm.rel \
	$(BUILD_DIR)/ram.rel \
	$(BUILD_DIR)/heap.rel \
	$(BUILD_DIR)/ipc.rel \
	$(BUILD_DIR)/zbus.rel \
	$(BUILD_DIR)/zlink.rel \
	$(BUILD_DIR)/activity_indicator.rel \
	$(BUILD_DIR)/ipc_demo.rel \
	$(BUILD_DIR)/rchk.rel \
	$(BUILD_DIR)/task.rel \
	$(BUILD_DIR)/args_b.rel \
	$(BUILD_DIR)/args_c.rel \
	$(BUILD_DIR)/args_rchk.rel \
	$(BUILD_DIR)/args.rel \
	$(BUILD_DIR)/pipe.rel \
	$(BUILD_DIR)/sprint.rel \
	$(BUILD_DIR)/xsh.rel \
	$(BUILD_DIR)/xsh_cmd.rel \
	$(BUILD_DIR)/boot.rel \
	$(BUILD_DIR)/rtos.rel \
	$(BUILD_DIR)/vdp_c.rel \
	$(BUILD_DIR)/io.rel \
	$(BUILD_DIR)/rchk_asm.rel \
	$(BUILD_DIR)/main_xsh.rel \
	$(BUILD_DIR)/main_b.rel \
	$(BUILD_DIR)/main_c.rel \
	$(BUILD_DIR)/main_rchk.rel

$(BUILD_DIR)/target_boot.inc: setup
	@printf ";; Auto-generated from %s\n" "$(TARGET_MANIFEST)" > $@
	@printf "PPI_CTRL_PORT = %s\n" "$(PPI_CTRL_PORT)" >> $@
	@printf "PPI_PSR_PORT = %s\n" "$(PPI_PSR_PORT)" >> $@
	@printf "PPI_CTRL_VALUE = %s\n" "$(PPI_CTRL_VALUE)" >> $@
	@printf "BOOT_PSR_VALUE = %s\n" "$(BOOT_PSR_VALUE)" >> $@
	@printf "BOOT_MARKER_VALUE = %s\n" "$(BOOT_MARKER_VALUE)" >> $@

$(BUILD_DIR)/target_autostart.h: setup
	@count=0; \
	raw_list="$(strip $(BOOT_AUTOSTART))"; \
	raw_cfg="$$raw_list"; \
	if [ -z "$$raw_cfg" ]; then raw_cfg="-"; fi; \
	printf "/* Auto-generated from %s */\n" "$(TARGET_MANIFEST)" > $@; \
	printf "#ifndef TARGET_AUTOSTART_H\n#define TARGET_AUTOSTART_H\n\n" >> $@; \
	printf "#include <stdint.h>\n\n" >> $@; \
	printf "typedef struct target_autostart_item {\n" >> $@; \
	printf "    const uint8_t *name;\n" >> $@; \
	printf "    uint8_t weight;\n" >> $@; \
	printf "} target_autostart_item_t;\n\n" >> $@; \
	printf "#define TARGET_AUTOSTART_RAW \"%s\"\n" "$$raw_cfg" >> $@; \
	printf "#define TARGET_AUTOSTART_STRICT %su\n\n" "$(BOOT_AUTOSTART_STRICT)" >> $@; \
	for item in $$raw_list; do \
		name=$${item%%:*}; \
		weight=$${item##*:}; \
		if [ -z "$$name" ] || [ -z "$$weight" ] || [ "$$name" = "$$item" ]; then \
			echo "Invalid BOOT_AUTOSTART entry '$$item' (expected name:weight)" >&2; \
			exit 1; \
		fi; \
		printf "static const uint8_t g_target_autostart_name_%s[] = \"%s\";\n" "$$count" "$$name" >> $@; \
		count=$$((count + 1)); \
	done; \
	printf "\n#define TARGET_AUTOSTART_COUNT %su\n" "$$count" >> $@; \
	if [ "$$count" -gt 0 ]; then \
		printf "static const target_autostart_item_t g_target_autostart[TARGET_AUTOSTART_COUNT] = {\n" >> $@; \
		idx=0; \
		for item in $$raw_list; do \
			weight=$${item##*:}; \
			printf "    {g_target_autostart_name_%s, %su},\n" "$$idx" "$$weight" >> $@; \
			idx=$$((idx + 1)); \
		done; \
		printf "};\n\n" >> $@; \
	fi; \
	printf "#endif\n" >> $@

$(BUILD_DIR)/target_rchk.h: setup
	@rchk_page="$(strip $(RCHK_PAGE))"; \
	rchk_slot="$(strip $(RCHK_SLOT))"; \
	rchk_allowed_start="$(strip $(RCHK_ALLOWED_START))"; \
	rchk_allowed_end="$(strip $(RCHK_ALLOWED_END))"; \
	rchk_offset="$(strip $(RCHK_OFFSET))"; \
	rchk_length="$(strip $(RCHK_LENGTH))"; \
	rchk_value="$(strip $(RCHK_VALUE))"; \
	rchk_safe_mode_raw="$(strip $(RCHK_SAFE_MODE))"; \
	rchk_safe_sp="$(strip $(RCHK_SAFE_SP))"; \
	rchk_exec_addr="$(strip $(RCHK_EXEC_ADDR))"; \
	ppi_psr_port="$(strip $(PPI_PSR_PORT))"; \
	case "$$rchk_safe_mode_raw" in \
		safe) rchk_safe_mode=1 ;; \
		unsafe) rchk_safe_mode=0 ;; \
		*) echo "RCHK_SAFE_MODE must be 'safe' or 'unsafe' in $(TARGET_MANIFEST)" >&2; exit 1 ;; \
	esac; \
	if [ -z "$$rchk_page" ] || [ -z "$$rchk_slot" ] || [ -z "$$rchk_allowed_start" ] || \
	   [ -z "$$rchk_allowed_end" ] || [ -z "$$rchk_offset" ] || [ -z "$$rchk_length" ] || \
	   [ -z "$$rchk_value" ] || [ -z "$$rchk_safe_mode_raw" ] || [ -z "$$rchk_safe_sp" ] || \
	   [ -z "$$rchk_exec_addr" ] || [ -z "$$ppi_psr_port" ]; then \
		echo "Missing RCHK_* or PPI_PSR_PORT in $(TARGET_MANIFEST)" >&2; \
		exit 1; \
	fi; \
	printf "/* Auto-generated from %s */\n" "$(TARGET_MANIFEST)" > $@; \
	printf "#ifndef TARGET_RCHK_H\n#define TARGET_RCHK_H\n\n" >> $@; \
	printf "#define RCHK_PAGE %su\n" "$$rchk_page" >> $@; \
	printf "#define RCHK_SLOT %su\n" "$$rchk_slot" >> $@; \
	printf "#define RCHK_ALLOWED_START %s\n" "$$rchk_allowed_start" >> $@; \
	printf "#define RCHK_ALLOWED_END %s\n" "$$rchk_allowed_end" >> $@; \
	printf "#define RCHK_OFFSET %s\n" "$$rchk_offset" >> $@; \
	printf "#define RCHK_LENGTH %s\n" "$$rchk_length" >> $@; \
	printf "#define RCHK_VALUE %s\n" "$$rchk_value" >> $@; \
	printf "#define RCHK_SAFE_MODE %su\n" "$$rchk_safe_mode" >> $@; \
	printf "#define RCHK_SAFE_SP %s\n" "$$rchk_safe_sp" >> $@; \
	printf "#define RCHK_EXEC_ADDR %s\n" "$$rchk_exec_addr" >> $@; \
	printf "#define PPI_PSR_PORT %s\n\n" "$$ppi_psr_port" >> $@; \
	printf "#endif\n" >> $@

bootstrap: $(BUILD_DIR)/target_boot.inc $(BUILD_DIR)/target_autostart.h $(BUILD_DIR)/target_rchk.h
ifeq ($(IMAGE_LAYOUT),flash2x64)
	@echo ">> Building firmware: startup (flash2x64 compatibility image) [TARGET=$(TARGET)]"
	$(AS) -I$(BUILD_DIR) -o $(BUILD_DIR)/startup.rel $(SRC_DIR)/bootstrap/startup.s
	$(AS) -I$(BUILD_DIR) -o $(BUILD_DIR)/rtos_asm.rel $(SRC_DIR)/bootstrap/rtos.s
	$(AS) -I$(BUILD_DIR) -o $(BUILD_DIR)/vdp.rel $(SRC_DIR)/drivers/vdp.s
	$(AS) -I$(BUILD_DIR) -o $(BUILD_DIR)/io_asm.rel $(SRC_DIR)/drivers/io.s
	$(AS) -o $(BUILD_DIR)/ram.rel $(SRC_DIR)/common/ram.s
	$(AS) -o $(BUILD_DIR)/rchk_asm.rel $(SRC_DIR)/rchk/rchk.s
	@for file in $(SRC_DIR)/bootstrap/*.c $(SRC_DIR)/drivers/*.c $(SRC_DIR)/lib/*.c $(SRC_DIR)/xsh/*.c $(SRC_DIR)/task_b/*.c $(SRC_DIR)/task_c/*.c $(SRC_DIR)/rchk/*.c; do \
		fname=$$(basename $$file .c); \
		if [ "$$fname" = "vdp" ]; then out="vdp_c"; else out="$$fname"; fi; \
		echo "   CC $$file"; \
		$(CC) $(FAMILY) -I$(SRC_DIR)/common -I$(BUILD_DIR) -DIO_DEFAULT_PORT=$(IO_DEFAULT_PORT) -c $$file -o $(BUILD_DIR)/$$out.rel; \
	done
	$(CC) $(FAMILY) --no-std-crt0 \
		-Wl-b_CODE=$(ADDR_CODE) \
		--data-loc $(ADDR_DATA) \
		-Wl-g__STACK_START=$(ADDR_STACK) \
		$(BUILD_DIR)/startup.rel \
		$(COMMON_REL_OBJECTS) \
		-o $(BIN_DIR)/startup.ihx
	makebin -s $(ROM_BANK_SIZE) $(BIN_DIR)/startup.ihx $(BIN_DIR)/startup.rom
	cat $(BIN_DIR)/startup.rom $(BIN_DIR)/startup.rom > $(BIN_DIR)/$(ROM_IMAGE_NAME)
ifeq ($(GEN_COMPACT_IMAGE),yes)
	head -c 32768 $(BIN_DIR)/startup.rom > $(BIN_DIR)/startup_slot01.rom
	@echo ">> Success: compact 32KB image generated (startup_slot01.rom)"
endif
	@echo ">> Success: startup image generated"
else
	@echo ">> Building firmware: bootstrap (single 64KB image) [TARGET=$(TARGET)]"
	$(AS) -I$(BUILD_DIR) -o $(BUILD_DIR)/startup.rel $(SRC_DIR)/bootstrap/startup.s
	$(AS) -I$(BUILD_DIR) -o $(BUILD_DIR)/rtos_asm.rel $(SRC_DIR)/bootstrap/rtos.s
	$(AS) -I$(BUILD_DIR) -o $(BUILD_DIR)/vdp.rel $(SRC_DIR)/drivers/vdp.s
	$(AS) -I$(BUILD_DIR) -o $(BUILD_DIR)/io_asm.rel $(SRC_DIR)/drivers/io.s
	$(AS) -o $(BUILD_DIR)/ram.rel $(SRC_DIR)/common/ram.s
	$(AS) -o $(BUILD_DIR)/rchk_asm.rel $(SRC_DIR)/rchk/rchk.s
	@for file in $(SRC_DIR)/bootstrap/*.c $(SRC_DIR)/drivers/*.c $(SRC_DIR)/lib/*.c $(SRC_DIR)/xsh/*.c $(SRC_DIR)/task_b/*.c $(SRC_DIR)/task_c/*.c $(SRC_DIR)/rchk/*.c; do \
		fname=$$(basename $$file .c); \
		if [ "$$fname" = "vdp" ]; then out="vdp_c"; else out="$$fname"; fi; \
		echo "   CC $$file"; \
		$(CC) $(FAMILY) -I$(SRC_DIR)/common -I$(BUILD_DIR) -DIO_DEFAULT_PORT=$(IO_DEFAULT_PORT) -c $$file -o $(BUILD_DIR)/$$out.rel; \
	done
	$(CC) $(FAMILY) --no-std-crt0 \
		-Wl-b_CODE=$(ADDR_CODE) \
		--data-loc $(ADDR_DATA) \
		-Wl-g__STACK_START=$(ADDR_STACK) \
		$(BUILD_DIR)/startup.rel \
		$(COMMON_REL_OBJECTS) \
		-o $(BIN_DIR)/bootstrap.ihx
	makebin -s $(ROM_BANK_SIZE) $(BIN_DIR)/bootstrap.ihx $(BIN_DIR)/$(ROM_IMAGE_NAME)
	@echo ">> Success: single-bank image generated"
endif

clean:
	@echo "Cleaning build and binary directories..."
	rm -rf $(BUILD_ROOT_DIR) $(BIN_ROOT_DIR)

fsm:
	@echo ">> Building zbridge FSM ROM image for target $(TARGET)"
	@mkdir -p $(FSM_OUT_DIR)
	python3 zbridge/zbrc.py \
		--target $(TARGET) \
		-o $(FSM_OUT_BIN) \
		--out-hex $(FSM_OUT_HEX) \
		--out-logisim $(FSM_OUT_LOGISIM)
	@echo ">> Success: FSM images written to $(FSM_OUT_DIR)"

.PHONY: all setup clean fsm $(GROUPS)
