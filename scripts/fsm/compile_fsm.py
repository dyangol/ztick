#!/usr/bin/env python3
"""Compile a declarative FSM spec into ROM images for SST39SF010A-like designs.

Address bits (example):
- A0..A2: current state (latched by 74HC374)
- A3..A7: async inputs (IORQ#, RD#, WR#, RXF#, TXE#)
- A8..A15: MSX A0..A7 (I/O port decode)

Data bits:
- D0..D2: next state
- D3..D7: control outputs
"""

from __future__ import annotations

import argparse
import ast
import json
from pathlib import Path
from typing import Any, Dict, Iterable, List, Mapping, MutableMapping, Sequence


class FsmCompileError(Exception):
    pass


_ALLOWED_BOOL_OPS = (ast.And, ast.Or)
_ALLOWED_UNARY_OPS = (ast.Not, ast.Invert, ast.USub, ast.UAdd)
_ALLOWED_BIN_OPS = (
    ast.Add,
    ast.Sub,
    ast.Mult,
    ast.FloorDiv,
    ast.Mod,
    ast.BitAnd,
    ast.BitOr,
    ast.BitXor,
    ast.LShift,
    ast.RShift,
)
_ALLOWED_CMP_OPS = (
    ast.Eq,
    ast.NotEq,
    ast.Lt,
    ast.LtE,
    ast.Gt,
    ast.GtE,
)


def _safe_eval_expr(expr: str, names: Mapping[str, int]) -> int:
    """Evaluate a small expression language safely via Python AST."""

    def eval_node(node: ast.AST) -> int:
        if isinstance(node, ast.Expression):
            return eval_node(node.body)
        if isinstance(node, ast.Constant):
            if isinstance(node.value, bool):
                return int(node.value)
            if isinstance(node.value, int):
                return node.value
            raise FsmCompileError(f"Unsupported constant in expression '{expr}': {node.value!r}")
        if isinstance(node, ast.Name):
            if node.id not in names:
                raise FsmCompileError(f"Unknown symbol '{node.id}' in expression '{expr}'")
            return names[node.id]
        if isinstance(node, ast.BoolOp):
            if not isinstance(node.op, _ALLOWED_BOOL_OPS):
                raise FsmCompileError(f"Unsupported boolean operator in expression '{expr}'")
            values = [bool(eval_node(v)) for v in node.values]
            if isinstance(node.op, ast.And):
                return int(all(values))
            return int(any(values))
        if isinstance(node, ast.UnaryOp):
            if not isinstance(node.op, _ALLOWED_UNARY_OPS):
                raise FsmCompileError(f"Unsupported unary operator in expression '{expr}'")
            value = eval_node(node.operand)
            if isinstance(node.op, ast.Not):
                return int(not bool(value))
            if isinstance(node.op, ast.Invert):
                return ~value
            if isinstance(node.op, ast.USub):
                return -value
            return +value
        if isinstance(node, ast.BinOp):
            if not isinstance(node.op, _ALLOWED_BIN_OPS):
                raise FsmCompileError(f"Unsupported binary operator in expression '{expr}'")
            lhs = eval_node(node.left)
            rhs = eval_node(node.right)
            if isinstance(node.op, ast.Add):
                return lhs + rhs
            if isinstance(node.op, ast.Sub):
                return lhs - rhs
            if isinstance(node.op, ast.Mult):
                return lhs * rhs
            if isinstance(node.op, ast.FloorDiv):
                return lhs // rhs
            if isinstance(node.op, ast.Mod):
                return lhs % rhs
            if isinstance(node.op, ast.BitAnd):
                return lhs & rhs
            if isinstance(node.op, ast.BitOr):
                return lhs | rhs
            if isinstance(node.op, ast.BitXor):
                return lhs ^ rhs
            if isinstance(node.op, ast.LShift):
                return lhs << rhs
            return lhs >> rhs
        if isinstance(node, ast.Compare):
            lhs = eval_node(node.left)
            result = True
            for op, comp in zip(node.ops, node.comparators):
                rhs = eval_node(comp)
                if not isinstance(op, _ALLOWED_CMP_OPS):
                    raise FsmCompileError(f"Unsupported compare operator in expression '{expr}'")
                if isinstance(op, ast.Eq):
                    ok = lhs == rhs
                elif isinstance(op, ast.NotEq):
                    ok = lhs != rhs
                elif isinstance(op, ast.Lt):
                    ok = lhs < rhs
                elif isinstance(op, ast.LtE):
                    ok = lhs <= rhs
                elif isinstance(op, ast.Gt):
                    ok = lhs > rhs
                else:
                    ok = lhs >= rhs
                if not ok:
                    result = False
                    break
                lhs = rhs
            return int(result)
        raise FsmCompileError(f"Unsupported syntax in expression '{expr}': {type(node).__name__}")

    tree = ast.parse(expr, mode="eval")
    return eval_node(tree)


