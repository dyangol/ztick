#!/usr/bin/env python3
"""Simulate FSM execution against a trace of input vectors.

Trace format (JSON):
[
  {"iorq_n":0, "rd_n":0, "wr_n":1, "rxf_n":0, "txe_n":1, "port":144},
  ...
]
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Any, Dict, List, Mapping

from compile_fsm import _safe_eval_expr, _normalize_state_id, _state_match


def _load(path: Path):
    with path.open("r", encoding="utf-8") as fh:
        return json.load(fh)


def _port_to_msx_a(vec: Dict[str, Any]) -> None:
    if "port" not in vec:
        return
    port = int(vec["port"]) & 0xFF
    for i in range(8):
        vec[f"msx_a{i}"] = (port >> i) & 1


def simulate(spec: Mapping[str, Any], trace: List[Dict[str, Any]], state_name: str) -> None:
    states = spec["states"]
    default = spec["default"]
    rules = spec.get("rules", [])

    reverse_states = {int(v): k for k, v in states.items()}

    try:
        state = _normalize_state_id(int(state_name, 0), states)
    except (TypeError, ValueError):
        state = _normalize_state_id(state_name, states)
    default_next = _normalize_state_id(default["next"], states)
    default_controls = dict(default.get("controls", {}))

    for i, vec in enumerate(trace):
        v = dict(vec)
        _port_to_msx_a(v)

        v["state"] = state
        v.setdefault("port", sum((int(v.get(f"msx_a{b}", 0)) & 1) << b for b in range(8)))

        chosen_next = default_next
        chosen_controls = dict(default_controls)
        winner = "default"

        for rule in rules:
            if not _state_match(rule.get("state"), state, states):
                continue
            if _safe_eval_expr(str(rule.get("when", "1")), v) == 0:
                continue
            if "next" in rule:
                chosen_next = _normalize_state_id(rule["next"], states)
            if "controls" in rule:
                merged = dict(chosen_controls)
                merged.update(rule["controls"])
                chosen_controls = merged
            winner = str(rule.get("name", "unnamed_rule"))
            if rule.get("stop", True):
                break

        current_name = reverse_states.get(state, f"S{state}")
        next_name = reverse_states.get(chosen_next, f"S{chosen_next}")
        print(
            f"step={i:03d} state={current_name:<10} port=0x{int(v['port']) & 0xFF:02X} "
            f"rule={winner:<16} next={next_name:<10} controls={chosen_controls}"
        )

        state = chosen_next


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Simulate an FSM spec with an input trace")
    parser.add_argument("spec", type=Path, help="FSM spec JSON")
    parser.add_argument("trace", type=Path, help="Trace JSON")
    parser.add_argument("--initial-state", default="IDLE", help="Initial state name or numeric id")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    spec = _load(args.spec)
    trace = _load(args.trace)
    simulate(spec, trace, args.initial_state)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
