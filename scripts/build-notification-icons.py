"""Pack XIUI's original notification PNGs into the embedded BGRA atlas."""

from pathlib import Path
import struct

from PIL import Image


ROOT = Path(__file__).resolve().parents[1]
ASSETS = ROOT / "assets/notifications"
atlas = Image.new("RGBA", (256, 64))
for index, name in enumerate(("invite_icon", "trade_icon", "bazaar_icon")):
    with Image.open(ASSETS / f"{name}.png") as source:
        icon = source.convert("RGBA")
    if icon.size != (64, 64):
        raise ValueError(f"Unexpected notification icon size: {name}")
    atlas.paste(icon, (index * 64, 0))
(ASSETS / "notifications.bgra").write_bytes(
    struct.pack("<II", *atlas.size) + atlas.tobytes("raw", "BGRA")
)
