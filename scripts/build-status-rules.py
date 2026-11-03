"""Import spell behavior and fixed durations from retained XIUI data without executing Lua."""

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
    parser.add_argument("--check", action="store_true", help="Verify the generated header without rewriting it.")
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
    spell_rules = []
    for spell, row in sorted(data["spells"].items()):
        statuses = set(row.get("buffIds", {}).values())
        if "buffId" in row:
            statuses.add(row["buffId"])
        statuses = sorted(statuses or {0})
        primary = row.get("buffId", statuses[0])
        mode = "guaranteed" if row.get("applyOnDamage") == "true" else "possible" if row.get("onDamage") == "true" else "none"
        clears = list(row.get("clearsBuffs", {}).values())
        assert len(clears) <= 1, f"Spell {spell} needs multiple clear-effect fields"
        for status in statuses:
            display = row.get("displayBuffId", status)
            seconds = row.get("buffDurations", {}).get(status, row["duration"])
            assert 0 <= status <= 1023 and 0 <= display <= 1023 and 0 < seconds <= 65535
            assert mode == "none" or status != 0, f"Spell {spell} needs an effect ID for damage inference"
            spell_rules.append((spell, status, display, seconds, clears[0] if clears else 0, mode, status == primary))
    lines += ["    enum class spell_damage_kind : uint8_t { none, guaranteed, possible };",
              "    struct spell_status_rule",
              "    {",
              "        uint16_t spell, status, display, seconds, clears;",
              "        spell_damage_kind damage;",
              "        bool primary;",
              "    };",
              f"    inline constexpr uint32_t xiui_spell_count = {len(data['spells'])};",
              f"    inline constexpr std::array<spell_status_rule, {len(spell_rules)}> xiui_spell_status_rules{{{{"]
    lines.extend(f"        {{{spell}, {status}, {display}, {seconds}, {clears}, spell_damage_kind::{mode}, {str(primary).lower()}}},"
                 for spell, status, display, seconds, clears, mode, primary in spell_rules)
    lines.append("    }};")
    for name, entries in rows.items():
        lines.append(f"    inline constexpr std::array<status_duration_rule, {len(entries)}> xiui_{name}_durations{{{{")
        lines.extend(f"        {{{action}, {status}, {seconds}}}," for (action, status), seconds in sorted(entries.items()))
        lines.append("    }};")
    lines += ["}", ""]
    generated = "\n".join(lines)
    if args.check:
        if not args.output.exists() or args.output.read_text(encoding="utf-8-sig") != generated:
            parser.error(f"{args.output} does not match the retained spell database")
    else:
        args.output.write_text(generated, encoding="utf-8")
    print(f"XIUI spell behavior: {len(data['spells'])} spells, {len(spell_rules)} effect rules. Fixed durations: " +
          ", ".join(f"{name}={len(entries)}" for name, entries in rows.items()))


if __name__ == "__main__":
    main()
