"""Import fixed durations from the retained XIUI Lua database; no Lua runtime needed."""

import argparse
import hashlib
from pathlib import Path
import re


def tables(text):
    tokens = re.findall(r"'[^']*'|\"[^\"]*\"|\d+|\w+|[{}\[\]=,;]", re.sub(r"--[^\n]*", "", text))

    def value(at):
        token = tokens[at]
        if token != "{":
            return (int(token) if token.isdigit() else token.strip("'\"")), at + 1
        result, at, ordinal = {}, at + 1, 1
        while tokens[at] != "}":
            if tokens[at] in (",", ";"):
                at += 1
                continue
            if tokens[at] == "[":
                key, at = value(at + 1)
                assert tokens[at:at + 2] == ["]", "="]
                at += 2
            elif tokens[at + 1] == "=":
                key, at = tokens[at], at + 2
            else:
                key, ordinal = ordinal, ordinal + 1
            result[key], at = value(at)
        return result, at + 1

    result = {}
    for at in range(len(tokens) - 3):
        if tokens[at] == "durations" and tokens[at + 2:at + 4] == ["=", "{"]:
            result[tokens[at + 1]], _ = value(at + 3)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    data = tables(args.source.read_text(encoding="utf-8-sig"))
    assert {"spells", "weaponSkills", "ja", "jaPhysical", "pet", "additionalEffect", "mobBuff"} <= data.keys()
    fallback = {status: row["duration"] for status, row in data["additionalEffect"].items()}
    fallback.update(data["mobBuff"])
    maximum = {}
    rows = {}
    for name in ("spells", "weaponSkills", "jaPhysical", "ja", "pet"):
        entries = {}
        for action, row in data[name].items():
            seconds = row.get("duration", 0)
            if not isinstance(seconds, int) or seconds <= 0:
                continue
            statuses = set(row.get("buffIds", {}).values())
            if "buffId" in row:
                statuses.add(row["buffId"])
            for status in statuses:
                maximum[status] = max(maximum.get(status, 0), seconds)
            for status in statuses or {0}:
                entries[action, status] = row.get("buffDurations", {}).get(status, seconds)
        rows[name] = entries
    fallback.update(maximum)
    rows["ja"] = rows.pop("jaPhysical") | rows["ja"]
    rows["fallback"] = {(status, 0): seconds for status, seconds in fallback.items()}
    rows["additional"] = {(status, 0): row["duration"] for status, row in data["additionalEffect"].items()}
    digest = hashlib.sha256(args.source.read_bytes()).hexdigest()
    lines = ["#pragma once", "", "#include <array>", "#include <cstdint>", "", "namespace xiui::resources", "{",
             "    // Generated from assets/status/debuff_retail.lua by scripts/build-status-rules.py.",
             f"    // XIUI database SHA-256: {digest}",
             "    struct status_duration_rule { uint16_t action, status, seconds; };"]
    for name, entries in rows.items():
        lines.append(f"    inline constexpr std::array<status_duration_rule, {len(entries)}> xiui_{name}_durations{{{{")
        lines.extend(f"        {{{action}, {status}, {seconds}}}," for (action, status), seconds in sorted(entries.items()))
        lines.append("    }};")
    lines += ["}", ""]
    args.output.write_text("\n".join(lines), encoding="utf-8")
    print("Imported XIUI fixed durations: " + ", ".join(f"{name}={len(entries)}" for name, entries in rows.items()))


if __name__ == "__main__":
    main()
