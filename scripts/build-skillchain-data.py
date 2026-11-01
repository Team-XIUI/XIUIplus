"""Generate native skillchain properties from the pinned XIUI Lua data files."""

import ast
import copy
import csv
import hashlib
import json
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "assets/skillchains"
NAMES = "None Liquefaction Induration Detonation Scission Impaction Reverberation Transfixion Compression Fusion Gravitation Distortion Fragmentation Light Darkness Light2 Darkness2 Radiance Umbra".split()


def read_table(path):
    result = {}
    category = None
    for line in path.read_text(encoding="utf-8-sig").splitlines():
        header = re.match(r"skills(?:\[(\d+)\]|\.(\w+))\s*=\s*\{", line)
        if header:
            category = int(header[1]) if header[1] else {"immanence": 20, "pup": 21}[header[2]]
        row = re.match(r"\s*\[(\d+)\]\s*=\s*(.*)", line)
        if not row:
            continue
        key, value = (category, int(row[1])), row[2]
        if value.startswith("false"):
            result[key] = None
            continue
        record = {}
        for field in ("en", "avatar"):
            match = re.search(field + r"='((?:\\.|[^'])*)'", value)
            if match:
                record[field] = ast.literal_eval("'" + match[1] + "'")
        props = re.search(r"skillchain=\{([^}]*)\}", value)
        if props:
            ordered = re.findall(r"'([^']+)'", props[1])
            record["properties"] = sum(1 << NAMES.index(name) for name in ordered)
            record["order"] = sum(NAMES.index(name) << (5 * index) for index, name in enumerate(ordered))
        reference = re.search(r"ref=(\d+),\s*id=(\d+)", value)
        if reference:
            record["reference"] = tuple(map(int, reference.groups()))
        if not record or category is None:
            raise ValueError(f"Unsupported property row: {line}")
        result[key] = record
    return result


def resolve(table, key, depth=0):
    record = table[key]
    if depth > 8:
        raise ValueError("Cyclic property reference")
    return resolve(table, record["reference"], depth + 1) if "reference" in record else record


def main():
    base = read_table(SOURCE / "skillchain_retail.lua")
    horizon = copy.deepcopy(base)
    for key, record in read_table(SOURCE / "skillchain_horizon.lua").items():
        if record is None:
            horizon.pop(key, None)
        elif "reference" not in record and "en" not in record:
            horizon[key] = {**horizon.get(key, {}), **record}
        else:
            horizon[key] = record
    phoenix = copy.deepcopy(base)
    pact_overlay = (ROOT / "assets/phoenix/source/modules__era__sql__soa__bloodpact_skillchains.sql").read_text()
    no_chain_pacts = set(re.findall(r"^\s*'([a-z_]+)'", pact_overlay, re.M))
    sql_names = "None Transfixion Compression Liquefaction Scission Reverberation Detonation Induration Impaction Gravitation Distortion Fusion Fragmentation Light Darkness Light2 Darkness2".split()
    for filename, category, id_key, name_key in (("weapon_skills", 3, "weaponskillid", "name"), ("blue_spell_list", 4, "spellid", None),
            ("pet_skills", 13, "pet_skill_id", "pet_skill_name"), ("mob_skills", 11, "mob_skill_id", "mob_skill_name")):
        text = (ROOT / "assets/phoenix/source" / f"sql__{filename}.sql").read_text(encoding="utf-8-sig")
        columns = re.findall(r"^  `([^`]+)`", text, re.M)
        constants = {key: int(value) for key, value in re.findall(r"SET\s+(@\w+)\s*=\s*(\d+)", text)}
        for match in re.finditer(r"^INSERT INTO `" + filename + r"` VALUES \((.*)\);", text, re.M):
            row = dict(zip(columns, next(csv.reader([match[1]], quotechar="'", escapechar="\\"))))
            key = category, int(row[id_key])
            if category == 13 and row[name_key] in no_chain_pacts:
                phoenix.pop(key, None)
                continue
            ordered = [constants[row[column]] if row[column].startswith("@") else int(row[column]) for column in ("primary_sc", "secondary_sc", "tertiary_sc")]
            ordered = [NAMES.index(sql_names[value]) for value in ordered if value]
            if not ordered:
                phoenix.pop(key, None)
                continue
            record = phoenix.get(key, {}).copy()
            record.update(properties=sum(1 << value for value in ordered), order=sum(value << (5 * index) for index, value in enumerate(ordered)))
            record.setdefault("en", row[name_key].replace("_", " ").title() if name_key else f"Spell {key[1]}")
            phoenix[key] = record
    # Pet packets use the mob-skill ID; preserve the same Phoenix properties at that wire ID.
    pet_sql = (ROOT / "assets/phoenix/source/sql__pet_skills.sql").read_text(encoding="utf-8-sig")
    pet_columns = re.findall(r"^  `([^`]+)`", pet_sql, re.M)
    for match in re.finditer(r"^INSERT INTO `pet_skills` VALUES \((.*)\);", pet_sql, re.M):
        row = dict(zip(pet_columns, next(csv.reader([match[1]], quotechar="'", escapechar="\\"))))
        record = phoenix.get((13, int(row["pet_skill_id"])))
        if record is not None:
            phoenix[11, int(row["mob_skill_id"])] = record.copy()
    output = ["#pragma once", "", "// Generated by scripts/build-skillchain-data.py; see assets/skillchains/NOTICES.txt.",
              "#include <array>", "#include <cstdint>", "#include <string_view>", "", "namespace xiui::resources", "{",
              "    struct skillchain_skill { uint16_t id; uint8_t category; uint32_t properties; uint64_t order; std::string_view name, avatar; };"]
    for name, table in (("retail", base), ("horizon", horizon), ("phoenix", phoenix)):
        output.append(f"    inline constexpr std::array<skillchain_skill, {len(table)}> skillchain_{name}{{{{")
        for (category, skill_id) in sorted(table):
            record = resolve(table, (category, skill_id))
            if "properties" not in record or "en" not in record:
                raise ValueError(f"Incomplete skill {category}/{skill_id}")
            output.append(f'        {{{skill_id}, {category}, {record["properties"]}u, {record["order"]}ull, {json.dumps(record["en"])}, {json.dumps(record.get("avatar", ""))}}},')
        output.append("    }};")
        print(f"{name}: {len(table)} property entries")
    output.append("}")
    (ROOT / "src/runtime/resources/skillchain_data.hpp").write_text("\n".join(output) + "\n", encoding="utf-8")
    hashes = {path.name: hashlib.sha256(path.read_bytes()).hexdigest() for path in SOURCE.glob("*.lua")}
    (SOURCE / "sources.json").write_text(json.dumps({"source": "Local XIUI modules/hotbar/database; Retail base and Horizon overlay; pinned Phoenix SQL/era inputs in assets/phoenix", "phoenix_manifest": "../phoenix/sources.json", "sha256": hashes}, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
