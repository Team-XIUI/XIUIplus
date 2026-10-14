"""Extract numeric attack/recovery modifiers from the Phoenix SQL tables."""

import argparse
import csv
from pathlib import Path
import re


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("phoenix", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    wanted = {71, 72, 73, 167, 171, 173, 217, 259, 295, 369, 370, 380, 383, 384}
    items = []
    for item, mod, value in re.findall(r"VALUES \((\d+),(\d+),(-?\d+)\)", (args.phoenix / "sql/item_mods.sql").read_text(encoding="utf-8")):
        if int(mod) in wanted:
            items.append((int(item), int(mod), int(value)))
    traits = []
    for row in re.findall(r"VALUES \(([^\n]+)\);", (args.phoenix / "sql/traits.sql").read_text(encoding="utf-8")):
        values = next(csv.reader([row], quotechar="'", skipinitialspace=True))
        trait, _, job, level, _, mod, amount, content, merit = values
        if int(mod) in wanted and int(level) <= 75 and content == "NULL" and int(merit) == 0:
            traits.append(tuple(map(int, (trait, job, level, mod, amount))))
    lines = ["#pragma once", "", "#include <array>", "#include <cstdint>", "", "namespace xiui::resources", "{",
             "    // Numeric facts from Phoenix SQL; regenerate with scripts/build-timing-rules.py.",
             "    struct timing_item_mod { uint16_t item, mod; int32_t value; };",
             "    struct timing_trait { uint16_t trait, job, level, mod; int32_t value; };",
             f"    inline constexpr std::array<timing_item_mod, {len(items)}> timing_items{{{{"]
    lines += ["        {" + ", ".join(map(str, row)) + "}," for row in sorted(items)]
    lines += ["    }};", f"    inline constexpr std::array<timing_trait, {len(traits)}> timing_traits{{{{"]
    lines += ["        {" + ", ".join(map(str, row)) + "}," for row in sorted(traits)]
    lines += ["    }};", "}", ""]
    args.output.write_text("\n".join(lines), encoding="utf-8")
    print(f"Extracted {len(items)} item modifiers and {len(traits)} job trait rows.")


if __name__ == "__main__":
    main()
