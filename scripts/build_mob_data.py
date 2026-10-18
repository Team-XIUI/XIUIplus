"""Pack HeadsUp's pinned Phoenix mob snapshot for the embedded offline lookup."""

import argparse
import csv
from pathlib import Path
import struct


def build(source, destination):
    record = struct.Struct('<5I2Hh12h8b10b4B')
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
            damage = [int(value) for value in row['sdt'].split(',')]
            elements = [int(value) for value in row['elementRanks'].split(',')]
            statuses = [int(value) for value in row['statusRanks'].split(',')]
            if (len(damage), len(elements), len(statuses)) != (12, 8, 10):
                raise ValueError('Unexpected Phoenix resistance column lengths.')
            records.extend(record.pack(identifier, offsets[name], int(row['respawn']), int(row['placeholderOf']),
                int(row['immunity']), int(row['zone']), int(row['detects']), int(row['expLevelMod']),
                *damage, *elements, *statuses, int(row['minLevel']), int(row['maxLevel']), int(row['flags']), 0))
    count = len(records) // record.size
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_bytes(struct.pack('<4s3I', b'XIM1', count, record.size, len(names)) + records + names)
    print(f'{destination}: {count} mobs, {record.size}-byte records, {len(names)} name bytes')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('destination', type=Path)
    arguments = parser.parse_args()
    build(arguments.source, arguments.destination)
