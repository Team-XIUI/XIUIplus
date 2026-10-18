"""Compress raw image atlases and mob data in generated Windows resources."""

import argparse
from pathlib import Path
import re
import struct
import zlib


def write_changed(path, data):
    if not path.exists() or path.read_bytes() != data:
        path.write_bytes(data)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    lines = []
    original_bytes = packed_bytes = 0
    for line in args.source.read_text(encoding="utf-8").splitlines():
        match = re.match(r'^(\s*(\d+)\s+RCDATA\s+)"([^"]+)"(.*)$', line)
        if match:
            source = Path(match[3])
            if source.suffix.lower() == ".bgra" or source.name == "phoenix_mobs.bin":
                raw = source.read_bytes()
                compressed = b"XZIP" + struct.pack("<I", len(raw)) + zlib.compress(raw, 9)
                original_bytes += len(raw)
                output = args.output.parent / f"{match[2]}-{source.name}.zlib"
                write_changed(output, compressed)
                if len(compressed) < len(raw):
                    line = f'{match[1]}"{output.as_posix()}"{match[4]}'
                    packed_bytes += len(compressed)
                else:
                    packed_bytes += len(raw)
        lines.append(line)
    # Asset-only changes must also rebuild the resource object.
    args.output.write_text("\n".join(lines) + "\n", encoding="utf-8")
    if original_bytes:
        print(f"{args.source.stem}: {original_bytes / 1048576:.2f} -> {packed_bytes / 1048576:.2f} MiB")


if __name__ == "__main__":
    main()
