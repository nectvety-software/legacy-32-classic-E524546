#!/usr/bin/env python3
"""Build the POCHITA OS OpenRhynn binary data package."""

from pathlib import Path
import struct
import zlib


ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / "sd_card" / "games" / "openrhynn" / "OpenRhynn.bin"

PACKAGE_MAGIC = b"PORHYNN1"
PAYLOAD_MAGIC = b"RHYDATA1"


def fixed(text: str, size: int) -> bytes:
    raw = text.encode("ascii")
    if len(raw) > size:
        raise ValueError(f"field too long: {text}")
    return raw.ljust(size, b"\0")


def build_payload() -> bytes:
    # This record is consumed by the native ESP32-S3 engine at launch.
    game_header = struct.pack(
        "<8s12H2I",
        PAYLOAD_MAGIC,
        1, 40,       # payload version, record size
        3, 8,        # maps, quests
        18, 7,       # mob templates/spawns, item types
        1, 40,       # starting level, HP
        7, 2,        # attack, defense
        15, 2,       # gold, potions
        0x20260716,  # content revision
        0x5248594E,  # deterministic world seed ("RHYN")
    )

    # Fixed binary content directory. It is intentionally not a text manifest.
    maps = b"".join([
        struct.pack("<BBHH16s", 0, 1, 30, 24, fixed("Elderwood", 16)),
        struct.pack("<BBHH16s", 1, 2, 30, 24, fixed("Greenwood", 16)),
        struct.pack("<BBHH16s", 2, 3, 30, 24, fixed("Sun Ruins", 16)),
    ])
    quests = b"".join(
        struct.pack("<BBHH24s", number, number - 1, target, reward, fixed(name, 24))
        for number, target, reward, name in [
            (1, 3, 25, "Wolves at the Gate"),
            (2, 1, 35, "The Moon Herb"),
            (3, 1, 50, "Forest Slimes"),
            (4, 1, 75, "The Lost Scout"),
            (5, 3, 90, "Bones in the Ruins"),
            (6, 1, 120, "Sun Crystal"),
            (7, 1, 180, "Ancient Guardian"),
            (8, 1, 250, "Elderwood Restored"),
        ]
    )
    items = b"".join(
        struct.pack("<BBHH20s", item_id, kind, power, price, fixed(name, 20))
        for item_id, kind, power, price, name in [
            (1, 0, 20, 10, "Health Potion"),
            (2, 1, 1, 0, "Moon Herb"),
            (3, 1, 1, 0, "Sun Crystal"),
            (4, 2, 3, 80, "Iron Sword"),
            (5, 2, 6, 180, "Sun Blade"),
            (6, 3, 2, 75, "Leather Armor"),
            (7, 3, 4, 170, "Guardian Armor"),
        ]
    )
    return game_header + maps + quests + items


def main() -> None:
    payload = build_payload()
    header = struct.pack(
        "<8sHHIII32s8s",
        PACKAGE_MAGIC,
        1,
        64,
        len(payload),
        zlib.crc32(payload) & 0xFFFFFFFF,
        0x00000001,  # offline/native-engine package
        fixed("OpenRhynn: Elderwood", 32),
        b"\0" * 8,
    )
    assert len(header) == 64
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    OUTPUT.write_bytes(header + payload)
    print(f"Built {OUTPUT} ({len(header) + len(payload)} bytes, CRC32={zlib.crc32(payload) & 0xFFFFFFFF:08X})")


if __name__ == "__main__":
    main()
