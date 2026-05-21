#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Unified AquaGarden Python entrypoint.

Subcommands:
  serial   MCU UART + tank camera bridge, formerly serial_bridge.py
  sensor   Phytium snapshot uploader, formerly send_sensor-ft.py
  bridge   Phytium HTTP bridge, formerly aqua_bridge-feiteng.py
"""

from __future__ import annotations

import argparse
import importlib.util
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent


def _load_module(filename: str, module_name: str):
    path = ROOT / filename
    spec = importlib.util.spec_from_file_location(module_name, path)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"cannot load {path}")
    module = importlib.util.module_from_spec(spec)
    sys.modules[module_name] = module
    spec.loader.exec_module(module)
    return module


def run_serial(argv: list[str] | None = None) -> None:
    module = _load_module("serial_bridge.py", "aquagarden_serial_bridge")
    old_argv = sys.argv
    try:
        sys.argv = [str(ROOT / "serial_bridge.py"), *(argv or [])]
        module.main()
    finally:
        sys.argv = old_argv


def run_sensor(argv: list[str] | None = None) -> None:
    if argv:
        raise SystemExit("sensor mode is configured by environment variables and accepts no arguments")
    module = _load_module("send_sensor-ft.py", "aquagarden_sensor_uploader")
    module.main()


def run_bridge(argv: list[str] | None = None) -> None:
    if argv:
        raise SystemExit("bridge mode is configured by environment variables and accepts no arguments")
    module = _load_module("aqua_bridge-feiteng.py", "aquagarden_phytium_bridge")
    module.app.run(host=module.HOST, port=module.PORT, debug=False, threaded=True)


def main() -> None:
    parser = argparse.ArgumentParser(description="AquaGarden unified Python runner")
    sub = parser.add_subparsers(dest="mode", required=True)
    sub.add_parser("serial", help="run MCU UART + tank camera bridge").add_argument("args", nargs=argparse.REMAINDER)
    sub.add_parser("sensor", help="run Phytium snapshot uploader").add_argument("args", nargs=argparse.REMAINDER)
    sub.add_parser("bridge", help="run Phytium HTTP bridge").add_argument("args", nargs=argparse.REMAINDER)
    ns = parser.parse_args()
    args = ns.args[1:] if ns.args[:1] == ["--"] else ns.args
    if ns.mode == "serial":
        run_serial(args)
    elif ns.mode == "sensor":
        run_sensor(args)
    elif ns.mode == "bridge":
        run_bridge(args)


if __name__ == "__main__":
    main()
