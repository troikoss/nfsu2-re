# First PC v1.2 decompilation slice

This local continuation starts from `cobanov/nfsu2-decomp` commit
`8415642c08e580cda24b19102b95e33ce2448fcf`. It reconstructs nine routines in
`src/Speed/Indep/Libs/bWare/Strings.cpp`. The path is provisional; without GameCube symbols,
the original translation unit and original function names remain unconfirmed.

Names and addresses come from [yugecin's function reference](https://yugecin.github.io/nfsu2-re/funcs.html).
Implementations were independently reconstructed from the local executable, with AI assistance;
no source or documentation was copied from that unlicensed project.

| Provisional name | PC virtual address | Function bytes | objdiff | Behavioral cases |
|---|---|---:|---:|---:|
| `cshash` | `0x43DB50` | 33 | 100% | 509 |
| `cshashWithStartValue` | `0x43DB80` | 33 | 100% | 2,545 |
| `strlenWithNullcheck` | `0x43DBB0` | 26 | 100% | 510 |
| `cihash` | `0x505450` | 47 | 100% | 510 |
| `nfs_strcpy` | `0x43DBD0` | 34 | 100% | 509 |
| `nfs_strncpy` | `0x43DC00` | 40 | 100% | 3,055 |
| `nfs_strcmp` | `0x43DC30` | 44 | 100% | 66,045 |
| `nfs_stricmp` | `0x43DCC0` | 67 | 100% | 66,045 |
| `nfs_strncmp` | `0x43DC60` | 89 | 100% | 330,226 |

The function bodies total 413 bytes, with 469,954 behavioral comparisons. The initial milestone
was four functions and 139 bytes; this continuation adds five functions and 274 bytes.
Exact comparison excludes the padding between functions:
the executable uses NOPs there and the compiler object uses INT3 padding. All instructions inside
each function, including internal loop alignment, match byte for byte. No assembly, forced
registers or binary patches are used in the C++ implementation. The `nfs_` prefix distinguishes
these game implementations from host C runtime functions; the manifest records the community
reference names separately. Copy routines expose the destination pointer left in EAX; their
original source-level return types remain provisional.

The inspected executable is a 32-bit PE at base `0x400000`, with entry point `0x75BCC7` and an
embedded build string naming Perforce change 126890, dated 2005-02-09. Its full-file SHA-256 is
`f9dd86c054878ce6276beb07c1fd61874f7a1e4bf1f241b084c65b73e24168a7`.
Its timestamp and additional sections differ from the documented retail layout. Verification
therefore fingerprints each selected function; this does not establish that the entire file is
an unmodified retail executable.

## Reproduce on Linux

Run from this repository's root. Requires Python 3, `g++`, and an already readable PC v1.2
executable. In this workspace, `orig/SPEED2_EXE_1_2/speed2.exe` is an ignored symlink to the
user-provided executable. Alternatively supply `--exe /path/to/speed2.exe`.

```sh
python3 -m venv build/venv
build/venv/bin/pip install -r requirements-pc-strings.txt
mkdir -p build/tools
python3 tools/download_tool.py compilers build/compilers --tag 20260903
python3 tools/download_tool.py wibo build/tools/wibo --tag 1.2.0
python3 tools/download_tool.py delink build/tools/delink --tag v0.16.4
python3 tools/download_tool.py objdiff-cli build/tools/objdiff-cli --tag v3.7.0
build/venv/bin/python tools/verify_pc_strings.py
```

The verifier checks the PE layout and function fingerprints, creates a selected-function export
in delink's IDA interchange format, splits the original routines into an object, and compiles
the C++ using MSVC 7.1 with `/Ox /Z7 /EHsc`. It then compares the complete function bodies and
requires every function to score 100% in objdiff. Finally it compiles a native host library and
compares it against the original x86 routines executed in Unicorn.

Behavior checks include every nonzero byte value, embedded terminators, a 4,096-byte string,
500 deterministic random strings, five initial hash values, and null arguments where supported.
The hashes consume unsigned bytes, start at `0xFFFFFFFF` by default and wrap at 32 bits.
Empty hashes retain their initial value. Case folding affects only ASCII `a` through `z`.
`cihash(NULL)` returns `0xFFFFFFFF`; `strlenWithNullcheck(NULL)` returns zero.
The two case-sensitive hash routines fault on null, as verified in emulation.

Copy verification compares return pointers, complete destination buffers and unchanged source
buffers, with sentinels before and after each buffer. Limits include negative one, zero, one,
the source length, and values beyond the source length. A negative bounded-copy limit behaves
like an unbounded copy for these terminated test inputs. A zero limit accesses neither pointer.
The bounded copy stops at a copied NUL without padding and may leave an unterminated result
when the limit is reached. It must not be replaced with host `strncpy`.

Comparison verification exhausts all 65,536 one-byte input pairs, including NUL and high bytes,
then checks prefixes, case-only differences, embedded terminators, long strings and 500 random
common-prefix pairs. Comparisons subtract signed character values; case folding is ASCII only.
Every emulated successful call also checks stack balance and preservation of EBX, ESI, EDI and EBP.

The routine at `0x43DC60`, called `strncmp` by the community reference, has unusual semantics.
Its cursors advance before it handles a mismatch; subsequent nonzero bytes can cause it to return
`1` regardless of the mismatch's ordering. When both strings start empty and the limit is positive,
it subtracts the bytes immediately before their starts. The test harness deliberately allocates
readable guard bytes there, including different signed values to detect this access. This is
observed game behavior, not a standard C string contract. Zero-limit comparison accepts null
arguments and returns zero. These routines retain the original PC assumption that plain `char`
is signed; ports to targets with unsigned plain `char` will need to preserve that explicitly.

All extracted machine code, generated objects and reports stay in ignored `build/pc-strings/`.
The durable function manifest in `config/SPEED2_EXE_1_2/strings.json` contains only addresses,
sizes and SHA-256 fingerprints. Outputs include:

- `verification.json`: executable fingerprint, per-function scores and test counts.
- `diff.json`: detailed objdiff output.
- `objdiff.json`: a scoped project that can be opened in objdiff.

## Current limits and next work

This verifies nine selected routines against the local PC v1.2 executable. It does not rebuild,
launch or port the whole game, verify PC v1.0 or GameCube, or validate replacement hooks in a
running game. The main `configure.py` flow remains incomplete because its full-game
`symbols.json` and `splits.json` have not been created. The selected export intentionally contains
only the nine verified functions and cannot replace those full-game files. No objects have been
marked `Matching` in the full-game build configuration.

Next, reconstruct the bounded case-insensitive comparator at `0x43DD10` and two-source copy at
`0x43DDA0`, verify their memory behavior, and recover the enclosing translation unit. Build the full
PC symbol map separately, then verify the unmodified split/relink before enabling replacement
objects in the full-game configuration. A user-provided GameCube `Speed.elf` would resolve many
original names and types, but it is not required for further PC analysis.

The community IDC export has since been indexed by `tools/recover_pc_index.py`: 18,444
function-boundary seeds, 3,252 named entries, 3,469,275 bytes, and no rejected boundaries under
its executable-section and overlap checks. All nine verified functions agree with that index.
The output is `build/analysis/function-index.json`; it is a starting map, not a verified
full-game split or compilable source. A separate native driving demo using OpenUG2 is described
in the workspace's `DEMO.md`.
