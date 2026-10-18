"""Pack the original MobDB damage, behavior and detection icons used by XIUI."""

from pathlib import Path
import struct

from PIL import Image


ASSETS = Path(__file__).resolve().parents[1] / "assets/mobs/icons"
packs = [
    ("physical", (256, 64), ("Slashing", "Piercing", "Impact", "H2H")),
    ("detection", (256, 256), ("PassiveNQ", "PassiveHQ", "AggroNQ", "AggroHQ", "Link", "Sight",
                               "TrueSight", "Sound", "Scent", "Magic", "JA", "Blood")),
]
for output, size, names in packs:
    atlas = Image.new("RGBA", size)
    for index, name in enumerate(names):
        with Image.open(ASSETS / f"{name}.png") as source:
            icon = source.convert("RGBA")
        if icon.size != (32, 32):
            raise ValueError(f"Unexpected icon size: {name}")
        atlas.paste(icon, (index % 4 * 64 + 16, index // 4 * 64 + 16))
    (ASSETS / f"{output}.bgra").write_bytes(
        struct.pack("<II", *atlas.size) + atlas.tobytes("raw", "BGRA")
    )
