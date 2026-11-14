"""Bundle Phoenix wiki maps and Remapster's calibrated minimap art as image records."""
import argparse
import concurrent.futures
import io
import json
from pathlib import Path
import struct
import time
import urllib.request

from PIL import Image, ImageChops, ImageDraw

ROOT = Path(__file__).resolve().parents[2]


def restore_edge_alpha(image):
    red, green, blue, _ = image.split()
    brightness = ImageChops.lighter(ImageChops.lighter(red, green), blue)
    outside = brightness.point(lambda value: 255 if value < 96 else 0)
    band = max(1, image.width // 32)
    ImageDraw.Draw(outside).rectangle((band, band, image.width - band - 1, image.height - band - 1), fill=0)
    for position in range(image.width):
        for point in ((position, 0), (position, image.height - 1), (0, position), (image.width - 1, position)):
            if outside.getpixel(point) == 255:
                ImageDraw.floodfill(outside, point, 128)
    connected = outside.point(lambda value: 255 if value == 128 else 0)
    alpha = brightness.point(lambda value: max(0, min(255, (value - 16) * 255 // 80)))
    image.putalpha(Image.composite(alpha, image.getchannel("A"), connected))
    return image


def fetch(url, path):
    if path.exists():
        return path.read_bytes()
    for attempt in range(3):
        try:
            request = urllib.request.Request(url, headers={"User-Agent": "XIUI-Map-Builder/1.0"})
            with urllib.request.urlopen(request, timeout=30) as response:
                data = response.read(8 * 1024 * 1024 + 1)
            if len(data) > 8 * 1024 * 1024:
                raise ValueError("Map exceeds download limit")
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)
            return data
        except Exception:
            if attempt == 2:
                raise
            time.sleep(attempt + 1)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("wiki", type=Path, nargs="?", help="phoenix-platform/standalone/wiki")
    parser.add_argument("--cached", action="store_true", help="Rebuild existing map entries from cached original images")
    args = parser.parse_args()
    cache = ROOT / "out/minimap-downloads"
    cache.mkdir(parents=True, exist_ok=True)
    output = ROOT / "assets/minimap"
    jobs = []
    if args.cached:
        catalog = json.loads((output / "catalog.json").read_text(encoding="utf-8"))
        packed = (output / "maps.bin").read_bytes()
        magic, count = struct.unpack_from("<4sI", packed)
        if magic != b"XMAP" or count == 0 or count > 2048 or len(packed) < 8 + count * 16:
            raise ValueError("Invalid existing map index")
        for index in range(count):
            zone, floor, style = struct.unpack_from("<HhB", packed, 8 + index * 16)
            jobs.append((zone, floor, style, None))
    else:
        if args.wiki is None:
            parser.error("provide a wiki directory or --cached")
        catalog = json.loads((args.wiki / "assets/zone-maps.json").read_text(encoding="utf-8"))
        commit = catalog["remapsterCommit"]
        tree = json.loads(fetch(
            f"https://api.github.com/repos/AkadenTK/remapster_maps/git/trees/{commit}?recursive=1",
            cache / f"tree-{commit}.json"))
        enhanced = {Path(entry["path"]).stem: entry["path"] for entry in tree["tree"]
                    if entry["path"].startswith("minimaps/ffxidb/") and entry["path"].endswith(".png")}
        for zone in catalog["zones"]:
            for floor in zone["original"]:
                key = f'{zone["id"]}_{floor}'
                jobs.append((zone["id"], floor, 0,
                             f"https://cdn.phoenix-xi.com/wiki/maps/v1/original/{key}.webp"))
                # Native floor IDs can be nonsequential; wiki page numbers cannot select floors.
                if key in enhanced:
                    jobs.append((zone["id"], floor, 1,
                                 f"https://raw.githubusercontent.com/AkadenTK/remapster_maps/{commit}/{enhanced[key]}"))
    commit = catalog["remapsterCommit"]

    def prepare(job):
        zone, floor, style, url = job
        original = cache / f"{zone}_{floor}_{style}.source"
        raw = original.read_bytes() if url is None else fetch(url, original)
        target = cache / f"{zone}_{floor}_{style}-512-webp-q90-m4-alpha.webp"
        if not target.exists() or target.stat().st_mtime_ns < original.stat().st_mtime_ns:
            with Image.open(io.BytesIO(raw)) as source:
                image = source.convert("RGBA")
                opaque = image.getchannel("A").getextrema() == (255, 255)
                if image.width != image.height:
                    raise ValueError(f"Non-square source: {url}")
                size = 512
                if image.width > size:
                    image = image.resize((size, size), Image.Resampling.LANCZOS)
                if opaque:
                    image = restore_edge_alpha(image)
                image.save(target, format="WEBP", quality=90, method=4)
        return zone, floor, style, target.read_bytes()

    records = []
    with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
        for index, result in enumerate(pool.map(prepare, jobs), 1):
            records.append(result)
            if index % 50 == 0 or index == len(jobs):
                print(f"{index}/{len(jobs)} maps", flush=True)
    records.sort(key=lambda row: row[:3])
    offset = 8 + len(records) * 16
    header = bytearray(struct.pack("<4sI", b"XMAP", len(records)))
    payload = bytearray()
    for zone, floor, style, image in records:
        header.extend(struct.pack("<HhB3xII", zone, floor, style, offset, len(image)))
        payload.extend(image)
        offset += len(image)
    output.mkdir(parents=True, exist_ok=True)
    (output / "maps.bin").write_bytes(header + payload)
    (output / "catalog.json").write_text(json.dumps({
        "schema": 2, "remapsterCommit": commit,
        "format": "WebP", "quality": 90, "maximumSize": 512,
        "alpha": "Original transparency; edge-connected black matte fallback for opaque sources",
        "regular": sum(row[2] == 0 for row in records),
        "enhanced": sum(row[2] == 1 for row in records),
        "source": "Phoenix wiki zone-maps.json; Remapster minimaps/ffxidb",
    }, indent=2) + "\n", encoding="utf-8")
    if not args.cached:
        license_data = fetch(
            f"https://raw.githubusercontent.com/AkadenTK/remapster_maps/{commit}/LICENSE",
            cache / "Remapster-LICENSE.txt")
        (output / "LICENSE.txt").write_bytes(license_data)
    print(f"Wrote {offset / 1024 / 1024:.1f} MiB to {output / 'maps.bin'}")


if __name__ == "__main__":
    main()
