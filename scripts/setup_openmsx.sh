#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
OPENMSX_HOME_DIR="${OPENMSX_HOME:-$HOME/.openMSX}"
SHARE_DIR="$OPENMSX_HOME_DIR/share"
MACHINES_DIR="$SHARE_DIR/machines"
OPENMSX_BIN="${OPENMSX_BIN:-openmsx}"
OPENMSX_SYSTEM_DATA_AUTO=""
OPENMSX_SYSTEM_DATA_LABEL=""

usage() {
    cat <<EOF
Usage: $0 [--target <name>] [--watch-io] [--bp-main-shell] [--bp-bootloader] [--self-check]
       $0 [<target>] [--watch-io] [--bp-main-shell] [--bp-bootloader] [--self-check]

Default target: ztick
Default watchpoints: disabled
Default shell entry breakpoint: disabled (`_main_shell`)
Default bootloader entry breakpoint: disabled (`0x0000`)
Default self-check: disabled
openMSX binary: \$OPENMSX_BIN (default: openmsx)
EOF
}

TARGET="ztick"
TARGET_SET=0
WATCH_IO=0
BP_MAIN_SHELL=0
BP_BOOTLOADER=0
SELF_CHECK=0
while [ "$#" -gt 0 ]; do
    case "$1" in
        -h|--help)
            usage
            exit 0
            ;;
        -t|--target)
            if [ "$#" -lt 2 ]; then
                echo "Missing value for $1" >&2
                usage >&2
                exit 1
            fi
            TARGET="$2"
            TARGET_SET=1
            shift 2
            ;;
        --target=*)
            TARGET="${1#*=}"
            TARGET_SET=1
            shift
            ;;
        --watch-io)
            WATCH_IO=1
            shift
            ;;
        --no-watch-io)
            WATCH_IO=0
            shift
            ;;
        --bp-main-shell)
            BP_MAIN_SHELL=1
            shift
            ;;
        --no-bp-main-shell)
            BP_MAIN_SHELL=0
            shift
            ;;
        --bp-bootloader)
            BP_BOOTLOADER=1
            shift
            ;;
        --no-bp-bootloader)
            BP_BOOTLOADER=0
            shift
            ;;
        --self-check)
            SELF_CHECK=1
            shift
            ;;
        --no-self-check)
            SELF_CHECK=0
            shift
            ;;
        -*)
            echo "Unknown option: $1" >&2
            usage >&2
            exit 1
            ;;
        *)
            if [ "$TARGET_SET" -ne 0 ]; then
                echo "Unexpected argument(s): $1" >&2
                usage >&2
                exit 1
            fi
            TARGET="$1"
            TARGET_SET=1
            shift
            ;;
    esac
done

TARGET_MANIFEST="$ROOT_DIR/targets/$TARGET.mk"
TARGET_BIN_DIR="$ROOT_DIR/bin/$TARGET"
SYMBOL_BASENAME="bootstrap"
IMAGE_LAYOUT_VALUE=""
SYMBOL_FILE=""
LEGACY_SYMBOL_FILE="$ROOT_DIR/bin/bootstrap.noi"
OPENMSX_IMGUI_INI="$OPENMSX_HOME_DIR/share/imgui.ini"
IMGUI_SYMBOL_FILE=""
ZLINK_TCL="$ROOT_DIR/openmsx/zlink.tcl"

manifest_get() {
    awk -F '=' -v k="$1" '
        $1 ~ "^[[:space:]]*"k"[[:space:]]*$" {
            v=$2
            gsub(/^[[:space:]]+|[[:space:]]+$/, "", v)
            print v
            exit
        }
    ' "$TARGET_MANIFEST"
}

if [ ! -f "$TARGET_MANIFEST" ]; then
    echo "Target manifest not found: $TARGET_MANIFEST" >&2
    exit 1
fi

if ! command -v "$OPENMSX_BIN" >/dev/null 2>&1; then
    echo "openMSX binary not found: $OPENMSX_BIN" >&2
    echo "Tip: export OPENMSX_BIN=/path/to/openmsx" >&2
    exit 1
