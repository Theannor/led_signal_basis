#!/usr/bin/env python3
"""Farbpalette von led_signal_basis per Bluetooth LE lesen oder setzen.

Beispiele:
    python tools/ble_set_colors.py "LED Signal" ff0000 00ff00   # setzen
    python tools/ble_set_colors.py "LED Signal"                 # lesen
    python tools/ble_set_colors.py "LED Signal" --clear         # zufällige Farbtöne

Braucht bleak: pip install bleak
"""
import argparse
import asyncio
import sys

from bleak import BleakClient, BleakScanner

PALETTE_CHR_UUID = "e5e16445-2813-4387-904d-6624f8aa0f03"
PALETTE_MAX = 16
SCAN_TIMEOUT_S = 10.0


def parse_color(text: str) -> bytes:
    """'#ff8000' oder 'ff8000' -> b'\\xff\\x80\\x00'"""
    value = text.removeprefix("#")
    if len(value) != 6:
        raise argparse.ArgumentTypeError(f"Farbe {text!r}: 6 Hex-Ziffern erwartet")
    try:
        return bytes.fromhex(value)
    except ValueError:
        raise argparse.ArgumentTypeError(f"Farbe {text!r}: keine Hex-Zahl") from None


def format_palette(data: bytes) -> str:
    if not data:
        return "(leer: zufällige Farbtöne)"
    return " ".join("#" + data[i:i + 3].hex() for i in range(0, len(data), 3))


async def run(name: str, colors: list[bytes], clear: bool) -> int:
    device = await BleakScanner.find_device_by_name(name, timeout=SCAN_TIMEOUT_S)
    if device is None:
        print(f"Gerät {name!r} nicht gefunden", file=sys.stderr)
        return 1
    async with BleakClient(device) as client:
        if colors or clear:
            # Mit Antwort schreiben, damit Fehler (falsche Länge) gemeldet werden
            await client.write_gatt_char(PALETTE_CHR_UUID, b"".join(colors), response=True)
        data = await client.read_gatt_char(PALETTE_CHR_UUID)
        print(format_palette(bytes(data)))
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("name", help="Bluetooth-Gerätename (Standard: LED Signal)")
    parser.add_argument("colors", nargs="*", type=parse_color, help="Farben als rrggbb")
    parser.add_argument("--clear", action="store_true", help="leere Palette setzen")
    args = parser.parse_args()
    if len(args.colors) > PALETTE_MAX:
        parser.error(f"höchstens {PALETTE_MAX} Farben")
    if args.clear and args.colors:
        parser.error("--clear und Farben zusammen geht nicht")
    return asyncio.run(run(args.name, args.colors, args.clear))


if __name__ == "__main__":
    sys.exit(main())
