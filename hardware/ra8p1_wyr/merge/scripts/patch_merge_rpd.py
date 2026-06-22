#!/usr/bin/env python3
"""Ensure Objects/merge.rpd requests expanded CPU0 secure code flash."""
from __future__ import annotations

import sys
from pathlib import Path

# 512 KB secure code MRAM for CPU0 — matches solution.xml FLASH_CPU0_S.
FLASH_CPU0_S_SIZE = "0x80000"


def patch(path: Path) -> bool:
    if not path.exists():
        print(f"skip: {path} not found (build merge first)", file=sys.stderr)
        return False
    lines = path.read_text(encoding="ascii").splitlines()
    out: list[str] = []
    changed = False
    for line in lines:
        if line.startswith("FLASH_CPU0_S_SIZE="):
            new = f"FLASH_CPU0_S_SIZE={FLASH_CPU0_S_SIZE}"
            if line != new:
                changed = True
            out.append(new)
        else:
            out.append(line)
    path.write_text("\n".join(out) + "\n", encoding="ascii")
    if changed:
        print(f"patched {path}: FLASH_CPU0_S_SIZE={FLASH_CPU0_S_SIZE}")
    else:
        print(f"{path} already has FLASH_CPU0_S_SIZE={FLASH_CPU0_S_SIZE}")
    return True


def main() -> int:
    rpd = Path(sys.argv[1]) if len(sys.argv) > 1 else Path("Objects/merge.rpd")
    return 0 if patch(rpd) else 1


if __name__ == "__main__":
    raise SystemExit(main())