def _load_json(path: Path) -> Dict[str, Any]:
    with path.open("r", encoding="utf-8") as fh:
        return json.load(fh)


def _bit(value: int, idx: int) -> int:
    return (value >> idx) & 1


def _set_bit(value: int, idx: int, bitval: int) -> int:
    if bitval:
        return value | (1 << idx)
    return value & ~(1 << idx)


def _build_signal_map(signals: Mapping[str, int], addr: int) -> Dict[str, int]:
    out: Dict[str, int] = {}
    for name, bit_index in signals.items():
        out[name] = _bit(addr, int(bit_index))
    return out


def _port_value(ctx: Mapping[str, int]) -> int:
    # Build 8-bit port from msx_a0..msx_a7, defaulting to 0 if absent.
    value = 0
    for i in range(8):
        value |= (ctx.get(f"msx_a{i}", 0) & 1) << i
    return value


def _normalize_state_id(state_value: Any, states: Mapping[str, int]) -> int:
    if isinstance(state_value, str):
        if state_value not in states:
            raise FsmCompileError(f"Unknown state '{state_value}'")
        return int(states[state_value])
    if isinstance(state_value, int):
        return state_value
    raise FsmCompileError(f"State must be string or int, got: {type(state_value).__name__}")


def _state_match(rule_state: Any, current_state: int, states: Mapping[str, int]) -> bool:
    if rule_state is None:
        return True
    if isinstance(rule_state, list):
        ids = {_normalize_state_id(item, states) for item in rule_state}
        return current_state in ids
    return current_state == _normalize_state_id(rule_state, states)


def _encode_controls(controls: Mapping[str, int], control_bits: Mapping[str, int]) -> int:
    out = 0
    for name, bit_index in control_bits.items():
        out = _set_bit(out, int(bit_index), int(controls.get(name, 0)) & 1)
    return out


def _render_ihex(data: bytes, base_addr: int = 0, record_size: int = 16) -> str:
    lines: List[str] = []

    def rec(addr: int, record_type: int, payload: bytes) -> str:
        body = bytearray()
        body.append(len(payload))
        body.append((addr >> 8) & 0xFF)
        body.append(addr & 0xFF)
        body.append(record_type)
        body.extend(payload)
        checksum = ((~sum(body) + 1) & 0xFF)
        return ":" + "".join(f"{b:02X}" for b in body) + f"{checksum:02X}"

    current_upper = None
    for offset in range(0, len(data), record_size):
        abs_addr = base_addr + offset
        upper = (abs_addr >> 16) & 0xFFFF
        if upper != current_upper:
            current_upper = upper
            lines.append(rec(0, 0x04, bytes([(upper >> 8) & 0xFF, upper & 0xFF])))
        low = abs_addr & 0xFFFF
        chunk = data[offset : offset + record_size]
        lines.append(rec(low, 0x00, chunk))

    lines.append(":00000001FF")
    return "\n".join(lines) + "\n"


def _render_logisim_raw(data: bytes, values_per_line: int = 16) -> str:
    lines = ["v2.0 raw"]
    for offset in range(0, len(data), values_per_line):
        chunk = data[offset : offset + values_per_line]
        lines.append(" ".join(f"{b:02x}" for b in chunk))
    return "\n".join(lines) + "\n"


