"""Check the native OpenRGB matrix against 68 vendor keys and wide-key rules."""
import json

from generate_keymap import LAYOUT, ROOT, key_name, native_column, openrgb_name

keys = json.loads(LAYOUT.read_text(encoding="utf-8"))["keys"]
matrix = [[None] * 16 for _ in range(5)]
names = set()
indices = set()
for key in keys:
    name = key_name(key["name"])
    row = (int(key["y"]) - 15 + 19) // 38
    col = native_column(key)
    assert 0 <= row < 5 and 0 <= col < 16, (name, row, col)
    assert matrix[row][col] is None, (name, matrix[row][col])
    assert name not in names and int(key["index"]) not in indices
    matrix[row][col] = name
    names.add(name)
    indices.add(int(key["index"]))
    assert openrgb_name(name).startswith("Key: ")

assert len(keys) == len(names) == len(indices) == 68
assert matrix[0][0] == "Escape" and matrix[0][13] == "Backspace"
assert matrix[1][0] == "Tab" and matrix[1][2] == "Q"
assert matrix[2][0] == "Caps Lock" and matrix[2][13] == "Enter"
assert matrix[3][0] == "Left Shift" and matrix[3][12] == "Right Shift"
assert matrix[4][4] == "Space" and matrix[4][13:16] == ["Left Arrow", "Down Arrow", "Right Arrow"]
for row, col in ((0, 13), (1, 0), (2, 0), (2, 13), (3, 0), (3, 12)):
    assert matrix[row][col + 1] is None, (row, col, "wide key cannot expand")
assert matrix[4][5:10] == [None] * 5, "Space must fill five empty cells"

device = (ROOT / "plugin/AulaHEDevice.cpp").read_text(encoding="utf-8")
assert "whole_keyboard.matrix_map.width = 16;" in device
assert "whole_keyboard.matrix_map.height = 5;" in device
assert "AULA_KEYS[i].row * 16 + AULA_KEYS[i].column" in device
print("PASS: 68 named keys, 5x16 native matrix, wide-key expansion cells")
