"""Pack recast action artwork into one embedded atlas."""

from pathlib import Path
import struct

from PIL import Image


ROOT = Path(__file__).resolve().parents[2]
ASSETS = ROOT / 'assets/recast'
WIDTH, HEIGHT, CELL, COLUMNS = 2048, 1024, 36, 56
atlas = Image.new('RGBA', (WIDTH, HEIGHT))
entries = []
for kind in ('abilities', 'spells'):
    for path in sorted((ASSETS / 'icons' / kind).glob('*.png')):
        if path.stem == 'default':
            key = 0x1FFFFFF if kind == 'spells' else 0xFFFFFF
        else:
            parts = [int(part) for part in path.stem.split('_')]
            key = parts[0] | ((parts[1] if len(parts) > 1 else 0) << 16)
            if kind == 'spells':
                key |= 0x1000000
        index = len(entries)
        x, y = (index % COLUMNS) * CELL + 2, (index // COLUMNS) * CELL + 2
        if y + 32 > HEIGHT:
            raise ValueError('Recast icon atlas is full')
        icon = Image.open(path).convert('RGBA').resize((32, 32), Image.Resampling.LANCZOS)
        atlas.paste(icon, (x, y))
        atlas.paste(icon.crop((0, 0, 1, 32)).resize((2, 32)), (x - 2, y))
        atlas.paste(icon.crop((31, 0, 32, 32)).resize((2, 32)), (x + 32, y))
        atlas.paste(atlas.crop((x - 2, y, x + 34, y + 1)).resize((36, 2)), (x - 2, y - 2))
        atlas.paste(atlas.crop((x - 2, y + 31, x + 34, y + 32)).resize((36, 2)), (x - 2, y + 32))
        entries.append((key, index))
if not entries:
    raise ValueError('No recast icons found')
(ASSETS / 'recast.bgra').write_bytes(struct.pack('<II', WIDTH, HEIGHT) + atlas.tobytes('raw', 'BGRA'))
rows = '\n'.join(f'        {{{key:#010x}, {index}}},' for key, index in sorted(entries))
(ROOT / 'src/runtime/resources/recast_icons.hpp').write_text(
    '#pragma once\n\n#include <array>\n#include <cstdint>\n\nnamespace xiui::resources\n{\n'
    '    struct recast_icon { uint32_t key; uint16_t index; };\n'
    f'    inline constexpr uint32_t recast_icon_width = {WIDTH}, recast_icon_height = {HEIGHT};\n'
    f'    inline constexpr std::array<recast_icon, {len(entries)}> recast_icons{{{{\n{rows}\n    }}}};\n}}\n',
    encoding='utf-8',
)
print(f'Packed {len(entries)} recast icons into {WIDTH}x{HEIGHT}.')
