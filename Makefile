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
BOOT_AUTOSTART ?=
BOOT_AUTOSTART_STRICT ?= 1

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
	$(BUILD_DIR)/ram.rel \
	$(BUILD_DIR)/heap.rel \
	$(BUILD_DIR)/ipc.rel \
	$(BUILD_DIR)/zbus.rel \
	$(BUILD_DIR)/zlink.rel \
	$(BUILD_DIR)/activity_indicator.rel \
	$(BUILD_DIR)/ipc_demo.rel \
	$(BUILD_DIR)/mem_probe.rel \
	$(BUILD_DIR)/task.rel \
	$(BUILD_DIR)/args_b.rel \
	$(BUILD_DIR)/args_c.rel \
	$(BUILD_DIR)/args_d.rel \
	$(BUILD_DIR)/args.rel \
	$(BUILD_DIR)/pipe.rel \
	$(BUILD_DIR)/sprint.rel \
	$(BUILD_DIR)/xsh.rel \
	$(BUILD_DIR)/xsh_cmd.rel \
	$(BUILD_DIR)/boot.rel \
	$(BUILD_DIR)/rtos.rel \
	$(BUILD_DIR)/vdp_c.rel \
	$(BUILD_DIR)/io.rel \
	$(BUILD_DIR)/mem_probe_asm.rel \
	$(BUILD_DIR)/main_shell.rel \
	$(BUILD_DIR)/main_b.rel \
	$(BUILD_DIR)/main_c.rel \
	$(BUILD_DIR)/main_d.rel

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

$(BUILD_DIR)/target_task_d.h: setup
	@task_d_page="$(strip $(TASK_D_PAGE))"; \
	task_d_slot="$(strip $(TASK_D_SLOT))"; \
	task_d_allowed_start="$(strip $(TASK_D_ALLOWED_START))"; \
	task_d_allowed_end="$(strip $(TASK_D_ALLOWED_END))"; \
	task_d_offset="$(strip $(TASK_D_OFFSET))"; \
	task_d_length="$(strip $(TASK_D_LENGTH))"; \
	task_d_value="$(strip $(TASK_D_VALUE))"; \
	task_d_safe_mode_raw="$(strip $(TASK_D_SAFE_MODE))"; \
	task_d_safe_sp="$(strip $(TASK_D_SAFE_SP))"; \
	task_d_exec_addr="$(strip $(TASK_D_EXEC_ADDR))"; \
	ppi_psr_port="$(strip $(PPI_PSR_PORT))"; \
	case "$$task_d_safe_mode_raw" in \
		safe) task_d_safe_mode=1 ;; \
		unsafe) task_d_safe_mode=0 ;; \
		*) echo "TASK_D_SAFE_MODE must be 'safe' or 'unsafe' in $(TARGET_MANIFEST)" >&2; exit 1 ;; \
	esac; \
	if [ -z "$$task_d_page" ] || [ -z "$$task_d_slot" ] || [ -z "$$task_d_allowed_start" ] || \
	   [ -z "$$task_d_allowed_end" ] || [ -z "$$task_d_offset" ] || [ -z "$$task_d_length" ] || \
	   [ -z "$$task_d_value" ] || [ -z "$$task_d_safe_mode_raw" ] || [ -z "$$task_d_safe_sp" ] || \
	   [ -z "$$task_d_exec_addr" ] || [ -z "$$ppi_psr_port" ]; then \
		echo "Missing TASK_D_* or PPI_PSR_PORT in $(TARGET_MANIFEST)" >&2; \
		exit 1; \
	fi; \
	printf "/* Auto-generated from %s */\n" "$(TARGET_MANIFEST)" > $@; \
	printf "#ifndef TARGET_TASK_D_H\n#define TARGET_TASK_D_H\n\n" >> $@; \
	printf "#define TASK_D_PAGE %su\n" "$$task_d_page" >> $@; \
	printf "#define TASK_D_SLOT %su\n" "$$task_d_slot" >> $@; \
	printf "#define TASK_D_ALLOWED_START %s\n" "$$task_d_allowed_start" >> $@; \
	printf "#define TASK_D_ALLOWED_END %s\n" "$$task_d_allowed_end" >> $@; \
	printf "#define TASK_D_OFFSET %s\n" "$$task_d_offset" >> $@; \
	printf "#define TASK_D_LENGTH %su\n" "$$task_d_length" >> $@; \
	printf "#define TASK_D_VALUE %s\n" "$$task_d_value" >> $@; \
	printf "#define TASK_D_SAFE_MODE %su\n" "$$task_d_safe_mode" >> $@; \
	printf "#define TASK_D_SAFE_SP %s\n" "$$task_d_safe_sp" >> $@; \
	printf "#define TASK_D_EXEC_ADDR %s\n" "$$task_d_exec_addr" >> $@; \
	printf "#define PPI_PSR_PORT %s\n\n" "$$ppi_psr_port" >> $@; \
	printf "#endif\n" >> $@