fi

if [ ! -f "$ZLINK_TCL" ]; then
    echo "zlink script not found: $ZLINK_TCL" >&2
    exit 1
fi

if [ -z "${OPENMSX_SYSTEM_DATA:-}" ]; then
    OPENMSX_BIN_PATH="$(command -v "$OPENMSX_BIN")"
    OPENMSX_BIN_DIR="$(cd "$(dirname "$OPENMSX_BIN_PATH")" && pwd)"
    OPENMSX_CANDIDATE_SHARE="$(cd "$OPENMSX_BIN_DIR/.." && pwd)/share"
    if [ -f "$OPENMSX_CANDIDATE_SHARE/init.tcl" ]; then
        OPENMSX_SYSTEM_DATA_AUTO="$OPENMSX_CANDIDATE_SHARE"
        OPENMSX_SYSTEM_DATA_LABEL="  OPENMSX_SYSTEM_DATA=$OPENMSX_SYSTEM_DATA_AUTO \\\\"
    fi
fi

OPENMSX_MACHINE_NAME="$(manifest_get OPENMSX_MACHINE_NAME)"
OPENMSX_MACHINE_XML="$(manifest_get OPENMSX_MACHINE_XML)"
ROM_IMAGE_NAME="$(manifest_get ROM_IMAGE_NAME)"
IO_DEFAULT_PORT="$(manifest_get IO_DEFAULT_PORT)"
OPENMSX_EXTRA_ROM_FILES="$(manifest_get OPENMSX_EXTRA_ROM_FILES || true)"
IMAGE_LAYOUT_VALUE="$(manifest_get IMAGE_LAYOUT)"

if [ -z "$OPENMSX_MACHINE_NAME" ] || [ -z "$OPENMSX_MACHINE_XML" ] || [ -z "$ROM_IMAGE_NAME" ] || [ -z "$IO_DEFAULT_PORT" ]; then
    echo "Missing OPENMSX_MACHINE_NAME/OPENMSX_MACHINE_XML/ROM_IMAGE_NAME/IO_DEFAULT_PORT in $TARGET_MANIFEST" >&2
    exit 1
fi

if [ "$IMAGE_LAYOUT_VALUE" = "flash2x64" ]; then
    SYMBOL_BASENAME="startup"
fi
SYMBOL_FILE="$TARGET_BIN_DIR/$SYMBOL_BASENAME.noi"

mkdir -p "$MACHINES_DIR"

if [ ! -f "$TARGET_BIN_DIR/$ROM_IMAGE_NAME" ]; then
    echo "ROM not found. Run 'make TARGET=$TARGET bootstrap' first." >&2
    exit 1
fi

if [ ! -f "$SYMBOL_FILE" ]; then
    echo "Symbol file not found. Run 'make TARGET=$TARGET bootstrap' first." >&2
    exit 1
fi

# openMSX GUI (ImGui) can persist an old symbol file path and try to load it at boot.
# Keep it aligned with the current target to avoid stale path warnings.
if [ -f "$OPENMSX_IMGUI_INI" ]; then
    IMGUI_SYMBOL_FILE="$(awk -F '=' '/^symbolfile=/ { print $2; exit }' "$OPENMSX_IMGUI_INI")"

    if [ -n "$IMGUI_SYMBOL_FILE" ] && [ "$IMGUI_SYMBOL_FILE" != "$SYMBOL_FILE" ]; then
        echo "INFO: Migrating openMSX persisted symbolfile path:" >&2
        echo "  from: $IMGUI_SYMBOL_FILE" >&2
        echo "    to: $SYMBOL_FILE" >&2
        TMP_IMGUI_INI="$(mktemp)"
        sed "s|^symbolfile=.*$|symbolfile=$SYMBOL_FILE|" "$OPENMSX_IMGUI_INI" > "$TMP_IMGUI_INI"
        mv "$TMP_IMGUI_INI" "$OPENMSX_IMGUI_INI"
    fi
