# NFSU2 RE recovery rules

The current objective is to completely decompile UG2 PC v1.2 into verified,
reproducibly buildable source. A playable demo is an intermediate milestone,
not completion of the project. See
docs/DECOMPILATION_SCOPE.md for the completion evidence.

Recover original UG2 code and mechanics.
Preserve evidence, call boundaries and arithmetic order. Keep unknown field names
offset-based until identified. Mark behavioral verification separately from byte
matching. Do not invent algorithms or force registers to fake a match.

Keep retail inputs and generated disassembly local under ignored directories.
Compile MSVC/wibo verification jobs sequentially.
