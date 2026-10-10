#!/usr/bin/env python3
"""Compile recovered UG2 list ranking and compare relocated original bytes."""
import hashlib
import json
import random
import struct
import subprocess
import pefile
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ECX, UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBX, UC_X86_REG_EBP
from probe_pc_race_reward import ROOT, EXE

BUILD = ROOT / "build/analysis/racing"
LIST_SYMBOL = "?driftRankList@@3UDriftRankList@@A"
END_SYMBOL = "?End@DriftRankList@@QAEPAUDriftRankNode@@XZ"
RANK_SYMBOL = "?FindDriftListPosition@@YAHH@Z"
REWARD_SYMBOL = "?GetBankWonFromRace@CareerReward@@QAEHXZ"
ELIGIBILITY_SYMBOL = "?CheckRewardEligibility@CareerRaceResult@@QAE_N_N@Z"
ADDRESSES = {LIST_SYMBOL: 0x864630, END_SYMBOL: 0x502fd0, RANK_SYMBOL: 0x56dd90,
             REWARD_SYMBOL: 0x532e50, "?activeGameRace@@3PAUCareerRaceReward@@A": 0x863470,
             ELIGIBILITY_SYMBOL: 0x5287a0, "?GetOverallResult@CareerRaceResult@@QAEHXZ": 0x528710}
FINGERPRINTS = {
    END_SYMBOL: (3, "58367ffa2a0179375018fa0f5c26da24391e42ebe0ed8fdda35a21fc7bdc396f"),
    RANK_SYMBOL: (54, "a959195c6e92fd576592b0702f9d7369971943adc3e35f1d5307363c3140f75a"),
    REWARD_SYMBOL: (79, "56587b0b1dd3d92ddd82b9c03cc38df0d052a12b8be1c75cb24a72bfc065525e"),
    ELIGIBILITY_SYMBOL: (480, "1decbbe918562e1f97a99c0fe5eca9d3d295804813051d4d201ac64d09ab4423"),
}


def coff_function(path, symbol):
    data = path.read_bytes()
    machine, count, _, symoff, nsyms, optional, _ = struct.unpack_from("<HHIIIHH", data)
    if machine != 0x14c:
        raise ValueError("Expected x86 COFF")
    sections = [struct.unpack_from("<8sIIIIIIHHI", data, 20+optional+i*40) for i in range(count)]
    strings = symoff+nsyms*18
    names, functions = {}, {}
    i = 0
    while i < nsyms:
        offset = symoff+i*18
        raw, value, section, kind, storage, aux = struct.unpack_from("<8sIhHBB", data, offset)
        if raw[:4] == b"\0"*4:
            start = strings+struct.unpack_from("<I", raw, 4)[0]
            name = data[start:data.index(b"\0", start)].decode("ascii")
        else:
            name = raw.rstrip(b"\0").decode("ascii")
        names[i] = name
        if kind == 0x20 and storage == 2 and section > 0 and aux:
            functions[name] = (value, section, struct.unpack_from("<I", data, offset+22)[0])
        i += 1+aux
    value, section, size = functions[symbol]
    sec = sections[section-1]
    if value+size > sec[3]:
        raise ValueError("COFF function exceeds section")
    body = bytearray(data[sec[4]+value:sec[4]+value+size])
    relocations = []
    for i in range(sec[7]):
        rva, index, kind = struct.unpack_from("<IIH", data, sec[5]+i*10)
        if not value <= rva < value+size:
            continue
        target, offset = names[index], rva-value
        if target not in ADDRESSES or offset+4 > size:
            raise ValueError("Unknown or unbounded relocation")
        addend = struct.unpack_from("<I", body, offset)[0]
        resolved = ADDRESSES[target]+addend
        if kind == 0x14:  # IMAGE_REL_I386_REL32
            resolved -= ADDRESSES[symbol]+offset+4
        elif kind != 6:  # IMAGE_REL_I386_DIR32
            raise ValueError("Unsupported relocation")
        struct.pack_into("<I", body, offset, resolved & 0xffffffff)
        relocations.append(dict(offset=offset, target=target, kind=kind))
    return bytes(body), relocations