fi

MAIN_SHELL_ADDR="$(awk '/DEF _main_shell / { print $3; exit }' "$SYMBOL_FILE")"
if [ -z "$MAIN_SHELL_ADDR" ]; then
    echo "Symbol _main_shell not found in $SYMBOL_FILE" >&2
    exit 1
fi

# Keep compatibility with legacy openMSX persisted paths.
ln -sf "$SYMBOL_FILE" "$LEGACY_SYMBOL_FILE"
if [ -n "$IMGUI_SYMBOL_FILE" ] && [ "$IMGUI_SYMBOL_FILE" != "$SYMBOL_FILE" ]; then
    mkdir -p "$(dirname "$IMGUI_SYMBOL_FILE")"
    ln -sf "$SYMBOL_FILE" "$IMGUI_SYMBOL_FILE"
fi

MACHINE_XML_SOURCE="$ROOT_DIR/openmsx/$OPENMSX_MACHINE_XML"
ROM_SOURCE="$TARGET_BIN_DIR/$ROM_IMAGE_NAME"

OPENMSX_ROM_ARGS=()
OPENMSX_ROM_LABEL=""

if [ -f "$MACHINE_XML_SOURCE" ]; then
    cp "$MACHINE_XML_SOURCE" "$MACHINES_DIR/$OPENMSX_MACHINE_XML"
    cp "$ROM_SOURCE" "$MACHINES_DIR/$ROM_IMAGE_NAME"
    if [ -n "$OPENMSX_EXTRA_ROM_FILES" ]; then
        for extra_rom in $OPENMSX_EXTRA_ROM_FILES; do
            extra_source="$TARGET_BIN_DIR/$extra_rom"
            if [ ! -f "$extra_source" ]; then
                echo "Required extra ROM not found for machine profile: $extra_source" >&2
                exit 1
            fi
            cp "$extra_source" "$MACHINES_DIR/$extra_rom"
        done
    fi
else
    if [ "$OPENMSX_MACHINE_NAME" = "Z-Tick" ]; then
        echo "Machine XML missing for Z-Tick target: $MACHINE_XML_SOURCE" >&2
        exit 1
    fi

    echo "WARN: local machine XML not found ($MACHINE_XML_SOURCE)." >&2
    echo "WARN: falling back to built-in machine '$OPENMSX_MACHINE_NAME' and -cart '$ROM_SOURCE'." >&2
    OPENMSX_ROM_ARGS=(-cart "$ROM_SOURCE")
    OPENMSX_ROM_LABEL="    -cart \"$ROM_SOURCE\" \\\\"
fi

BREAKPOINT_MAIN_SHELL_LABEL="    -command \"debug set_bp $MAIN_SHELL_ADDR\" \\\\"
OPENMSX_BREAKPOINT_MAIN_SHELL_ARGS=(-command "if {[catch {debug set_bp $MAIN_SHELL_ADDR} _bp_err]} { puts stderr \"WARN main_shell breakpoint ($MAIN_SHELL_ADDR): \$_bp_err\" }")
BREAKPOINT_BOOTLOADER_LABEL="    -command \"debug set_bp 0x0000\" \\\\"
OPENMSX_BREAKPOINT_BOOTLOADER_ARGS=(-command "if {[catch {debug set_bp 0x0000} _bp_boot_err]} { puts stderr \"WARN bootloader breakpoint (0x0000): \$_bp_boot_err\" }")
OPENMSX_SOURCE_ZLINK_ARGS=(-command "if {[catch {source {$ZLINK_TCL}} _zlink_err]} { puts stderr \"WARN zlink script ($ZLINK_TCL): \$_zlink_err\" }")
OPENMSX_INSTALL_ZLINK_ARGS=(-command "if {[catch {zlink_dev::install $IO_DEFAULT_PORT} _zlink_install_err]} { puts stderr \"WARN zlink install ($IO_DEFAULT_PORT): \$_zlink_install_err\" }")
OPENMSX_SELF_CHECK_ARGS=()
SELF_CHECK_LABEL=""
if [ "$SELF_CHECK" -ne 0 ]; then
    OPENMSX_SELF_CHECK_ARGS=(
        -command "if {[catch {zlink_dev::get_task_list} _chk_task_list_err]} { puts stderr \"WARN self-check get_task_list: \$_chk_task_list_err\" }"
        -command "if {[catch {zlink_dev::get_stack_wm} _chk_stack_wm_err]} { puts stderr \"WARN self-check get_stack_wm: \$_chk_stack_wm_err\" }"
        -command "if {[catch {zlink_dev::shell_cmd help} _chk_shell_help_err]} { puts stderr \"WARN self-check shell_cmd help: \$_chk_shell_help_err\" }"
    )
    SELF_CHECK_LABEL="    -command \"zlink_dev::get_task_list\" \\
    -command \"zlink_dev::get_stack_wm\" \\
    -command \"zlink_dev::shell_cmd help\" \\"
