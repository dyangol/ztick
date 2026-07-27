#!/usr/bin/env python3
"""ROM compiler for the MSX <-> FT245 bridge FSM (SST39SF010A, 64 KiB bank).

Physical wiring this compiler encodes:

Address bits (A0-A15):
  A0-A2  : current state, latched by a 74HC374 fed back from D0-D2
  A3     : RXF# (FT245)       - active low (low = a byte is available to read)
  A4     : TXE# (FT245)       - active low (low = TX FIFO not full, can write)
  A5     : WR#  (Z80/MSX)     - active low
  A6     : RD#  (Z80/MSX)     - active low
  A7     : IORQ#(Z80/MSX)     - active low
  A8-A15 : MSX A0-A7 (I/O port number)
  A16    : tied to 0V -- only this 64 KiB bank is ever addressed.

Data bits (D0-D7). D0-D2 are latched by the state 74HC374; D3-D6 are now
ALSO latched, by a second, separate 74HC374 (4 spare flip-flops would have
been enough, but this board uses its own dedicated chip for D3-D6) clocked
by the same clock -- they used to go straight from the ROM to FT245/74HC245
and suffered the same slow/glitchy raw-ROM-output transitions seen on D0-D7
during address changes; latching them gives clean, registered edges instead:
  D0-D2  : next state (fed back into the state 74HC374)
  D3     : F_WR   (FT245 WR)    - active high
  D4     : EBUS_L (74HC245 OE#/CE#) - active low
  D5     : DIR    (74HC245 DIR) - direction; 1 = MSX bus -> FT245, no inverter
  D6     : F_RD_L (FT245 RD#)   - active low
  D7     : RST_L  (FT245 RESET#) - active low; also latched (its own
           74HC374 bit), output wired to FT245's RESET# pin

States: IDLE -> WRITE on a matched OUT, IDLE -> READ on a matched IN with
data available; WRITE/READ then HOLD (bus connected, F_WR/F_RD_L asserted)
for as long as the Z80 keeps IORQ# down, only returning to IDLE once IORQ#
is released. This matters because our state-machine clock (measured at
3.54 MHz, ~282 ns/cycle) is much faster than one Z80 IORQ# cycle: an
unconditional one-cycle WRITE/READ would re-trigger IDLE->WRITE/READ
repeatedly within the same Z80 access, producing multiple short EBUS_L/F_WR
pulses instead of one continuous one spanning the whole access (confirmed
on real hardware with a scope on D4/EBUS_L vs IORQ#).

IDLE -> RESET on any OUT to RESET_PORT (0x35), regardless of data value --
the Z80 data bus isn't wired into any ROM address line (A0-A15 are already
fully committed to state + control signals + the full port number), so the
ROM has no way to see what byte accompanied the write, only that one
happened. RESET then HOLDs (RST_L asserted, FT245 bus left disconnected)
using the exact same re-check pattern as WRITE/READ, for the same reason:
returning to IDLE unconditionally after one FSM tick would let a still-open
Z80 access immediately re-trigger IDLE->RESET, effectively holding RST_L low
for as long as the Z80 access lasts anyway, just via repeated retriggers
instead of one clean hold. Holding explicitly makes that intentional and
correct instead of an accident of timing. This ties the reset pulse width to
one Z80 OUT bus cycle (deterministic, set by the Z80's own clock, not by
this FSM) -- if that turns out shorter than the FT245 needs (check RESET#'s
minimum pulse width in its datasheet against a scope capture), a longer,
explicitly-counted hold can be built later using the state field's 4 still-
unused codes (4-7 in the current 3-bit A0-A2 encoding; STATE_RESET already
claimed code 3).

Output files are 128 KiB (the real 64 KiB image mirrored into the unreachable
upper half), matching the SST39SF010A's full physical size so programmers
like minipro accept it without size-mismatch flags.

Note: TXE# and RXF# are both active low (0 = FIFO has room / has data).

Note: the WRITE/READ entry conditions (port match, IORQ#, RXF#/TXE#,
RD#/WR#) are re-checked for the whole duration of the hold, not just on
IDLE->READ/WRITE entry -- see `write_ok`/`read_ok` in `build_rom`. IORQ#
alone is not enough to hold on: it is asserted for every Z80 I/O
instruction, not just ours, and our own port takes both IN and OUT
accesses, so a same-port or unrelated-port access arriving within one
FSM clock (~282 ns, much faster than a Z80 IORQ# cycle) could otherwise
be mistaken for a continuation of the access in progress.
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

TARGET_PORT = 0x38  # IO_DEFAULT_PORT for targets/vg-8010.mk
RESET_PORT = 0x35  # MSX -> zbridge FT245 RESET# trigger (see module docstring)

def _read_target_mk_value(target_name: str, key: str) -> int:
    """Read a single `KEY = 0xNN`-style value out of ztick's targets/<name>.mk."""

    manifest = Path(__file__).resolve().parent.parent / "targets" / f"{target_name}.mk"
    if not manifest.is_file():
        raise FileNotFoundError(f"no target manifest: {manifest}")

    pattern = re.compile(rf"^\s*{re.escape(key)}\s*=\s*(\S+)")
    for line in manifest.read_text(encoding="utf-8").splitlines():
        match = pattern.match(line)
        if match:
            return int(match.group(1), 0)

    raise ValueError(f"{key} not found in {manifest}")


