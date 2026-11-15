"""Generate the Hy3x catalog and preview PNGs from the installed CS2 archives."""

import argparse
import json
import re
import shutil
import struct
from pathlib import Path
from uuid import uuid4

from PIL import Image
from catalog_outputs import digest, make_web_catalog, read_inventory


ROOT = Path(__file__).resolve().parents[1]
ITEM_ICONS = "panorama/images/econ/default_generated/"
FIREARMS = {
    7: ("AK-47", "rifle", [44, 724, 921, 180, 302, 1171, 801, 282, 1035, 707]),
    9: ("AWP", "rifle", [344, 756, 736, 446, 1026]),
    60: ("M4A1-S", "rifle", [326, 445, 1001, 1017, 430, 984, 548]),
    16: ("M4A4", "rifle", [309, 449, 1063, 512, 695]),
    1: ("Desert Eagle", "pistol", [37, 757, 962, 469, 328]),
    61: ("USP-S", "pistol", [1065, 1027, 504, 313, 1142]),
    4: ("Glock-18", "pistol", [38, 437, 988, 1119, 1120, 1121, 1122, 1123]),
}
GLOVES = {
    5030: ("Sport Gloves", [10037, 10038, 10018, 10048, 10045, 10073]),
    5034: ("Specialist Gloves", [10033, 10063, 10064, 10065, 10068]),
    5031: ("Driver Gloves", [10042, 10041, 10070, 10013]),
    5033: ("Moto Gloves", [10027, 10079, 10078]),
    5027: ("Bloodhound Gloves", [10006, 10007]),
    5032: ("Hand Wraps", [10053, 10021, 10084]),
    5035: ("Hydra Gloves", [10057, 10060]),
}
KNIVES = {
    500: "Bayonet", 503: "Classic Knife", 505: "Flip Knife", 506: "Gut Knife",
    507: "Karambit", 508: "M9 Bayonet", 509: "Huntsman Knife", 512: "Falchion Knife",
    514: "Bowie Knife", 515: "Butterfly Knife", 516: "Shadow Daggers",
    517: "Paracord Knife", 518: "Survival Knife", 519: "Ursus Knife",
    520: "Navaja Knife", 521: "Nomad Knife", 522: "Stiletto Knife",
    523: "Talon Knife", 525: "Skeleton Knife", 526: "Kukri Knife",
}
KNIFE_FINISHES = {
    44: "Case Hardened", 38: "Fade", 413: "Marble Fade", 12: "Crimson Web",
    409: "Tiger Tooth", 59: "Slaughter",
    415: "Doppler - Ruby", 416: "Doppler - Sapphire", 417: "Doppler - Black Pearl",
    418: "Doppler - Phase 1", 419: "Doppler - Phase 2", 420: "Doppler - Phase 3",
    421: "Doppler - Phase 4", 617: "Doppler - Black Pearl", 618: "Doppler - Phase 2",
    619: "Doppler - Sapphire", 568: "Gamma Doppler - Emerald",
    569: "Gamma Doppler - Phase 1", 570: "Gamma Doppler - Phase 2",
    571: "Gamma Doppler - Phase 3", 572: "Gamma Doppler - Phase 4",
    852: "Doppler - Phase 1", 853: "Doppler - Phase 2", 854: "Doppler - Phase 3",
    855: "Doppler - Phase 4", 856: "Marble Fade",
}
EXTRA_KNIFE_FINISHES = {"Lore", "Autotronic", "Black Laminate", "Blue Steel",
                       "Stained", "Damascus Steel", "Ultraviolet", "Freehand"}
GLOCK_GAMMA = {
    1119: "Gamma Doppler - Emerald", 1120: "Gamma Doppler - Phase 1",
    1121: "Gamma Doppler - Phase 2", 1122: "Gamma Doppler - Phase 3",
    1123: "Gamma Doppler - Phase 4",
}


class Archive:
    def __init__(self, path):
        self.path = path
        self.index = path.read_bytes()
        signature, version, tree_size = struct.unpack_from("<III", self.index)
        if (signature, version) != (0x55AA1234, 2):
            raise ValueError("Expected a version 2 VPK directory")
        self.data_offset = 28 + tree_size
        self.entries = {}
        self.position = 28
        while extension := self.read_string():
            while directory := self.read_string():
                while name := self.read_string():
                    _, count, archive, offset, length, end = struct.unpack_from(
                        "<IHHIIH", self.index, self.position
                    )
                    self.position += 18
                    if end != 0xFFFF:
                        raise ValueError("Invalid VPK entry")
                    preload = self.index[self.position:self.position + count]
                    self.position += count
                    self.entries[f"{directory}/{name}.{extension}"] = (archive, offset, length, preload)

    def read_string(self):
        end = self.index.index(0, self.position)
        value = self.index[self.position:end].decode()
        self.position = end + 1
        return value

    def read(self, name):
        archive, offset, size, preload = self.entries[name]
        if not size:
            return preload
        if archive == 0x7FFF:
            start = self.data_offset + offset
            return preload + self.index[start:start + size]
        with self.path.with_name(f"pak01_{archive:03}.vpk").open("rb") as file:
            file.seek(offset)
            return preload + file.read(size)


