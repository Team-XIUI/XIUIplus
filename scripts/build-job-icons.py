"""Pack the pinned XIVParty job PNGs into the embedded BGRA atlas."""

from pathlib import Path
import struct

from PIL import Image


ROOT = Path(__file__).resolve().parents[1]
JOBS = "war mnk whm blm rdm thf pld drk bst brd rng sam nin drg smn blu cor pup dnc sch geo run mon spc".split()
atlas = Image.new("RGBA", (512, 256))
for index, job in enumerate(JOBS):
    icon = Image.open(ROOT / "assets/jobs/xivparty" / f"{job}.png").convert("RGBA")
    if icon.size != (64, 64):
        raise ValueError(f"Unexpected job icon size: {job}")
    atlas.paste(icon, ((index % 8) * 64, (index // 8) * 64))
(ROOT / "assets/jobs/xivparty.bgra").write_bytes(
    struct.pack("<II", *atlas.size) + atlas.tobytes("raw", "BGRA")
)
