#!/usr/bin/env python3
"""Generate merge/solution.xml with 512KB CPU0 secure code flash (FLASH_CPU0_S)."""
from pathlib import Path

TEMPLATE = Path(r"E:/AquaGarden/hardware/ra8p1/template/template_multicore/solution.xml")
OUT = Path(__file__).resolve().parent.parent / "solution.xml"

text = TEMPLATE.read_text(encoding="utf-8")
text = text.replace(
    '<option key="#SELECTED_TOOLCHAIN#" value="gcc-arm-embedded"/>',
    '<option key="#SELECTED_TOOLCHAIN#" value="com.arm.toolchain"/>',
)
text = text.replace(
    "${workspace_loc:/template_multicore_CPU0}/Debug/template_multicore_CPU0.sbd",
    "Objects/merge.sbd",
)
text = text.replace(
    "${workspace_loc:/template_multicore_CPU1}/Debug/template_multicore_CPU1.sbd",
    "",
)
text = text.replace(
    "    <bundle>Objects/merge.sbd</bundle>\n    <bundle></bundle>\n",
    "    <bundle>Objects/merge.sbd</bundle>\n",
)
OUT.write_text(text, encoding="utf-8", newline="\n")
print(f"wrote {OUT} (FLASH_CPU0_S=0x80000 / 512KB)")