def lz4_block(data, expected_size):
    decoded = bytearray()
    pos = 0

    def length(nibble):
        nonlocal pos
        if nibble == 15:
            while True:
                extra = data[pos]
                pos += 1
                nibble += extra
                if extra != 255:
                    break
        return nibble

    while pos < len(data):
        token = data[pos]
        pos += 1
        literals = length(token >> 4)
        decoded.extend(data[pos:pos + literals])
        pos += literals
        if pos == len(data):
            break
        distance = struct.unpack_from("<H", data, pos)[0]
        pos += 2
        count = length(token & 15) + 4
        if not 0 < distance <= len(decoded) or len(decoded) + count > expected_size:
            raise ValueError("Invalid LZ4 mip")
        match = decoded[-distance:]
        decoded.extend((match * ((count + distance - 1) // distance))[:count])
    if len(decoded) != expected_size:
        raise ValueError("Incorrect mip size")
    return bytes(decoded)


def decode_icon(data):
    payload, _, version, relative, blocks = struct.unpack_from("<IHHII", data)
    if version != 1:
        raise ValueError("Unsupported texture resource version")
    for index in range(blocks):
        block = 8 + relative + 12 * index
        tag, offset, _ = struct.unpack_from("<4sII", data, block)
        if tag == b"DATA":
            start = block + 4 + offset
            break
    else:
        raise ValueError("Missing texture data")

    width, height, depth, fmt, levels, _, relative, extras = struct.unpack_from("<HHHBBIII", data, start + 20)
    if depth != 1 or fmt not in (1, 2, 4, 20, 28):
        raise ValueError(f"Unsupported icon texture format {fmt}")
    sizes = []
    compression = 0
    for index in range(extras):
        entry = start + 32 + relative + index * 12
        kind, offset, _ = struct.unpack_from("<III", data, entry)
        if kind == 4:
            info = entry + 4 + offset
            compression, offset, count = struct.unpack_from("<III", data, info)
            sizes = struct.unpack_from(f"<{count}I", data, info + 4 + offset)
    block_bytes = {1: 8, 2: 16, 20: 16}
    for level in range(levels - 1, -1, -1):
        w, h = max(1, width >> level), max(1, height >> level)
        size = ((w + 3) // 4) * ((h + 3) // 4) * block_bytes[fmt] if fmt in block_bytes else w * h * 4
        stored_size = sizes[level] if sizes else size
        if level:
            payload += stored_size
            continue
        pixels = data[payload:payload + stored_size]
        if stored_size < size:
            if compression != 1:
                raise ValueError(f"Unsupported mip compression {compression}")
            pixels = lz4_block(pixels, size)
        if fmt in block_bytes:
            mode = {1: 1, 2: 3, 20: 7}[fmt]
            image = Image.frombytes("RGBA", (w, h), pixels, "bcn", mode)
        else:
            image = Image.frombytes("RGBA", (w, h), pixels, "raw", "BGRA" if fmt == 28 else "RGBA")
        image.thumbnail((384, 288), Image.Resampling.LANCZOS)
        return image
    raise ValueError("Missing mip levels")


def sections(text, name):
    return re.findall(r'^\t"' + name + r'"[^\S\n]*\n\t\{[^\S\n]*\n(.*?)^\t\}', text, re.M | re.S)


def entries(text):
    pattern = r'^\t\t"(\d+)"\s*\n\t\t\{\s*\n(.*?)^\t\t\}'
    return {
        int(key): dict(re.findall(r'^\t{3}"([^"]+)"\s+"([^"]*)"', entry, re.M))
        for key, entry in re.findall(pattern, text, re.M | re.S)
    }


def quoted(text):
    return '"' + text.replace("\\", "\\\\").replace('"', '\\"') + '"'


def build(vpk, destination):
    content = vpk.read("scripts/items/items_game.txt").decode("utf-8-sig").replace("\r\n", "\n")
    weapons = entries(sections(content, "items")[0])
    finishes = {}
    for section in sections(content, "paint_kits"):
        for kit, fields in entries(section).items():
            finishes.setdefault(kit, {}).update(fields)
    language = vpk.read("resource/csgo_english.txt")
    encoding = "utf-16" if language[:2] in (b"\xff\xfe", b"\xfe\xff") else "utf-8-sig"
    labels = {key.lower(): val for key, val in re.findall(r'"([^"\r\n]+)"[ \t]+"([^"\r\n]*)"', language.decode(encoding, errors="replace"))}

    catalog = []
    preview_dir = destination / "assets" / "preview"
    preview_dir.mkdir(parents=True, exist_ok=True)

    def add(definition, kit, weapon, group):
        finish = finishes.get(kit)
        if not finish or "name" not in finish:
            return False
        name = weapons[definition]["name"]
        path = f"{ITEM_ICONS}{name}_{finish['name']}_light_png.vtex_c"
        if path not in vpk.entries:
            return False
        tag = finish["description_tag"].lstrip("#").lower()
        label = labels[tag]
        if group == "knife":
            label = KNIFE_FINISHES.get(kit, label)
        if definition == 4:
            label = GLOCK_GAMMA.get(kit, label)
        if definition == 1 and kit == 469:
            label = "Sunset Storm (I)"
        minimum = finish.get("wear_remap_min", "0.06")
        maximum = finish.get("wear_remap_max", "0.8")
        def fmt(value):
            return f"{float(value):.6f}f"
        catalog.append(f'    {{ {quoted(weapon)}, {quoted(label)}, category::{group}, '
                       f'{{ {definition}, {kit}, 1, 1000, {fmt(minimum)}, {fmt(maximum)} }} }},')
        decode_icon(vpk.read(path)).save(preview_dir / f"{definition}-{kit}.png")
        return True

    for definition, (weapon, group, kits) in FIREARMS.items():
        for kit in kits:
            if not add(definition, kit, weapon, group):
                print(f"Skipping unsupported finish {definition}/{kit}")
    extra_knife_kits = [
        kit for kit, fields in finishes.items()
        if labels.get(fields.get("description_tag", "").lstrip("#").lower()) in EXTRA_KNIFE_FINISHES
    ]
    knife_kits = dict.fromkeys((*KNIFE_FINISHES, *extra_knife_kits))
    installed_knives = {definition for definition, item in weapons.items() if item.get("prefab") == "melee_unusual"}
    if installed_knives != KNIVES.keys():
        raise ValueError(f"Review changed knife definitions: {installed_knives ^ KNIVES.keys()}")
    for definition, weapon in KNIVES.items():
        count = sum(add(definition, kit, weapon, "knife") for kit in knife_kits)
        if not count:
            raise ValueError(f"No compatible finish for {weapon}")
        print(f"{weapon}: {count}")
    for definition, (weapon, kits) in GLOVES.items():
        for kit in kits:
            if not add(definition, kit, weapon, "glove"):
                print(f"Skipping unsupported finish {definition}/{kit}")
    output = destination / "menu" / "acervo_finishes.inc"
    output.parent.mkdir()
    output.write_text("// Generated from the installed CS2 inventory by prepare_collection.py.\n"
                      + "\n".join(catalog) + "\n", encoding="utf-8")
    return output


def publish_snapshot(source, destination, build_id):
    if destination.exists():
        raise FileExistsError(f"Use a new snapshot directory: {destination}")
    destination.parent.mkdir(parents=True, exist_ok=True)
    staging = destination.parent / (".collection-" + uuid4().hex)
    staging.mkdir()
    try:
        index = build(Archive(source), staging)
        inventory = read_inventory(index, staging / "assets" / "preview")
        for icon in (staging / "assets" / "preview").glob("*.png"):
            with Image.open(icon) as image:
                image.verify()
        (staging / "web-catalog.js").write_text(
            make_web_catalog(inventory, ROOT / "tools" / "web-collection.json", ROOT.parent / "ui" / "assets"),
            encoding="utf-8",
        )
        metadata = {
            "game_build": build_id, "entries": len(inventory), "index_sha256": digest(index),
            "source": source.name, "vpk_index_sha256": digest(source),
        }
        (staging / "assets" / "catalog-source.json").write_text(
            json.dumps(metadata, indent=2) + "\n", encoding="utf-8",
        )
        staging.rename(destination)
    finally:
        if staging.is_dir() and staging.resolve().parent == destination.parent and not staging.is_symlink():
            shutil.rmtree(staging)
    print(f"Published {len(inventory)} verified previews to {destination}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("vpk", type=Path, help="Path to game/csgo/pak01_dir.vpk")
    parser.add_argument("--output", type=Path, required=True, help="New directory for the complete snapshot")
    parser.add_argument("--build", required=True, help="Steam build ID matching the input archive")
    args = parser.parse_args()
    publish_snapshot(args.vpk.resolve(), args.output.resolve(), args.build)
