"""Pack the pinned Vincerno macro icon sources into a separate embedded atlas."""

from io import BytesIO
from pathlib import Path
import json
import re
import struct
import zipfile

from PIL import Image


ROOT = Path(__file__).resolve().parents[2]
ASSETS = ROOT / "assets/actionbars/vincerno"
WIDTH, HEIGHT, CELL, COLUMNS = 1024, 512, 36, 28


def main():
    atlas = Image.new("RGBA", (WIDTH, HEIGHT))
    entries = []
    with zipfile.ZipFile(ASSETS / "sources.zip") as sources:
        paths = sorted(sources.namelist(), key=str.lower)
        if len(paths) > COLUMNS * (HEIGHT // CELL):
            raise ValueError("Custom icons exceed atlas capacity")
        for index, name in enumerate(paths):
            path = Path(name)
            key = "vincerno/" + path.with_suffix("").as_posix().lower()
            label = re.sub(r"([a-z])([A-Z])", r"\1 \2", path.stem).replace("_", " ")
            category = " / ".join(path.parent.parts)
            entries.append((key, label, category, index))
            icon = Image.open(BytesIO(sources.read(name))).convert("RGBA").resize((32, 32), Image.Resampling.LANCZOS)
            x, y = (index % COLUMNS) * CELL + 2, (index // COLUMNS) * CELL + 2
            atlas.paste(icon, (x, y))
            atlas.paste(icon.crop((0, 0, 1, 32)).resize((2, 32)), (x - 2, y))
            atlas.paste(icon.crop((31, 0, 32, 32)).resize((2, 32)), (x + 32, y))
            atlas.paste(atlas.crop((x - 2, y, x + 34, y + 1)).resize((36, 2)), (x - 2, y - 2))
            atlas.paste(atlas.crop((x - 2, y + 31, x + 34, y + 32)).resize((36, 2)), (x - 2, y + 32))
    (ASSETS / "icons.bgra").write_bytes(struct.pack("<II", WIDTH, HEIGHT) + atlas.tobytes("raw", "BGRA"))
    rows = "\n".join("        {" + ", ".join([json.dumps(key), json.dumps(label), json.dumps(category), str(index)]) + "},"
                     for key, label, category, index in entries)
    (ROOT / "src/runtime/resources/custom_action_icons.hpp").write_text(
        "#pragma once\n\n#include <array>\n#include <cstdint>\n#include <string_view>\n\nnamespace xiui::resources\n{\n"
        "    struct custom_action_icon { std::string_view key, label, category; uint16_t index; };\n"
        f"    inline constexpr uint32_t custom_action_icon_width = {WIDTH}, custom_action_icon_height = {HEIGHT};\n"
        f"    inline constexpr uint32_t custom_action_icon_columns = {COLUMNS}, custom_action_icon_cell = {CELL};\n"
        f"    inline constexpr std::array<custom_action_icon, {len(entries)}> custom_action_icons{{{{\n{rows}\n    }}}};\n}}\n",
        encoding="utf-8",
    )
    print(f"Packed {len(entries)} custom icons into {WIDTH}x{HEIGHT}.")


if __name__ == "__main__":
    main()
