"""Create a small source archive, excluding local benchmark/runtime files."""
from pathlib import Path
from zipfile import ZIP_DEFLATED, ZipFile

root = Path(__file__).resolve().parents[1]
destination = root.parent / f"{root.name}-source.zip"
with ZipFile(destination, "w", ZIP_DEFLATED) as archive:
    for path in sorted(root.rglob("*")):
        if (path.is_file() and not set(path.parts).intersection(
                {".venv", ".vscode", "__pycache__", "reports", "dist", "vendor"})
                and path.suffix.lower() not in {".zip", ".dll", ".obj", ".pdb"}):
            archive.write(path, path.relative_to(root.parent))
print(destination)