bootstrap: $(BUILD_DIR)/target_boot.inc $(BUILD_DIR)/target_autostart.h $(BUILD_DIR)/target_task_d.h
ifeq ($(IMAGE_LAYOUT),flash2x64)
	@echo ">> Building firmware: bootloader+startup (128KB flash image) [TARGET=$(TARGET)]"
	$(AS) -I$(BUILD_DIR) -o $(BUILD_DIR)/bootloader.rel $(SRC_DIR)/bootstrap/bootloader.s
	$(AS) -I$(BUILD_DIR) -o $(BUILD_DIR)/startup_reset.rel $(SRC_DIR)/bootstrap/startup.s
	$(AS) -I$(BUILD_DIR) -o $(BUILD_DIR)/rtos_asm.rel $(SRC_DIR)/bootstrap/rtos.s
	$(AS) -I$(BUILD_DIR) -o $(BUILD_DIR)/vdp.rel $(SRC_DIR)/drivers/vdp.s
	$(AS) -I$(BUILD_DIR) -o $(BUILD_DIR)/io_asm.rel $(SRC_DIR)/drivers/io.s
	$(AS) -o $(BUILD_DIR)/ram.rel $(SRC_DIR)/common/ram.s
	$(AS) -o $(BUILD_DIR)/mem_probe_asm.rel $(SRC_DIR)/lib/mem_probe.s
	@for file in $(SRC_DIR)/bootstrap/*.c $(SRC_DIR)/drivers/*.c $(SRC_DIR)/lib/*.c $(SRC_DIR)/task_shell/*.c $(SRC_DIR)/task_b/*.c $(SRC_DIR)/task_c/*.c $(SRC_DIR)/task_d/*.c; do \
		fname=$$(basename $$file .c); \
		if [ "$$fname" = "vdp" ]; then out="vdp_c"; else out="$$fname"; fi; \
		echo "   CC $$file"; \
		$(CC) $(FAMILY) -I$(SRC_DIR)/common -I$(BUILD_DIR) -DIO_DEFAULT_PORT=$(IO_DEFAULT_PORT) -c $$file -o $(BUILD_DIR)/$$out.rel; \
	done
	$(CC) $(FAMILY) --no-std-crt0 \
		-Wl-b_CODE=$(ADDR_CODE) \
		--data-loc $(ADDR_DATA) \
		-Wl-g__STACK_START=$(ADDR_STACK) \
		$(BUILD_DIR)/startup_reset.rel \
		$(COMMON_REL_OBJECTS) \
		-o $(BIN_DIR)/startup.ihx
	makebin -s $(ROM_BANK_SIZE) $(BIN_DIR)/startup.ihx $(BIN_DIR)/startup.rom
	head -c 32768 $(BIN_DIR)/startup.rom > $(BIN_DIR)/startup_slot01.rom
	$(CC) $(FAMILY) --no-std-crt0 \
		-Wl-b_CODE=0x0000 \
		-Wl-g__STACK_START=$(ADDR_STACK) \
		$(BUILD_DIR)/bootloader.rel \
		-o $(BIN_DIR)/bootloader.ihx
	makebin -s $(ROM_BANK_SIZE) $(BIN_DIR)/bootloader.ihx $(BIN_DIR)/bootloader.rom
	head -c 32768 $(BIN_DIR)/bootloader.rom > $(BIN_DIR)/bootloader_slot01.rom
	cat $(BIN_DIR)/bootloader.rom $(BIN_DIR)/startup.rom > $(BIN_DIR)/$(ROM_IMAGE_NAME)
	@echo ">> Success: bootloader bank + startup bank generated"
else
	@echo ">> Building firmware: bootstrap (single 64KB image) [TARGET=$(TARGET)]"
	$(AS) -I$(BUILD_DIR) -o $(BUILD_DIR)/startup_reset.rel $(SRC_DIR)/bootstrap/startup.s
	$(AS) -I$(BUILD_DIR) -o $(BUILD_DIR)/rtos_asm.rel $(SRC_DIR)/bootstrap/rtos.s
	$(AS) -I$(BUILD_DIR) -o $(BUILD_DIR)/vdp.rel $(SRC_DIR)/drivers/vdp.s
	$(AS) -I$(BUILD_DIR) -o $(BUILD_DIR)/io_asm.rel $(SRC_DIR)/drivers/io.s
	$(AS) -o $(BUILD_DIR)/ram.rel $(SRC_DIR)/common/ram.s
	$(AS) -o $(BUILD_DIR)/mem_probe_asm.rel $(SRC_DIR)/lib/mem_probe.s
	@for file in $(SRC_DIR)/bootstrap/*.c $(SRC_DIR)/drivers/*.c $(SRC_DIR)/lib/*.c $(SRC_DIR)/task_shell/*.c $(SRC_DIR)/task_b/*.c $(SRC_DIR)/task_c/*.c $(SRC_DIR)/task_d/*.c; do \
		fname=$$(basename $$file .c); \
		if [ "$$fname" = "vdp" ]; then out="vdp_c"; else out="$$fname"; fi; \
		echo "   CC $$file"; \
		$(CC) $(FAMILY) -I$(SRC_DIR)/common -I$(BUILD_DIR) -DIO_DEFAULT_PORT=$(IO_DEFAULT_PORT) -c $$file -o $(BUILD_DIR)/$$out.rel; \
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
