"""Repack the pinned XIUI Magic Burst element art for the native icon cache."""
from pathlib import Path
from PIL import Image
import struct
root = Path(__file__).resolve().parents[1]
names = ['fire', 'ice', 'wind', 'earth', 'lightning', 'water', 'light', 'dark']
atlas = Image.new('RGBA', (1024, 512))
for i, name in enumerate(names):
    with Image.open(root / 'assets/skillchains/art' / (name + '.png')) as source:
        art = source.convert('RGBA').resize((256, 256), Image.Resampling.LANCZOS)
        atlas.paste(art, ((i % 4) * 256, (i // 4) * 256))
(root / 'assets/skillchains/burst.bgra').write_bytes(struct.pack('<II', 1024, 512) + atlas.tobytes('raw', 'BGRA'))
print('Packed eight elemental orbs into the 1024x512 burst atlas.')
