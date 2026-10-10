# NFSU2 RE recovery rules

The objective is complete Underground 2 decompilation. Recover the whole original
game, including systems outside driving physics. A partial driving demo does not
establish completion. See docs/DECOMPILATION_SCOPE.md for the completion evidence.

Recover original UG2 code and mechanics.
Preserve evidence, call boundaries and arithmetic order. Keep unknown field names
offset-based until identified. Mark behavioral verification separately from byte
matching. Do not invent algorithms or force registers to fake a match.

Keep retail inputs and generated disassembly local under ignored directories.
Compile MSVC/wibo verification jobs sequentially.
