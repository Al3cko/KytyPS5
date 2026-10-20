"""Read-only check of the render-size patch's signatures in a local uncompressed SELF/ELF.

Does not execute guest code, apply patches, or write to the game directory.
The instruction patterns and dimension tables are read from the production header.
"""
import argparse
import hashlib
import json
import re
import struct
from pathlib import Path


def check(eboot, header):
    data = eboot.read_bytes()
    text = header.read_text(encoding="utf-8-sig")
    patterns = re.findall(r'ModeCase(?:Duplicate|Duplicated)?\s*=\s*"([^"]+)"', text)
    assert patterns, "No production instruction patterns found"
    def table(name):
        match = re.search(name + r'\[5\]\s*\{([^}]+)\}', text)
        assert match, "Missing dimension table " + name
        return struct.pack("<5I", *(int(v.strip()) for v in match[1].split(",")))
    def dimension(name):
        return int(re.search(r'\b' + name + r'\s*=\s*(\d+)', text)[1])
    output_pair = struct.pack("<II", dimension("OutputWidth"), dimension("OutputHeight"))
    render_pair = struct.pack("<II", dimension("RenderWidth"), dimension("RenderHeight"))
    is_elf = data[:4] == b"\x7fELF"
    if is_elf:
        elf_offset, self_segments = 0, []
    else:
        assert data[:4] == bytes.fromhex("5414f5ee"), "Unknown file container"
        count = struct.unpack_from("<H", data, 24)[0]
        elf_offset = 32 + count * 32
        self_segments = [struct.unpack_from("<QQQQ", data, 32 + i * 32) for i in range(count)]
    eh = struct.unpack_from("<16sHHIQQQIHHHHHH", data, elf_offset)
    assert eh[0][:6] == b"\x7fELF\x02\x01", "Expected little-endian ELF64"
    phdrs = [struct.unpack_from("<IIQQQQQQ", data, elf_offset + eh[5] + i * eh[9]) for i in range(eh[10])]
    segments = []
    for index, ph in enumerate(phdrs):
        if ph[0] != 1 or ph[5] == 0:
            continue
        offset = ph[2]
        if not is_elf:
            matches = [s for s in self_segments if s[0] & 0x800 and (s[0] >> 20) & 0xfff == index]
            assert len(matches) == 1, "Ambiguous/missing SELF segment"
            segment = matches[0]
            assert segment[2] == segment[3] == ph[5], "Compressed SELF is not supported by this probe"
            offset = segment[1]
        assert offset + ph[5] <= len(data), "Segment extends beyond file"
        segments.append((ph[3], ph[1], data[offset:offset + ph[5]]))
    def read(address, size):
        for base, _, blob in segments:
            if base <= address and address + size <= base + len(blob):
                return blob[address - base:address - base + size]
        return None
    constants, sites = set(), []
    for pattern in patterns:
        regex = b"".join(b"." if value == "??" else re.escape(bytes.fromhex(value)) for value in pattern.split())
        for base, flags, blob in segments:
            if not flags & 1:
                continue
            for match in re.finditer(regex, blob, re.S):
                site = base + match.start()
                output = site + 8 + struct.unpack_from("<i", match.group(), 4)[0]
                render = site + 16 + struct.unpack_from("<i", match.group(), 12)[0]
                if read(output, 8) == output_pair and read(render, 8) == render_pair:
                    constants.add(render)
                    sites.append({"site": hex(site), "constant": hex(render), "pattern": pattern})
    trinity_constants, trinity_sites = set(), []
    trinity_patterns = re.findall(r'TrinityCase(?:Duplicated)?\s*=\s*"([^"]+)"', text)
    trinity_pair = struct.pack("<II", dimension("TrinityWidth"), dimension("TrinityHeight"))
    for pattern in trinity_patterns:
        regex = b"".join(b"." if value == "??" else re.escape(bytes.fromhex(value)) for value in pattern.split())
        for base, flags, blob in segments:
            if not flags & 1:
                continue
            for match in re.finditer(regex, blob, re.S):
                site = base + match.start()
                output = site + 8 + struct.unpack_from("<i", match.group(), 4)[0]
                render = site + 16 + struct.unpack_from("<i", match.group(), 12)[0]
                if read(output, 8) == output_pair and read(render, 8) == trinity_pair:
                    trinity_constants.add(render)
                    trinity_sites.append({"site": hex(site), "constant": hex(render), "pattern": pattern})
    widths = sum(blob.count(table("ModeWidths")) for _, _, blob in segments)
    heights = sum(blob.count(table("ModeHeights")) for _, _, blob in segments)
    return {"file": str(eboot), "sha256": hashlib.sha256(data).hexdigest(), "read_only": True,
            "header": str(header), "render_constant_candidates": len(constants),
            "width_table_candidates": widths, "height_table_candidates": heights,
            "trinity_sites": trinity_sites, "trinity_pattern_validation_passed": len(trinity_constants) == 1,
            "sites": sites, "pattern_validation_passed": len(constants) == widths == heights == 1,
            "note": "Static signature check only; no guest execution or GPU validation."}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("eboot", type=Path)
    parser.add_argument("--header", type=Path, default=Path(__file__).resolve().parents[2] / "src/loader/demonsSoulsRenderSize.h")
    args = parser.parse_args()
    result = check(args.eboot, args.header)
    print(json.dumps(result, indent=2))
    raise SystemExit(0 if result["pattern_validation_passed"] else 1)
