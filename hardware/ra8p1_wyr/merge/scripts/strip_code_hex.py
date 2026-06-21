#!/usr/bin/env python3
"""Keep only RA8P1 code MRAM records (0x02000000-0x020FFFFF) from a Keil merge.hex."""

from __future__ import annotations

import sys
from pathlib import Path

CODE_BASE = 0x02000000
CODE_END = 0x02100000


def parse_records(path: Path) -> list[tuple[int, int, int, bytes]]:
    records: list[tuple[int, int, int, bytes]] = []
    ext = 0
    for lineno, line in enumerate(path.read_text(encoding="ascii", errors="ignore").splitlines(), 1):
        line = line.strip()
        if not line.startswith(":"):
            continue
        if len(line) < 11:
            print(f"skip line {lineno}: too short", file=sys.stderr)
            continue
        try:
            count = int(line[1:3], 16)
            addr = int(line[3:7], 16)
            rtype = int(line[7:9], 16)
            data = bytes.fromhex(line[9 : 9 + count * 2])
        except ValueError as exc:
            print(f"skip line {lineno}: {exc} ({line[:40]!r})", file=sys.stderr)
            continue
        if rtype == 4:
            ext = int.from_bytes(data, "big") << 16
        elif rtype in (0, 1):
            records.append((ext, addr, rtype, data))
    return records


def emit_hex(records: list[tuple[int, int, int, bytes]], out: Path) -> None:
    lines: list[str] = []
    cur_ext: int | None = None

    def checksum(count: int, addr: int, rtype: int, data: bytes) -> int:
        total = count + ((addr >> 8) & 0xFF) + (addr & 0xFF) + rtype + sum(data)
        return (~total + 1) & 0xFF

    for ext, addr, rtype, data in records:
        if rtype == 1:
            count = 0
            cs = checksum(count, 0, 1, b"")
            lines.append(f":{count:02X}{0:04X}{1:02X}{cs:02X}")
            continue
        base = ext + addr
        end = base + len(data)
        if end <= CODE_BASE or base >= CODE_END:
            continue
        if cur_ext != ext:
            cur_ext = ext
            payload = (ext >> 16).to_bytes(2, "big")
            count = len(payload)
            cs = checksum(count, 0, 4, payload)
            lines.append(f":{count:02X}{0:04X}{4:02X}{payload.hex().upper()}{cs:02X}")
        count = len(data)
        cs = checksum(count, addr, 0, data)
        lines.append(f":{count:02X}{addr:04X}{0:02X}{data.hex().upper()}{cs:02X}")

    out.write_text("\n".join(lines) + "\n", encoding="ascii")


def main() -> int:
    src = Path(sys.argv[1]) if len(sys.argv) > 1 else Path("Objects/merge.hex")
    dst = Path(sys.argv[2]) if len(sys.argv) > 2 else src.with_name("merge_code.hex")
    if not src.exists():
        print(f"missing input: {src}", file=sys.stderr)
        return 1
    if src.suffix.lower() != ".hex":
        print(f"expected .hex file, got: {src}", file=sys.stderr)
        return 1
    records = parse_records(src)
    code_records = [
        r for r in records
        if r[2] == 1 or (CODE_BASE <= (r[0] + r[1]) < CODE_END)
    ]
    if not any(r[2] == 0 for r in code_records):
        print(f"no code records in {src}", file=sys.stderr)
        return 1
    emit_hex(records, dst)
    print(f"wrote {dst}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