def main():
    BUILD.mkdir(parents=True, exist_ok=True)
    original_file = EXE.read_bytes()
    pe = pefile.PE(data=original_file)
    base = pe.OPTIONAL_HEADER.ImageBase
    original_image = pe.get_memory_mapped_image()
    rebuilt_image = bytearray(original_image)
    records = []
    replacements = {}
    for unit, symbol in (("DriftRankList", END_SYMBOL), ("DriftRanking", RANK_SYMBOL),
                         ("CareerReward", REWARD_SYMBOL), ("CareerEligibility", ELIGIBILITY_SYMBOL)):
        source = ROOT / "src/Speed/Indep/Src/Gameplay" / (unit+".cpp")
        obj = BUILD / (unit+".obj")
        subprocess.run([str(ROOT/"build/tools/wibo"), "build/compilers/Win32/7.1/cl.exe",
                        "/nologo", "/c", "/Ox", "/Z7", "/EHsc", "/Fo"+str(obj.relative_to(ROOT)),
                        str(source.relative_to(ROOT))], cwd=ROOT, check=True)
        body, relocations = coff_function(obj, symbol)
        size, digest = FINGERPRINTS[symbol]
        address = ADDRESSES[symbol]
        original = original_image[address-base:address-base+size]
        if base != 0x400000 or hashlib.sha256(original).hexdigest() != digest:
            raise ValueError("Unsupported executable fingerprint")
        if body != original and symbol != ELIGIBILITY_SYMBOL:
            raise ValueError(f"Compiled bytes differ: {symbol}")
        if len(body) > size:
            raise ValueError("Candidate exceeds original function region")
        rebuilt_image[address-base:address-base+len(body)] = body
        replacements[address] = body
        records.append(dict(symbol=symbol, address=hex(address), original_size=size,
                            rebuilt_size=len(body), sha256=digest,
                            byte_identical_after_relocation=body == original, relocations=relocations))
    machines = []
    for image in (original_image, rebuilt_image):
        uc = Uc(UC_ARCH_X86, UC_MODE_32)
        uc.mem_map(base, (len(image)+4095) & ~4095)
        uc.mem_write(base, bytes(image))
        uc.mem_map(0x2000000, 0x20000)
        machines.append(uc)
    sentinel, nodes, stack, stop = ADDRESSES[LIST_SYMBOL], 0x2000000, 0x201f000, 0x201ff00
    rng, count = random.Random(0x126890), 0
    for length in range(33):
        for trial in range(8):
            ids = [rng.choice((0, 1, 2, 0xffffffff, rng.randrange(0x100000000))) for _ in range(length)]
            queries = [0, 1, 2, 0xffffffff, 0x80000000, 0xabcdef01]+ids
            for uc in machines:
                uc.mem_write(sentinel, struct.pack("<II", nodes if length else sentinel,
                                                 nodes+(length-1)*16 if length else sentinel))
                for i, identifier in enumerate(ids):
                    uc.mem_write(nodes+i*16, struct.pack("<III", nodes+(i+1)*16 if i+1 < length else sentinel,
                                                        nodes+(i-1)*16 if i else sentinel, identifier))
            for query in queries:
                expected = ids.index(query)+1 if query in ids else 0
                for uc in machines:
                    uc.mem_write(stack, struct.pack("<II", stop, query))
                    uc.reg_write(UC_X86_REG_ESP, stack)
                    saved = (UC_X86_REG_EBX, UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP)
                    for reg in saved:
                        uc.reg_write(reg, 0x12345678)
                    uc.emu_start(ADDRESSES[RANK_SYMBOL], stop, count=2000)
                    assert uc.reg_read(UC_X86_REG_EIP) == stop
                    assert uc.reg_read(UC_X86_REG_EAX) == expected
                    assert uc.reg_read(UC_X86_REG_ESP) == stack+4
                    assert all(uc.reg_read(reg) == 0x12345678 for reg in saved)
                count += 1
    from probe_pc_race_eligibility import main as verify_eligibility
    verify_eligibility(replacements)
    report = dict(executable_sha256=hashlib.sha256(original_file).hexdigest(),
                  compiler="MSVC 7.1 /Ox /Z7 /EHsc", functions=records,
                  differential_behavior_cases=count, stubbed_callees=[],
                  scope="Matching list/reward; eligibility source is nonmatching and behavior-checked only in bounded cases",
                  rebuilt_reward_integrated_cases=976)
    (BUILD/"drift-ranking-verification.json").write_text(json.dumps(report, indent=2)+"\n")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