def read_target_port(target_name: str) -> int:
    """Read IO_DEFAULT_PORT straight out of ztick's targets/<name>.mk."""
    return _read_target_mk_value(target_name, "IO_DEFAULT_PORT")


def read_target_reset_port(target_name: str) -> int:
    """Read IO_RESET_PORT straight out of ztick's targets/<name>.mk."""
    return _read_target_mk_value(target_name, "IO_RESET_PORT")


STATE_IDLE = 0
STATE_WRITE = 1
STATE_READ = 2
STATE_RESET = 3

ROM_SIZE = 1 << 16  # A0-A15


def build_rom(target_port: int = TARGET_PORT, reset_port: int = RESET_PORT) -> bytearray:
    if target_port == reset_port:
        raise ValueError(
            f"target_port and reset_port both 0x{target_port:02X} -- RESET_PORT must be "
            "a distinct port from the FT245 data port"
        )

    rom = bytearray(ROM_SIZE)

    for addr in range(ROM_SIZE):
        state = addr & 0x07
        rxf_l = (addr >> 3) & 1
        txe_l = (addr >> 4) & 1
        wr_l = (addr >> 5) & 1
        rd_l = (addr >> 6) & 1
        iorq_l = (addr >> 7) & 1
        port = (addr >> 8) & 0xFF

        # Whether a WRITE/READ is currently valid: matched port, IORQ#
        # asserted, FT245 FIFO has room/data, and the matching Z80 strobe
        # asserted. Used both to enter WRITE/READ from IDLE and to decide
        # whether to keep holding it (see module docstring for why the
        # hold must keep re-checking these instead of trusting IORQ# alone).
        write_ok = iorq_l == 0 and txe_l == 0 and port == target_port and wr_l == 0
        read_ok = iorq_l == 0 and rxf_l == 0 and port == target_port and rd_l == 0
        # RESET_PORT ignores RXF#/TXE# (irrelevant -- this isn't an FT245
        # data transfer) and the data byte (unreachable, see module
        # docstring): any OUT to this port is the trigger.
        reset_ok = iorq_l == 0 and port == reset_port and wr_l == 0

        # Safe defaults: stay/return to IDLE, bus disconnected, RST_L released.
        next_state = STATE_IDLE
        f_rd_l = 1
        f_wr = 0
        dir_ = 1
        ebus_l = 1
        rst_l = 1

        if state == STATE_IDLE:
            if write_ok:
                next_state = STATE_WRITE
                f_wr = 1
                ebus_l = 0
            elif read_ok:
                next_state = STATE_READ
                f_rd_l = 0
                dir_ = 0
                ebus_l = 0
            elif reset_ok:
                next_state = STATE_RESET
                rst_l = 0
        elif state == STATE_WRITE and write_ok:
            # Hold WRITE (bus connected, F_WR asserted) for as long as the
            # access stays valid: our state-machine clock (~282 ns) is
            # much faster than one Z80 IORQ# cycle, so an unconditional
            # one-tick WRITE would re-trigger IDLE->WRITE repeatedly within
            # the same Z80 access. FT245's WR is rising-edge-triggered, so
            # holding it does not produce extra writes.
            next_state = STATE_WRITE
            f_wr = 1
            ebus_l = 0
        elif state == STATE_READ and read_ok:
            # Same reasoning as WRITE: keep the bus connected and F_RD_L
            # asserted for the whole access, not just one clock tick, so
            # the Z80 always samples valid, stable data.
            next_state = STATE_READ
            f_rd_l = 0
            dir_ = 0
            ebus_l = 0
        elif state == STATE_RESET and reset_ok:
            # Same hold pattern as WRITE/READ, for the same reason: without
            # it, a still-open Z80 access would immediately re-trigger
            # IDLE->RESET after one tick, which in practice holds RST_L low
            # for the whole access anyway -- just via repeated retriggers
            # instead of one clean, intentional hold.
            next_state = STATE_RESET
            rst_l = 0
        # Any unused state value, or a hold whose condition no longer
        # holds, falls through to the safe IDLE defaults set above.

        data = next_state
        data |= f_wr << 3
        data |= ebus_l << 4
        data |= dir_ << 5
        data |= f_rd_l << 6
        data |= rst_l << 7

        rom[addr] = data

    return rom