fi
if [ "$BP_MAIN_SHELL" -eq 0 ]; then
    BREAKPOINT_MAIN_SHELL_LABEL=""
    OPENMSX_BREAKPOINT_MAIN_SHELL_ARGS=()
fi
if [ "$BP_BOOTLOADER" -eq 0 ]; then
    BREAKPOINT_BOOTLOADER_LABEL=""
    OPENMSX_BREAKPOINT_BOOTLOADER_ARGS=()
fi

ZMSG_WATCH_PROC='proc zmsg_watch {} { puts [format {io_write port=0x%02X value=0x%02X} [expr {$::wp_last_address & 0xFF}] [expr {$::wp_last_value & 0xFF}]] }'

WATCH_MAIN_LABEL=""
OPENMSX_WATCH_ARGS=()
if [ "$WATCH_IO" -ne 0 ]; then
    WATCH_MAIN_LABEL="    -command \"debug set_watchpoint write_io $IO_DEFAULT_PORT {} { zmsg_watch }\" \\\\"
    OPENMSX_WATCH_ARGS=(-command "debug set_watchpoint write_io $IO_DEFAULT_PORT {} { zmsg_watch }")
fi

cat <<EOF
Installed machine files into:
  $MACHINES_DIR

Launching openMSX with:
$OPENMSX_SYSTEM_DATA_LABEL
  $OPENMSX_BIN -machine $OPENMSX_MACHINE_NAME -ext debugdevice -ext programmabledevice \\
${OPENMSX_ROM_LABEL}
    -command "debug symbols load $SYMBOL_FILE" \\
$BREAKPOINT_BOOTLOADER_LABEL
$BREAKPOINT_MAIN_SHELL_LABEL
    -command "source $ZLINK_TCL" \\
    -command "zlink_dev::install $IO_DEFAULT_PORT" \\
$SELF_CHECK_LABEL
    -command "$ZMSG_WATCH_PROC" \\
$WATCH_MAIN_LABEL
EOF

if [ -n "$OPENMSX_SYSTEM_DATA_AUTO" ]; then
    export OPENMSX_SYSTEM_DATA="$OPENMSX_SYSTEM_DATA_AUTO"
fi

exec "$OPENMSX_BIN" -machine "$OPENMSX_MACHINE_NAME" -ext debugdevice -ext programmabledevice \
    "${OPENMSX_ROM_ARGS[@]}" \
    -command "debug symbols load $SYMBOL_FILE" \
    "${OPENMSX_BREAKPOINT_BOOTLOADER_ARGS[@]}" \
    "${OPENMSX_BREAKPOINT_MAIN_SHELL_ARGS[@]}" \
    "${OPENMSX_SOURCE_ZLINK_ARGS[@]}" \
    "${OPENMSX_INSTALL_ZLINK_ARGS[@]}" \
    "${OPENMSX_SELF_CHECK_ARGS[@]}" \
    -command "$ZMSG_WATCH_PROC" \
    "${OPENMSX_WATCH_ARGS[@]}"