def compile_fsm(spec: Mapping[str, Any]) -> bytes:
    rom_cfg = spec.get("rom", {})
    rom_size = int(rom_cfg.get("size", 65536))
    fill = int(rom_cfg.get("fill", 0xFF)) & 0xFF

    state_bits: Sequence[int] = spec["address_layout"]["state_bits"]
    input_signals: Mapping[str, int] = spec["address_layout"]["input_signals"]

    out_layout = spec["output_layout"]
    next_state_bits: Sequence[int] = out_layout["next_state_bits"]
    control_bits: Mapping[str, int] = out_layout["control_bits"]

    states: Mapping[str, int] = spec["states"]
    default = spec["default"]
    rules = spec.get("rules", [])

    max_state = (1 << len(state_bits)) - 1
    for sname, sid in states.items():
        if int(sid) < 0 or int(sid) > max_state:
            raise FsmCompileError(
                f"State '{sname}' id {sid} does not fit in {len(state_bits)} state bits"
            )

    default_next = _normalize_state_id(default["next"], states)
    default_controls = dict(default.get("controls", {}))

    image = bytearray([fill] * rom_size)

    for addr in range(rom_size):
        ctx = _build_signal_map(input_signals, addr)

        current_state = 0
        for i, bit_idx in enumerate(state_bits):
            current_state |= _bit(addr, int(bit_idx)) << i

        ctx["state"] = current_state
        ctx["addr"] = addr
        ctx["port"] = _port_value(ctx)

        chosen_next = default_next
        chosen_controls = dict(default_controls)

        for rule in rules:
            if not _state_match(rule.get("state"), current_state, states):
                continue

            when = str(rule.get("when", "1"))
            if _safe_eval_expr(when, ctx) == 0:
                continue

            if "next" in rule:
                chosen_next = _normalize_state_id(rule["next"], states)
            if "controls" in rule:
                merged = dict(chosen_controls)
                merged.update(rule["controls"])
                chosen_controls = merged
            if rule.get("stop", True):
                break

        data = 0
        for i, bit_idx in enumerate(next_state_bits):
            data = _set_bit(data, int(bit_idx), (chosen_next >> i) & 1)

        control_word = _encode_controls(chosen_controls, control_bits)
        for bit_idx in range(8):
            if _bit(control_word, bit_idx):
                data = _set_bit(data, bit_idx, 1)

        image[addr] = data & 0xFF

    return bytes(image)


def mirror_64k_to_128k(data64k: bytes) -> bytes:
    if len(data64k) != 65536:
        raise FsmCompileError("Mirror helper expects exactly 64KiB")
    return data64k + data64k


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Compile FSM JSON spec into ROM image")
    parser.add_argument("spec", type=Path, help="FSM JSON spec path")
    parser.add_argument("--out-bin", type=Path, required=True, help="Output binary path")
    parser.add_argument("--out-hex", type=Path, help="Optional Intel HEX output path")
    parser.add_argument(
        "--out-logisim",
        type=Path,
        help="Optional Logisim-evolution image output path (v2.0 raw)",
    )
    parser.add_argument(
        "--mirror-39sf010a",
        action="store_true",
        help="Also emit a 128KiB image by mirroring the 64KiB bank",
    )
    parser.add_argument(
        "--out-bin-128k",
        type=Path,
        help="Optional output path for mirrored 128KiB image (used with --mirror-39sf010a)",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    spec = _load_json(args.spec)
    image = compile_fsm(spec)

    args.out_bin.parent.mkdir(parents=True, exist_ok=True)
    args.out_bin.write_bytes(image)

    if args.out_hex is not None:
        args.out_hex.parent.mkdir(parents=True, exist_ok=True)
        args.out_hex.write_text(_render_ihex(image), encoding="ascii")

    if args.out_logisim is not None:
        args.out_logisim.parent.mkdir(parents=True, exist_ok=True)
        args.out_logisim.write_text(_render_logisim_raw(image), encoding="ascii")

    if args.mirror_39sf010a:
        out128 = args.out_bin_128k
        if out128 is None:
            out128 = args.out_bin.with_name(args.out_bin.stem + "_39sf010a.bin")
        out128.parent.mkdir(parents=True, exist_ok=True)
        out128.write_bytes(mirror_64k_to_128k(image))

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
