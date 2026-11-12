"""Pack HeadsUp's pinned Phoenix mob snapshot for the embedded offline lookup."""

import argparse
import csv
from pathlib import Path
import struct

# Stored as index + 1 (0 is none); mob_database.cpp's addEffectNames follows this order.
ADD_EFFECTS = ['Unknown', 'Fire Damage', 'Ice Damage', 'Wind Damage', 'Earth Damage', 'Thunder Damage', 'Water Damage',
    'Light Damage', 'Dark Damage', 'HP Drain', 'MP Drain', 'TP Drain', 'Poison', 'Paralysis', 'Blindness', 'Silence',
    'Slow', 'Stun', 'Bind', 'Weight', 'Sleep', 'Petrify', 'Plague', 'Curse I', 'Terror', 'Amnesia', 'Mute',
    'Attack Down', 'Defense Down', 'Evasion Down', 'Absorb Status']
ADD_EFFECT_ALIASES = {'Petrification': 'Petrify'}


def numbers(row, column, count):
    values = [int(value) for value in row[column].split(',')]
    if len(values) != count:
        raise ValueError(f'{row["id"]}: expected {count} values in {column}.')
    return values


def read_info(source):
    info = {}
    with source.open(encoding='utf-8', newline='') as stream:
        for row in csv.DictReader(stream, delimiter='\t'):
            main, sub = numbers(row, 'jobs', 2)
            traits = int(row['traits'] or 0)
            absorb = numbers(row, 'absorb', 8)
            if not (0 <= main <= 22 and 0 <= sub <= 22) or not 0 <= traits < 32 or any(value not in (0, 100) for value in absorb):
                raise ValueError(f'{row["id"]}: unsupported jobs, traits or partial absorb.')
            effect = ADD_EFFECT_ALIASES.get(row['addEffect'], row['addEffect'])
            if effect and effect not in ADD_EFFECTS:
                raise ValueError(f'{row["id"]}: unknown additional effect {effect!r}.')
            info[int(row['id'])] = (main, sub, traits, ADD_EFFECTS.index(effect) + 1 if effect else 0,
                sum(1 << bit for bit, value in enumerate(absorb) if value), numbers(row, 'damageTaken', 4))
    return info


def build(source, info_source, destination):
    record = struct.Struct('<5I2Hh12h4h8b10b8B')
    info = read_info(info_source)
    names = bytearray()
    offsets = {}
    records = bytearray()
    previous = 0
    with source.open(encoding='utf-8', newline='') as stream:
        for row in csv.DictReader(stream, delimiter='\t'):
            identifier = int(row['id'])
            if identifier <= previous:
                raise ValueError('Mob server IDs must be strictly increasing.')
            previous = identifier
            name = row['name']
            if name not in offsets:
                offsets[name] = len(names)
                names.extend(name.encode('utf-8') + b'\0')
            main, sub, traits, effect, absorbs, taken = info.pop(identifier)
            records.extend(record.pack(identifier, offsets[name], int(row['respawn']), int(row['placeholderOf']),
                int(row['immunity']), int(row['zone']), int(row['detects']), int(row['expLevelMod']),
                *numbers(row, 'sdt', 12), *taken, *numbers(row, 'elementRanks', 8), *numbers(row, 'statusRanks', 10),
                int(row['minLevel']), int(row['maxLevel']), int(row['flags']), main, sub, traits, effect, absorbs))
    if info:
        raise ValueError(f'{len(info)} mob info rows have no matching mob.')
    count = len(records) // record.size
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_bytes(struct.pack('<4s3I', b'XIM2', count, record.size, len(names)) + records + names)
    print(f'{destination}: {count} mobs, {record.size}-byte records, {len(names)} name bytes')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path, help='phoenix_mobs.tsv')
    parser.add_argument('info', type=Path, help='phoenix_mob_info.tsv from the same HeadsUp commit')
    parser.add_argument('destination', type=Path)
    arguments = parser.parse_args()
    build(arguments.source, arguments.info, arguments.destination)
