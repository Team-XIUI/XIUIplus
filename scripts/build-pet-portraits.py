"""Pack the retained XIUI pet portraits into the native Pet atlas."""

from argparse import ArgumentParser
from io import BytesIO
from pathlib import Path
import shutil
import struct
from zipfile import ZIP_DEFLATED, ZipFile

from PIL import Image, ImageOps

ROOT = Path(__file__).resolve().parents[1]
ASSETS = ROOT / "assets/pets"
AVATARS = "carbuncle ifrit shiva garuda titan ramuh leviathan fenrir diabolos atomos odin alexander caitsith siren".split()
SPIRITS = "fire ice wind earth thunder water light dark".split()
PORTRAITS = [f"avatars/{name}.png" for name in AVATARS] + [f"spirits/{name}spirit.png" for name in SPIRITS] + ["drg_wyvern.png", "jug.png"]


def build(source):
    ASSETS.mkdir(parents=True, exist_ok=True)
    archive_path = ASSETS / "sources.zip"
    if source:
        with ZipFile(archive_path, "w", ZIP_DEFLATED) as archive:
            for name in PORTRAITS:
                archive.writestr(name, (source / name).read_bytes())
        shutil.copyfile(source.parent.parent.parent / "LICENSE", ASSETS / "LICENSE.txt")
    atlas = Image.new("RGBA", (1024, 512))
    with ZipFile(archive_path) as archive:
        for index, name in enumerate(PORTRAITS):
            with Image.open(BytesIO(archive.read(name))) as original:
                image = original.convert("RGBA")
            image = ImageOps.contain(image, (128, 128), Image.Resampling.LANCZOS)
            atlas.paste(image, ((index % 8) * 128 + (128 - image.width) // 2,
                                (index // 8) * 128 + (128 - image.height) // 2))
    (ASSETS / "portraits.bgra").write_bytes(struct.pack("<II", *atlas.size) + atlas.tobytes("raw", "BGRA"))
    print(f"Pet portraits: {len(PORTRAITS)} images, 1024x512 atlas")


if __name__ == "__main__":
    parser = ArgumentParser()
    parser.add_argument("--import-from", type=Path, help="Capture original XIUI/assets/pets source images.")
    build(parser.parse_args().import_from)
