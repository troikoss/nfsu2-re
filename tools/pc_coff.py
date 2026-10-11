"""Analysis-only loader for linked x86 COFF sections, including local jump tables."""
import json
import os
import struct


def read_object(path):
    data = path.read_bytes()
    machine, count, _, symoff, nsyms, optional, _ = struct.unpack_from("<HHIIIHH", data)
    if machine != 0x14c:
        raise ValueError("Expected x86 COFF")
    sections = []
    for i in range(count):
        name, _, _, size, raw, relocs, _, nrelocs, _, flags = struct.unpack_from("<8sIIIIIIHHI", data, 20+optional+i*40)
        sections.append(dict(name=name.rstrip(b"\0").decode(), body=bytearray(size) if flags & 0x80 else bytearray(data[raw:raw+size]), flags=flags,
                             relocations=[struct.unpack_from("<IIH", data, relocs+j*10) for j in range(nrelocs)]))
    strings, symbols, index = symoff+nsyms*18, {}, 0
    while index < nsyms:
        offset = symoff+index*18
        raw, value, section, kind, storage, aux = struct.unpack_from("<8sIhHBB", data, offset)
        if raw[:4] == b"\0"*4:
            start = strings+struct.unpack_from("<I", raw, 4)[0]
            name = data[start:data.index(b"\0", start)].decode()
        else:
            name = raw.rstrip(b"\0").decode()
        size = struct.unpack_from("<I", data, offset+22)[0] if kind == 0x20 and aux else None
        symbols[index] = dict(name=name, value=value, section=section, kind=kind, storage=storage, size=size)
        index += aux+1
    return dict(sections=sections, symbols=symbols)


def link_objects(paths, externals, start=0x2200000):
    objects = [read_object(path) for path in paths]
    if os.environ.get("UG2_LINK_LOG"):
        with open(os.environ["UG2_LINK_LOG"], "a") as log:
            log.write(json.dumps({name: address for name, address in externals.items() if address < 0x1000000})+"\n")
    cursor, exports, functions = start, dict(externals), {}
    for obj in objects:
        for section in obj["sections"]:
            if section["flags"] & (0x20 | 0x40 | 0x80) and not section["name"].startswith(".debug"):
                section["address"] = cursor
                cursor += max(0x1000, (len(section["body"])+4095) & ~4095)
        for symbol in obj["symbols"].values():
            if symbol["section"] <= 0:
                continue
            section = obj["sections"][symbol["section"]-1]
            if "address" not in section:
                continue
            address = section["address"]+symbol["value"]
            if symbol["name"].startswith("__real@"):
                bits = symbol["name"].split("@", 1)[1]
                expected = int(bits, 16).to_bytes(len(bits)//2, "little")
                if section["body"][symbol["value"]:symbol["value"]+len(expected)] != expected:
                    raise ValueError("Floating literal does not match its symbol")
            if symbol["storage"] == 2:
                if symbol["name"] in exports and not symbol["name"].startswith("__real@"):
                    raise ValueError("Duplicate external definition: "+symbol["name"])
                exports[symbol["name"]] = address
            if symbol["kind"] == 0x20 and symbol["size"] is not None:
                functions[symbol["name"]] = dict(address=address, size=symbol["size"])
    blocks, executable_ranges = [], []
    for obj in objects:
        for section in obj["sections"]:
            if "address" not in section:
                continue
            body, address = section["body"], section["address"]
            for offset, index, kind in section["relocations"]:
                if offset+4 > len(body):
                    raise ValueError("Relocation exceeds section")
                symbol = obj["symbols"][index]
                if symbol["section"] > 0:
                    target = obj["sections"][symbol["section"]-1]["address"]+symbol["value"]
                elif symbol["section"] == -1:
                    target = symbol["value"]
                else:
                    target = exports[symbol["name"]]
                addend = struct.unpack_from("<I", body, offset)[0]
                resolved = target+addend
                if kind == 0x14:
                    resolved -= address+offset+4
                elif kind != 6:
                    raise ValueError(f"Unsupported relocation {kind}")
                struct.pack_into("<I", body, offset, resolved & 0xffffffff)
            blocks.append((address, bytes(body)))
            if section["flags"] & 0x20:
                executable_ranges.append((address, address+len(body)))
    return dict(blocks=blocks, functions=functions, ranges=executable_ranges,
                exports=exports, address=start, allocation=cursor-start)
