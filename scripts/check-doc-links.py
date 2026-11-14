"""Check local Markdown file targets; external URLs and heading anchors are excluded."""
from pathlib import Path
import re
from urllib.parse import unquote

ROOT = Path(__file__).resolve().parents[1]


def main():
    files = [ROOT / "README.md", ROOT / "AGENTS.md", ROOT / "third_party/README.md"]
    for directory in ("docs", "assets", "tests"):
        files.extend((ROOT / directory).rglob("*.md"))
    errors = []
    for path in files:
        fenced = False
        for number, line in enumerate(path.read_text(encoding="utf-8-sig").splitlines(), 1):
            if line.lstrip().startswith(("```", "~~~")):
                fenced = not fenced
                continue
            if fenced:
                continue
            for target in re.findall(r"\]\(([^)]+)\)", line):
                target = target.split(' "', 1)[0].strip().strip("<>").split("#", 1)[0]
                if not target or target.startswith("/") or re.match(r"^[A-Za-z][A-Za-z0-9+.-]*:", target):
                    continue
                target = re.sub(r":\d+$", "", unquote(target))
                if not (path.parent / target).exists():
                    errors.append(f"{path.relative_to(ROOT)}:{number}: {target}")
    for error in errors:
        print(error)
    print(f"Checked {len(files)} Markdown files; {len(errors)} missing local targets.")
    return bool(errors)


if __name__ == "__main__":
    raise SystemExit(main())
