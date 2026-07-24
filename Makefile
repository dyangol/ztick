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
	$(BUILD_DIR)/xsh_cmd_emit.rel \
	$(BUILD_DIR)/xsh_cmd_report.rel \
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

# RCHK_TESTS: space-separated list of "page:slot:allowed_start:allowed_end:offset:length"
# entries (same colon-separated-fields-in-a-list style as BOOT_AUTOSTART),
# one per internal-RAM page/slot to sweep. length=0 means "use the whole
# allowed range". Entries may target the page RCHK_EXEC_ADDR/RCHK_SAFE_SP
# live in: main_rchk() defers those to a second pass, after relocating the
# trampoline to RCHK_ALT_EXEC_ADDR/RCHK_ALT_SAFE_SP -- a *second*, always-
# trusted location on the trampoline's own home slot (never the slot under
# test), declared by the target the same way as the primary RCHK_EXEC_ADDR/
# RCHK_SAFE_SP. Required whenever RCHK_TESTS has an entry on that page, since
# that's the only way to free it up for testing.
$(BUILD_DIR)/target_rchk.h: setup
	@rchk_tests="$(strip $(RCHK_TESTS))"; \
	rchk_value="$(strip $(RCHK_VALUE))"; \
	rchk_safe_mode_raw="$(strip $(RCHK_SAFE_MODE))"; \
	rchk_safe_sp="$(strip $(RCHK_SAFE_SP))"; \
	rchk_exec_addr="$(strip $(RCHK_EXEC_ADDR))"; \
	rchk_alt_safe_sp="$(strip $(RCHK_ALT_SAFE_SP))"; \
	rchk_alt_exec_addr="$(strip $(RCHK_ALT_EXEC_ADDR))"; \
	ppi_psr_port="$(strip $(PPI_PSR_PORT))"; \
	case "$$rchk_safe_mode_raw" in \
		safe) rchk_safe_mode=1 ;; \
		unsafe) rchk_safe_mode=0 ;; \
		*) echo "RCHK_SAFE_MODE must be 'safe' or 'unsafe' in $(TARGET_MANIFEST)" >&2; exit 1 ;; \
	esac; \
	if [ -z "$$rchk_tests" ] || [ -z "$$rchk_value" ] || [ -z "$$rchk_safe_mode_raw" ] || \
	   [ -z "$$rchk_safe_sp" ] || [ -z "$$rchk_exec_addr" ] || [ -z "$$ppi_psr_port" ]; then \
		echo "Missing RCHK_TESTS/RCHK_* or PPI_PSR_PORT in $(TARGET_MANIFEST)" >&2; \
		exit 1; \
	fi; \
	safe_page=$$(( ($$rchk_exec_addr) >> 14 )); \
	deferred_seen=0; \
	for entry in $$rchk_tests; do \
		page=$$(echo "$$entry" | cut -d: -f1); \
		if [ "$$page" = "$$safe_page" ]; then deferred_seen=1; fi; \
	done; \
	if [ "$$deferred_seen" -eq 1 ]; then \
		if [ -z "$$rchk_alt_exec_addr" ] || [ -z "$$rchk_alt_safe_sp" ]; then \
			echo "RCHK_TESTS in $(TARGET_MANIFEST) has an entry on page $$safe_page (RCHK_EXEC_ADDR/RCHK_SAFE_SP's own page) but RCHK_ALT_EXEC_ADDR/RCHK_ALT_SAFE_SP aren't set -- need a second trusted trampoline location to free that page up for testing" >&2; \
			exit 1; \
		fi; \
		alt_safe_page=$$(( ($$rchk_alt_exec_addr) >> 14 )); \
		if [ "$$alt_safe_page" -eq "$$safe_page" ]; then \
			echo "RCHK_ALT_EXEC_ADDR in $(TARGET_MANIFEST) is on the same page ($$alt_safe_page) as RCHK_EXEC_ADDR -- it needs to be on a different page to be useful as a fallback" >&2; \
			exit 1; \
		fi; \
	fi; \
	printf "/* Auto-generated from %s */\n" "$(TARGET_MANIFEST)" > $@; \
	printf "#ifndef TARGET_RCHK_H\n#define TARGET_RCHK_H\n\n" >> $@; \
	printf "#include <stdint.h>\n\n" >> $@; \
	printf "typedef struct rchk_test_entry {\n" >> $@; \
	printf "    uint8_t page;\n    uint8_t slot;\n    uint16_t allowed_start;\n    uint16_t allowed_end;\n    uint16_t offset;\n    uint16_t length;\n" >> $@; \
	printf "} rchk_test_entry_t;\n\n" >> $@; \
	count=0; \
	for entry in $$rchk_tests; do \
		page=$$(echo "$$entry" | cut -d: -f1); \
		slot=$$(echo "$$entry" | cut -d: -f2); \
		astart=$$(echo "$$entry" | cut -d: -f3); \
		aend=$$(echo "$$entry" | cut -d: -f4); \
		off=$$(echo "$$entry" | cut -d: -f5); \
		len=$$(echo "$$entry" | cut -d: -f6); \
		if [ -z "$$page" ] || [ -z "$$slot" ] || [ -z "$$astart" ] || [ -z "$$aend" ] || \
		   [ -z "$$off" ] || [ -z "$$len" ] || [ "$$(echo "$$entry" | cut -d: -f7)" != "" ]; then \
			echo "Invalid RCHK_TESTS entry '$$entry' (expected page:slot:allowed_start:allowed_end:offset:length) in $(TARGET_MANIFEST)" >&2; \
			exit 1; \
		fi; \
		printf "#define RCHK_TEST_%s_PAGE %su\n" "$$count" "$$page" >> $@; \
		printf "#define RCHK_TEST_%s_SLOT %su\n" "$$count" "$$slot" >> $@; \
		printf "#define RCHK_TEST_%s_ALLOWED_START %s\n" "$$count" "$$astart" >> $@; \
		printf "#define RCHK_TEST_%s_ALLOWED_END %s\n" "$$count" "$$aend" >> $@; \
		printf "#define RCHK_TEST_%s_OFFSET %s\n" "$$count" "$$off" >> $@; \
		printf "#define RCHK_TEST_%s_LENGTH %s\n\n" "$$count" "$$len" >> $@; \
		count=$$((count + 1)); \
	done; \
	printf "#define RCHK_TEST_COUNT %su\n\n" "$$count" >> $@; \
	printf "static const rchk_test_entry_t g_rchk_tests[RCHK_TEST_COUNT] = {\n" >> $@; \
	idx=0; \
	while [ "$$idx" -lt "$$count" ]; do \
		printf "    { RCHK_TEST_%s_PAGE, RCHK_TEST_%s_SLOT, RCHK_TEST_%s_ALLOWED_START, RCHK_TEST_%s_ALLOWED_END, RCHK_TEST_%s_OFFSET, RCHK_TEST_%s_LENGTH },\n" \
			"$$idx" "$$idx" "$$idx" "$$idx" "$$idx" "$$idx" >> $@; \
		idx=$$((idx + 1)); \
	done; \
	printf "};\n\n" >> $@; \
	printf "#define RCHK_VALUE %s\n" "$$rchk_value" >> $@; \
	printf "#define RCHK_SAFE_MODE %su\n" "$$rchk_safe_mode" >> $@; \
	printf "#define RCHK_SAFE_SP %s\n" "$$rchk_safe_sp" >> $@; \
	printf "#define RCHK_EXEC_ADDR %s\n" "$$rchk_exec_addr" >> $@; \
	printf "#define RCHK_ALT_SAFE_SP %s\n" "$${rchk_alt_safe_sp:-0}" >> $@; \
	printf "#define RCHK_ALT_EXEC_ADDR %s\n" "$${rchk_alt_exec_addr:-0}" >> $@; \
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
		$(CC) $(FAMILY) -I$(SRC_DIR)/common -I$(BUILD_DIR) -DIO_DEFAULT_PORT=$(IO_DEFAULT_PORT) -DIO_RESET_PORT=$(IO_RESET_PORT) -c $$file -o $(BUILD_DIR)/$$out.rel; \
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
		$(CC) $(FAMILY) -I$(SRC_DIR)/common -I$(BUILD_DIR) -DIO_DEFAULT_PORT=$(IO_DEFAULT_PORT) -DIO_RESET_PORT=$(IO_RESET_PORT) -c $$file -o $(BUILD_DIR)/$$out.rel; \
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
