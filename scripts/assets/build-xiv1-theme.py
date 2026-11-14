"""Rebuild the XIV 1.0 theme atlas from the supplied screenshot crops."""

from pathlib import Path
import math
import struct

from PIL import Image, ImageChops, ImageDraw, ImageStat


ROOT = Path(__file__).resolve().parents[2] / "assets" / "xiv1"
SOURCE = ROOT / "source"
atlas = Image.new("RGBA", (256, 256))


def source(name):
    return Image.open(SOURCE / f"{name}.png").convert("RGBA")


def place(image, x, y):
    atlas.paste(image, (x, y))


def framed_patch(image, x, y):
    """Extrude one texel around atlas patches for scaled linear filtering."""
    padded = Image.new("RGBA", (image.width + 2, image.height + 2))
    padded.paste(image, (1, 1))
    padded.paste(image.crop((0, 0, image.width, 1)), (1, 0))
    padded.paste(image.crop((0, image.height - 1, image.width, image.height)), (1, image.height + 1))
    padded.paste(padded.crop((1, 0, 2, padded.height)), (0, 0))
    padded.paste(padded.crop((image.width, 0, image.width + 1, padded.height)), (image.width + 1, 0))
    place(padded, x - 1, y - 1)


def frame_corner(image, right=False, bottom=False):
    """Keep the complete photographed rim; mask only its rounded silhouette."""
    result = image.copy()
    pixels = result.load()
    for y in range(result.height):
        for x in range(result.width):
            r, g, b, _ = pixels[x, y]
            coverage = 0
            for sy in range(4):
                for sx in range(4):
                    px = x + (sx + .5) / 4
                    py = y + (sy + .5) / 4
                    if right:
                        px = result.width - px
                    if bottom:
                        py = result.height - py
                    distance = math.hypot(px - 12, py - 12)
                    outer = px >= 12 or py >= 12 or distance <= 12
                    inner = px >= 8 and py >= 8 and (px >= 12 or py >= 12 or distance <= 4)
                    coverage += outer and not inner
            alpha = round(255 * coverage / 16)
            pixels[x, y] = (r, g, b, alpha)
    return result


framed_patch(frame_corner(source("menu-top-left-refined")), 1, 1)
framed_patch(frame_corner(source("menu-top-right-refined"), right=True), 27, 1)
framed_patch(frame_corner(source("menu-bottom-left-refined"), bottom=True), 53, 1)
framed_patch(frame_corner(source("menu-bottom-right-refined"), right=True, bottom=True), 79, 1)
framed_patch(source("menu-top-refined"), 1, 33)
framed_patch(source("menu-bottom-refined"), 1, 45)
framed_patch(source("menu-left-refined"), 69, 33)
framed_patch(source("menu-right-refined"), 81, 33)

# Mirroring joins the original fine fabric without a hard repeating seam.
fabric = source("menu-fabric-refined").resize((32, 32), Image.Resampling.LANCZOS)
tile = Image.new("RGBA", (64, 64))
tile.paste(fabric, (0, 0))
tile.paste(fabric.transpose(Image.Transpose.FLIP_LEFT_RIGHT), (32, 0))
tile.paste(fabric.transpose(Image.Transpose.FLIP_TOP_BOTTOM), (0, 32))
tile.paste(fabric.transpose(Image.Transpose.ROTATE_180), (32, 32))
place(tile, 160, 0)

# Keep the body nearly white: resource gradients supply its salmon/olive color.
# The original gauge has a narrow shaded bevel and a bright upper highlight.
fill = Image.new("RGBA", (64, 8))
fill_pixels = fill.load()
for y in range(8):
    luminance = (200, 247, 255, 247, 229, 241, 248, 218)[y]
    for x in range(64):
        fill_pixels[x, y] = (luminance, luminance, luminance, 255)
place(fill, 96, 72)
track_source = source("gauge-track-refined")
track = Image.new("RGBA", (64, 8))
draw = ImageDraw.Draw(track)
for y in range(8):
    color = tuple(round(value) for value in ImageStat.Stat(track_source.crop((0, y, track_source.width, y + 1))).mean)
    draw.line((0, y, 63, y), fill=color)
place(track, 96, 88)

body = source("gauge-body-refined")
rim = Image.new("RGBA", (64, 14))
for y in range(14):
    if y < 3 or y >= 11:
        color = tuple(round(value) for value in ImageStat.Stat(body.crop((0, y, body.width, y + 1))).mean)
        ImageDraw.Draw(rim).line((0, y, 63, y), fill=color)
place(rim, 0, 112)
for name, position, reverse in (("gauge-cap-left-refined", 72, False), ("gauge-cap-right-refined", 88, True)):
    cap = source(name)
    silhouette = (7, 6, 5, 3, 2, 1, 1, 1, 1, 2, 3, 5, 6, 7)
    for y in range(14):
        for x in range(9):
            r, g, b, _ = cap.getpixel((x, y))
            side = 8 - x if reverse else x
            alpha = 255 if side >= silhouette[y] else 0
            if alpha and side == silhouette[y]:
                alpha = 170
            if b > r * 1.12 and g > r * 1.03:
                alpha = 0
            cap.putpixel((x, y), (r, g, b, alpha))
    place(cap, position, 112)

underline = source("target-underline")
for y in range(underline.height):
    for x in range(130, underline.width):
        underline.putpixel((x, y), underline.getpixel((120, y)))
for y in range(underline.height):
    for x in range(underline.width):
        r, g, b, _ = underline.getpixel((x, y))
        alpha = max(0, min(255, round((min(r, g) - 45) * 3)))
        if g > r * 1.03 or y < 4 or y > 10 or x < 5 or (x > 50 and y not in (7, 8)):
            alpha = 0
        alpha = round(alpha * min(1, (underline.width - x) / 18))
        underline.putpixel((x, y), (r, g, b, alpha))
place(underline, 0, 144)
medallion = source("action-medallion")
clean_medallion = medallion.copy()
for y in range(medallion.height):
    for x in range(medallion.width):
        # The source's lower/right rim overlaps a cast bar and the scene.
        clean_medallion.putpixel((x, y), medallion.getpixel((min(x, 58 - x), min(y, 59 - y))))
medallion = clean_medallion
for y in range(medallion.height):
    for x in range(medallion.width):
        r, g, b, _ = medallion.getpixel((x, y))
        radius = math.hypot(x - 29, y - 29)
        alpha = round(max(0, min(1, radius - 21)) * max(0, min(1, 30 - radius)) * 255)
        if g > r * 1.03:
            alpha = 0
        medallion.putpixel((x, y), (r, g, b, alpha))
place(medallion, 192, 144)

for y in range(atlas.height):
    for x in range(atlas.width):
        r, g, b, alpha = atlas.getpixel((x, y))
        if alpha < 8:
            atlas.putpixel((x, y), (0, 0, 0, 0))

atlas.save(ROOT / "theme.png")
(ROOT / "theme.bgra").write_bytes(struct.pack("<II", *atlas.size) + atlas.tobytes("raw", "BGRA"))
