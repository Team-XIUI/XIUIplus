"""Pack the pinned job, mob, notification and Magic Burst PNGs into embedded BGRA atlases."""

from pathlib import Path
import struct

from PIL import Image


ASSETS = Path(__file__).resolve().parents[2] / "assets"
JOBS = "war mnk whm blm rdm thf pld drk bst brd rng sam nin drg smn blu cor pup dnc sch geo run mon spc".split()
DETECTION = ("PassiveNQ", "PassiveHQ", "AggroNQ", "AggroHQ", "Link", "Sight",
             "TrueSight", "Sound", "Scent", "Magic", "JA", "Blood")
ELEMENTS = ("fire", "ice", "wind", "earth", "lightning", "water", "light", "dark")

# output, atlas size, source folder, names, columns, cell, icon size, resize to icon size
PACKS = [
    ("jobs/xivparty.bgra", (512, 256), "jobs/xivparty", JOBS, 8, 64, 64, False),
    ("mobs/icons/physical.bgra", (256, 64), "mobs/icons", ("Slashing", "Piercing", "Impact", "H2H"), 4, 64, 32, False),
    ("mobs/icons/detection.bgra", (256, 256), "mobs/icons", DETECTION, 4, 64, 32, False),
    ("notifications/notifications.bgra", (256, 64), "notifications", ("invite_icon", "trade_icon", "bazaar_icon"), 4, 64, 64, False),
    ("skillchains/burst.bgra", (1024, 512), "skillchains/art", ELEMENTS, 4, 256, 256, True),
]

for output, size, folder, names, columns, cell, icon_size, resize in PACKS:
    atlas = Image.new("RGBA", size)
    inset = (cell - icon_size) // 2
    for index, name in enumerate(names):
        with Image.open(ASSETS / folder / f"{name}.png") as source:
            icon = source.convert("RGBA")
        if resize:
            icon = icon.resize((icon_size, icon_size), Image.Resampling.LANCZOS)
        elif icon.size != (icon_size, icon_size):
            raise ValueError(f"Unexpected icon size: {folder}/{name}")
        atlas.paste(icon, (index % columns * cell + inset, index // columns * cell + inset))
    (ASSETS / output).write_bytes(struct.pack("<II", *atlas.size) + atlas.tobytes("raw", "BGRA"))
