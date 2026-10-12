# bWare slot pool

Reference: UG2 PC v1.2 `speed2.exe`, SHA-256
`f9dd86c054878ce6276beb07c1fd61874f7a1e4bf1f241b084c65b73e24168a7`.

`src/Speed/Indep/Libs/bWare/bSlotPool.cpp` recovers the bWare slot pool and
replaces the native `SlotPool` scaffold that the demo previously used.

| Routine | Address | Role |
| --- | --- | --- |
| `bSlotPool::InitialiseFreeList` | `0x43cd40` | rebuild each block's free list in slot order |
| `bNewSlotPoolBlock` | `0x440b40` | heap block of `count` slots after a 0x30-byte header; flags 0xb |
| `bSlotPool::Expand` | `0x440bb0` | chain a block and prepend its slots to the free list |
| `bSlotPool::FindUsedSlot` | `0x440c30` | n-th slot not on the free list (leak report) |
| `bSlotPool::Allocate` | `0x440d40` | take a slot; expand by count/4 + 1 when empty (flag 1), clear (flag 2) |
| `bSlotPoolRegistry::Delete` | `0x440da0` | unregister, leak check (flag 8), free blocks |
| `bNewSlotPool` | `0x440df0` | round size to 4, create, register at 0x828534 when 0x828530 is 1 |
| `bOMalloc` | `0x440e50` | allocation wrapper |
| `bSlotPoolAllocate` | `0x43f8f0` | allocation without expansion or clearing |
| `bFree` | `0x43f920` | push a slot on the free list |
| `bIsSlotPoolFull` | `0x43cd10` | used count equals total capacity |
| `bNewNodeSlotPool` | `0x440e60` | 0xc-byte list-node pool "bPNode SlotPool" |
| `bNewListNode` / `bDeleteListNode` | `0x440ec0` / `0x440ef0` | list nodes; pending pool delete once empty |

Header layout: +0 next / +4 previous (registry links), +8 next block, +0xc
name, +0x10 free list, +0x14 flags (1 expand, 2 clear, 8 leak check), +0x18
used, +0x1c maximum used, +0x20 heap flags, +0x24 block slot count, +0x28 slot
size, +0x2c total capacity; slots start at +0x30. New pools always get flags
0xb, so original pools grow on demand: the former native scaffold enforced a
fixed capacity and the track loader had to lift it. The bWare heap
(`0x440390` bMalloc, `0x440540` free) remains a native platform seam backed by
`calloc`/`free`.

`tools/verify_pc_bslotpool.py` runs 300 random sequences of 60 operations
(creation of registered and unregistered pools, all three allocation paths,
frees, full tests, expansion, leak queries, list nodes with pending deletes,
and deletion) on both sides with the heap hooked deterministically (0xa5-filled
blocks, traced frees). On 2026-10-12 **17,000** operations had bit-identical
heap, registry and node-pool state, return values and allocation traces.
Mutations of the expansion size and the leak-query skip were detected. Block
slot sizes are multiples of four and pointers well formed; byte matching is not
established. Native driving smoke results are unchanged (123.6/76.8 km/h).
