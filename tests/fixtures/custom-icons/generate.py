"""Regenerate small synthetic raster fixtures with Pillow."""

from pathlib import Path
from PIL import Image

root = Path(__file__).resolve().parent
image = Image.new("RGBA", (16, 8), (255, 0, 0, 255))
for x in range(8, 16):
    for y in range(8):
        image.putpixel((x, y), (0, 0, 255, 128))
rgb = image.convert("RGB")
second = Image.new("RGB", image.size, (0, 255, 0))
rgb.save(root / "sample.jpg", quality=95)
rgb.save(root / "sample.bmp")
rgb.save(root / "animated.gif", save_all=True, append_images=[second], duration=100, loop=0)
rgb.save(root / "pages.tiff", save_all=True, append_images=[second], compression="raw")
image.resize((16, 16), Image.Resampling.NEAREST).save(root / "sample.ico", sizes=[(16, 16)])
image.save(root / "sample.webp", lossless=True)
