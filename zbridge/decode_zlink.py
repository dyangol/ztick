#!/usr/bin/env python3
"""Decode a raw byte capture (e.g. a picocom --logfile dump) as zlink frames.

Frame format (see top-level README.md):
  B0 = SOF5(5 bits, 0b10101) | TYPE(3 bits)
  B1 = TTY (0..15, upper bits reserved)
  B2 = LEN (0..64)
  B3 = SEQ
  PAYLOAD = LEN bytes
  CRC8 (1 byte) over B0..B(3+LEN), polynomial 0x07, init 0x00, xorout 0x00

This does not assume frames start at offset 0: it scans the byte stream for
a plausible SOF, and on a CRC mismatch (or a truncated tail) it resyncs by
advancing a single byte and trying again, the same way zlink_read_frame()
and openmsx/zlink.tcl's parse_tx_stream() recover from noise.
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

SOF5 = 0x15
MAX_PAYLOAD = 64

TYPE_NAMES = {0: "POLL", 1: "EMPTY", 2: "DATA", 3: "ACK", 4: "NACK"}


def crc8(data: bytes) -> int:
    crc = 0
    for byte in data:
        crc ^= byte
        for _ in range(8):
            if crc & 0x80:
                crc = ((crc << 1) ^ 0x07) & 0xFF
            else:
                crc = (crc << 1) & 0xFF
    return crc


def format_payload(payload: bytes) -> str:
    hex_part = payload.hex(" ")
    ascii_part = "".join(chr(b) if 32 <= b <= 126 else "." for b in payload)
    return f'{hex_part}  "{ascii_part}"'


def decode(data: bytes):
    """Yield tuples (offset, status, type_id, tty, seq, payload, b0, crc_rx, crc_calc).

    status values:
      "ok"        -- valid frame; crc_rx == crc_calc
      "crc_error" -- SOF matched, length plausible, but CRC mismatch;
                     crc_rx and crc_calc both set so caller can log them
      "skip"      -- byte skipped (no valid SOF or implausible length);
                     type_id/tty/seq/payload/crc_rx/crc_calc are all None
    """

    i = 0
    n = len(data)
    while i < n:
        b0 = data[i]
        if (b0 >> 3) != SOF5:
            yield (i, "skip", None, None, None, None, b0, None, None)
            i += 1
            continue

        if i + 3 >= n:
            break  # not enough bytes left even for the header; stop here

        length = data[i + 2]
        if length > MAX_PAYLOAD:
            yield (i, "skip", None, None, None, None, b0, None, None)
            i += 1
            continue

        frame_len = 4 + length + 1
        if i + frame_len > n:
            break  # incomplete trailing frame; stop here

        frame = data[i:i + frame_len]
        crc_calc = crc8(frame[:-1])
        crc_rx = frame[-1]

        if crc_calc != crc_rx:
            yield (i, "crc_error", None, None, None, None, b0, crc_rx, crc_calc)
            i += 1
            continue

        type_id = b0 & 0x07
        tty = data[i + 1] & 0x0F
        seq = data[i + 3]
        payload = bytes(frame[4:4 + length])

        yield (i, "ok", type_id, tty, seq, payload, b0, crc_rx, crc_calc)
        i += frame_len


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument(
        "capture", nargs="?", type=Path,
        help="Raw capture file (e.g. picocom --logfile output); reads stdin if omitted",
    )
    parser.add_argument(
        "--no-poll", action="store_true",
        help="Suppress POLL frames from output (useful to reduce noise)",
    )
    parser.add_argument(
        "--tty", type=int, action="append", metavar="N", dest="ttys",
        help="Show only frames for TTY N (can be repeated: --tty 1 --tty 2)",
    )
    parser.add_argument(
        "--fields", action="store_true",
        help="Show each frame as labeled fields instead of a one-line summary",
    )
    args = parser.parse_args()

    if args.capture is not None:
        if not args.capture.exists():
            print(f"error: capture file not found: {args.capture}", file=sys.stderr)
            return 1
        data = args.capture.read_bytes()
    else:
        data = sys.stdin.buffer.read()

    frame_count = 0
    skipped = 0
    crc_errors = 0

    for offset, status, type_id, tty, seq, payload, b0, crc_rx, crc_calc in decode(data):
        if status == "skip":
            skipped += 1
            continue
        if status == "crc_error":
            crc_errors += 1
            continue

        # status == "ok"
        if args.no_poll and type_id == 0:
            continue

        if args.ttys is not None and tty not in args.ttys:
            continue

        frame_count += 1
        type_name = TYPE_NAMES.get(type_id, f"TYPE{type_id}")
        unknown = type_id not in TYPE_NAMES
        warn = "  *** tipus desconegut, possible corrupció ***" if unknown else ""

        if args.fields:
            print(f"[{offset:06d}] {type_name}{warn}")
            print(f"  b0=0x{b0:02X}  type={type_name}({type_id})  tty={tty}  len={len(payload)}  seq=0x{seq:02X}  crc=0x{crc_rx:02X} OK")
            if payload:
                ascii_part = "".join(chr(b) if 32 <= b <= 126 else "." for b in payload)
                print(f"  hex  : {payload.hex(' ')}")
                print(f"  ascii: {ascii_part}")
        else:
            print(
                f"[{offset:06d}] {type_name:<5} tty={tty:2d} seq=0x{seq:02X} "
                f"len={len(payload):2d} payload={format_payload(payload)}{warn}"
            )

    print(
        f"--- {frame_count} frame(s) decoded, "
        f"{crc_errors} crc error(s), "
        f"{skipped} byte(s) skipped (no SOF) ---",
        file=sys.stderr,
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
