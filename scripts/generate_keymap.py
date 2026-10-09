"""Generate the WIN68 physical/firmware map from Aether's vendor layout."""
from __future__ import annotations

import argparse
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
LAYOUT = ROOT / "layout/aula-win68he-si2828heargb.json"
HEADER = ROOT / "plugin/AulaKeymap.h"
SDK_LAYOUT = ROOT / "examples/win68-led-layout.json"

NAMES = {
    "Esc": "Escape", "Back": "Backspace", "Ins": "Insert",
    "Del": "Delete", "Caps": "Caps Lock", "Enter": "Enter",
    "Tab": "Tab", "L-Shift": "Left Shift", "R-Shift": "Right Shift",
    "L-Ctrl": "Left Control", "R-Ctrl": "Right Control",
    "L-Win": "Left Windows", "L-Alt": "Left Alt", "R-Alt": "Right Alt",
    "Space": "Space", "Fn": "Fn", "Up": "Up Arrow",
    "Down": "Down Arrow", "Left": "Left Arrow", "Right": "Right Arrow",
    "PgUp": "Page Up", "PgDn": "Page Down",
    "-_": "Minus", "=+": "Equals", "[{": "Left Bracket",
    "]}": "Right Bracket", "\\|": "Backslash", ";:": "Semicolon",
    '" \'': "Apostrophe", ",<": "Comma", ".>": "Period", "/?": "Slash",
}

# OpenRGB 1.0 DeviceView looks up LED names from RGBControllerKeyNames.cpp.
# Its native keyboard labels and wide-key drawing require these exact names.
OPENRGB_SYMBOLS = {
    "Minus": "-", "Equals": "=", "Left Bracket": "[",
    "Right Bracket": "]", "Backslash": "\\", "Semicolon": ";",
    "Apostrophe": "'", "Comma": ",", "Period": ".", "Slash": "/",
    "Fn": "Right Fn",
}


def openrgb_name(name: str) -> str:
    return "Key: " + OPENRGB_SYMBOLS.get(name, name)


def native_column(key: dict) -> int:
    # Use the left edge, not the center: wide keys otherwise drift right and
    # OpenRGB's native key expansion leaves artificial gaps before the next key.
    # The vendor layout starts at x=10 and has a 38px pitch for regular keys.
    return (int(key["x"]) - 10 + 19) // 38


def key_name(raw: str) -> str:
    if raw in NAMES:
        return NAMES[raw]
    if len(raw) == 1 and raw.isalpha():
        return raw.upper()
    if raw and raw[0].isdigit():
        return raw[0]
    raise ValueError(f"Unrecognized WIN68 key label: {raw!r}")


def generate() -> str:
    keys = json.loads(LAYOUT.read_text(encoding="utf-8"))["keys"]
    assert len(keys) == 68
    seen_indices: set[int] = set()
    seen_cells: set[tuple[int, int]] = set()
    lines = [
        "// Generated from layout/aula-win68he-si2828heargb.json (Aether-HE vendor import)",
        "// Vendor indices are provisional until confirmed per key on WIN68 hardware.",
        "#pragma once",
        "struct AulaKey { const char* name; const char* openrgb_name; const char* legend; unsigned int firmware_index; unsigned int column; unsigned int row; int x; int y; int width; int height; };",
        "static constexpr AulaKey AULA_KEYS[] = {",
    ]
    for key in keys:
        index = int(key["index"])
        x, y, width, height = (int(key[k]) for k in ("x", "y", "width", "height"))
        column = native_column(key)
        row = round((y - 15) / 38)
        if not (0 <= index < 132 and 0 <= row < 5 and 0 <= column < 16):
            raise ValueError(f"Invalid position/index for {key['name']}")
        if index in seen_indices or (row, column) in seen_cells:
            raise ValueError(f"Duplicate index or matrix cell for {key['name']}")
        seen_indices.add(index)
        seen_cells.add((row, column))
        friendly = key_name(key["name"])
        lines.append(f"    {{ {json.dumps(friendly)}, {json.dumps(openrgb_name(friendly))}, "
                     f"{json.dumps(key['name'])}, {index}, {column}, {row}, {x}, {y}, {width}, {height} }},")
    lines.append("};")
    return "\n".join(lines) + "\n"


def sdk_layout() -> str:
    keys = json.loads(LAYOUT.read_text(encoding="utf-8"))["keys"]
    data = [
        {"name": openrgb_name(key_name(k["name"])), "firmware_index": int(k["index"]),
         "x": int(k["x"]) + int(k["width"]) / 2,
         "y": int(k["y"]) + int(k["height"]) / 2}
        for k in keys
    ]
    return json.dumps(data, indent=2) + "\n"


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--check", action="store_true", help="verify checked-in header")
    args = parser.parse_args()
    expected = generate()
    if args.check:
        if HEADER.read_text(encoding="utf-8") != expected:
            raise SystemExit("AulaKeymap.h differs from the Aether layout; regenerate it")
        if SDK_LAYOUT.read_text(encoding="utf-8") != sdk_layout():
            raise SystemExit("win68-led-layout.json differs from the Aether layout")
        print("PASS: 68 named keys, unique firmware indices and physical cells")
    else:
        HEADER.write_text(expected, encoding="utf-8", newline="\n")
        SDK_LAYOUT.write_text(sdk_layout(), encoding="utf-8", newline="\n")
        print(f"Wrote {HEADER}")