def to_logisim(data: bytes, values_per_line: int = 16) -> str:
    lines = ["v2.0 raw"]
    for offset in range(0, len(data), values_per_line):
        chunk = data[offset:offset + values_per_line]
        lines.append(" ".join(f"{b:02x}" for b in chunk))
    return "\n".join(lines) + "\n"


def to_ihex(data: bytes, record_size: int = 16) -> str:
    lines = []
    for offset in range(0, len(data), record_size):
        chunk = data[offset:offset + record_size]
        body = bytearray([len(chunk), (offset >> 8) & 0xFF, offset & 0xFF, 0x00])
        body.extend(chunk)
        checksum = (-sum(body)) & 0xFF
        lines.append(":" + body.hex().upper() + f"{checksum:02X}")
    lines.append(":00000001FF")
    return "\n".join(lines) + "\n"


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    parser.add_argument(
        "-o", "--out-bin", type=Path, default=Path("zbridge_fsm.bin"),
        help="Output ROM binary, 64 KiB (default: zbridge_fsm.bin)",
    )
    parser.add_argument(
        "--out-hex", type=Path, help="Optional Intel HEX output path",
    )
    parser.add_argument(
        "--out-logisim", type=Path,
        help="Optional Logisim-evolution image (v2.0 raw), 64 KiB, no chip-size "
             "mirroring -- load it via the ROM component's 'Load Image...'",
    )
    port_source = parser.add_mutually_exclusive_group()
    port_source.add_argument(
        "--port", type=lambda s: int(s, 0),
        help=f"MSX I/O port to decode directly (default 0x{TARGET_PORT:02X} if neither "
             "--port nor --target is given)",
    )
    port_source.add_argument(
        "--target",
        help="Read IO_DEFAULT_PORT/IO_RESET_PORT from targets/<name>.mk instead "
             "(e.g. vg-8010, ztick)",
    )
    parser.add_argument(
        "--reset-port", type=lambda s: int(s, 0),
        help=f"MSX I/O port that triggers the FT245 RESET# pulse (default: read "
             f"IO_RESET_PORT from the target manifest if --target is given, else "
             f"0x{RESET_PORT:02X}); overrides either source if given",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()

    if args.target is not None:
        try:
            port = read_target_port(args.target)
            reset_port = read_target_reset_port(args.target)
        except (FileNotFoundError, ValueError) as exc:
            print(f"error: {exc}", file=sys.stderr)
            return 1
        print(f"using port 0x{port:02X} from targets/{args.target}.mk")
        print(f"using reset port 0x{reset_port:02X} from targets/{args.target}.mk")
    elif args.port is not None:
        port = args.port
        reset_port = RESET_PORT
    else:
        port = TARGET_PORT
        reset_port = RESET_PORT

    if args.reset_port is not None:
        reset_port = args.reset_port
        print(f"using reset port 0x{reset_port:02X} from --reset-port")

    rom = build_rom(port, reset_port)

    # SST39SF010A is 128 KiB (A0-A16). A16 is tied to 0V on this board, so
    # only the lower 64 KiB is ever addressed -- but programmers like
    # minipro still expect a file matching the chip's full physical size.
    # Mirror the real 64 KiB image into the unreachable upper half so the
    # output is directly flashable without extra flags.
    chip_image = rom + rom

    args.out_bin.parent.mkdir(parents=True, exist_ok=True)
    args.out_bin.write_bytes(chip_image)
    print(f"wrote {len(chip_image)} bytes to {args.out_bin}")

    if args.out_hex is not None:
        args.out_hex.parent.mkdir(parents=True, exist_ok=True)
        args.out_hex.write_text(to_ihex(chip_image))
        print(f"wrote Intel HEX to {args.out_hex}")

    if args.out_logisim is not None:
        # The real 64 KiB image, not the chip-size-mirrored one: in Logisim
        # you model exactly the 16 address bits in use (A0-A15), not the
        # physical SST39SF010A's unreachable upper half.
        args.out_logisim.parent.mkdir(parents=True, exist_ok=True)
        args.out_logisim.write_text(to_logisim(rom))
        print(f"wrote Logisim image to {args.out_logisim}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
