#include "bSlotPool.h"
#include "../../Src/World/WCollision.h"

extern bSlotPoolRegistry slotPoolRegistry828530;
extern bSlotPool *nodeSlotPool8284F0;
extern int nodeSlotPoolPendingDelete8284F4;
extern char nodeSlotPoolName786F40[];

// 0x43cd40 rebuilds each block's free list in slot order.
void bSlotPool::InitialiseFreeList() {
    if (this == 0) return;
    bSlotPool *block = this;
    do {
        int count = block->count24;
        int size = block->slotSize28;
        block->free10 = 0;
        if (count) {
            char *slot = reinterpret_cast<char *>(block) + 0x30;
            block->free10 = slot;
            for (int i = count - 1; i > 0; --i) {
                *reinterpret_cast<char **>(slot) = slot + size;
                slot += size;
            }
            *reinterpret_cast<char **>(slot) = 0;
        }
        block = block->nextBlock8;
    } while (block);
}

// 0x440b40 allocates a block of `count` slots with a 0x30-byte header.
bSlotPool *bNewSlotPoolBlock(int slotSize, int count, const char *name, int heapFlags) {
    bSlotPool *pool = static_cast<bSlotPool *>(bMalloc(slotSize * count + 0x30, 0, 0, heapFlags));
    if (pool) {
        pool->nameC = name;
        pool->count24 = count;
        pool->slotSize28 = slotSize;
        pool->flags14 = 0xb;
        pool->free10 = 0;
        pool->nextBlock8 = 0;
        pool->heapFlags20 = heapFlags;
        pool->total2C = count;
        pool->used18 = 0;
        pool->maximumUsed1C = 0;
        pool->InitialiseFreeList();
    }
    return pool;
}

// 0x440bb0 chains a new block and prepends its slots to the free list.
void bSlotPool::Expand(int count) {
    bSlotPool *block = bNewSlotPoolBlock(slotSize28, count, nameC, heapFlags20);
    block->flags14 = flags14;
    bSlotPool *last = this;
    while (last->nextBlock8) last = last->nextBlock8;
    last->nextBlock8 = block;
    reinterpret_cast<void **>(reinterpret_cast<char *>(block) + 0x30)[((count - 1) * slotSize28) / 4] = free10;
    free10 = block->free10;
    block->free10 = 0;
    total2C += count;
}

// 0x440c30 returns the `skip`-th slot not on the free list (leak report), or 0.
void *bSlotPool::FindUsedSlot(int skip) {
    unsigned char *used = static_cast<unsigned char *>(bMalloc(total2C, 0, 0, 0));
    for (int i = 0; i < total2C; ++i) used[i] = 1;
    for (void **node = static_cast<void **>(free10); node; node = static_cast<void **>(*node)) {
        int base = 0;
        int index = -1;
        for (bSlotPool *block = this; block; block = block->nextBlock8) {
            int local = (int)((unsigned int)(reinterpret_cast<char *>(node) - reinterpret_cast<char *>(block) - 0x30)
                              / (unsigned int)slotSize28);
            if (local >= 0 && local < block->count24) {
                index = local + base;
                break;
            }
            base += block->count24;
        }
        used[index] = 0;
    }
    int slot = 0;
    for (; slot < total2C; ++slot) {
        if (used[slot]) {
            if (skip == 0) break;
            --skip;
        }
    }
    void *result = 0;
    if (slot < total2C) {
        for (bSlotPool *block = this; block; block = block->nextBlock8) {
            if (slot >= 0 && slot < block->count24) {
                result = reinterpret_cast<char *>(block) + slotSize28 * slot + 0x30;
                break;
            }
            slot -= block->count24;
        }
    }
    bFreeMemory(used);
    return result;
}

// 0x440d40 takes a slot, expanding by a quarter when empty and clearing it
// when flag 2 is set.
void *bSlotPool::Allocate() {
    if (free10 == 0 && (flags14 & 1)) Expand(count24 / 4 + 1);
    int used = used18 + 1;
    void **slot = static_cast<void **>(free10);
    if (slot) {
        if (used > maximumUsed1C) maximumUsed1C = used;
        used18 = used;
        free10 = *slot;
        if (flags14 & 2) {
            int words = slotSize28 >> 2;
            for (int i = 0; i < words; ++i) reinterpret_cast<int *>(slot)[i] = 0;
        }
    }
    return slot;
}

// 0x440da0 unregisters a pool, runs the leak check, and frees its blocks.
void bSlotPoolRegistry::Delete(bSlotPool *pool) {
    if (pool == 0) return;
    if (enabled0) {
        bSlotPool *next = pool->next0;
        bSlotPool *previous = pool->previous4;
        previous->next0 = next;
        next->previous4 = previous;
    }
    if ((pool->flags14 & 8) && pool->used18 > 0) pool->FindUsedSlot(0);
    do {
        bSlotPool *next = pool->nextBlock8;
        bFreeMemory(pool);
        pool = next;
    } while (pool);
}

// 0x440df0 creates and registers a pool with slots rounded to four bytes.
void *bNewSlotPool(int size, int count, const char *name, int flags) {
    bSlotPool *pool = bNewSlotPoolBlock((size + 3) & ~3, count, name, flags);
    if (pool == 0) return pool;
    if (slotPoolRegistry828530.enabled0 == 1) {
        bSlotPool *last = slotPoolRegistry828530.last8;
        last->next0 = pool;
        slotPoolRegistry828530.last8 = pool;
        pool->previous4 = last;
        pool->next0 = reinterpret_cast<bSlotPool *>(&slotPoolRegistry828530.first4);
    }
    return pool;
}

// 0x440e50.
void *bOMalloc(void *pool) {
    return static_cast<bSlotPool *>(pool)->Allocate();
}

// 0x43f8f0: allocation without expansion or clearing.
void *bSlotPoolAllocate(void *pool) {
    bSlotPool *slots = static_cast<bSlotPool *>(pool);
    int used = slots->used18 + 1;
    void **slot = static_cast<void **>(slots->free10);
    if (slot) {
        if (used > slots->maximumUsed1C) slots->maximumUsed1C = used;
        slots->free10 = *slot;
        slots->used18 = used;
    }
    return slot;
}

// 0x43f920.
void bFree(void *pool, void *memory) {
    bSlotPool *slots = static_cast<bSlotPool *>(pool);
    *static_cast<void **>(memory) = slots->free10;
    --slots->used18;
    slots->free10 = memory;
}

// 0x43cd10.
int bIsSlotPoolFull(void *pool) {
    bSlotPool *slots = static_cast<bSlotPool *>(pool);
    return slots->used18 == slots->total2C;
}

// 0x440e60 creates the 0xc-byte node pool on first use.
void bNewNodeSlotPool(int count) {
    if (nodeSlotPool8284F0) return;
    bSlotPool *pool = bNewSlotPoolBlock(0xc, count, nodeSlotPoolName786F40, 0);
    if (pool == 0) {
        nodeSlotPool8284F0 = pool;
        nodeSlotPoolPendingDelete8284F4 = 0;
        return;
    }
    if (slotPoolRegistry828530.enabled0 == 1) {
        bSlotPool *last = slotPoolRegistry828530.last8;
        last->next0 = pool;
        slotPoolRegistry828530.last8 = pool;
        pool->previous4 = last;
        pool->next0 = reinterpret_cast<bSlotPool *>(&slotPoolRegistry828530.first4);
    }
    nodeSlotPool8284F0 = pool;
    nodeSlotPoolPendingDelete8284F4 = 0;
}

// 0x440ec0.
WCollisionLink *bNewListNode() {
    if (nodeSlotPool8284F0 == 0) bNewNodeSlotPool(0x100);
    return static_cast<WCollisionLink *>(nodeSlotPool8284F0->Allocate());
}

// 0x440ef0 returns a node; a pending pool delete completes once it is empty.
void bDeleteListNode(WCollisionLink *node) {
    bSlotPool *pool = nodeSlotPool8284F0;
    int used = pool->used18;
    *reinterpret_cast<void **>(node) = pool->free10;
    pool->free10 = node;
    pool->used18 = used - 1;
    if (nodeSlotPoolPendingDelete8284F4 && nodeSlotPool8284F0) {
        if (nodeSlotPool8284F0->used18 == 0) {
            slotPoolRegistry828530.Delete(nodeSlotPool8284F0);
            nodeSlotPool8284F0 = 0;
        } else {
            nodeSlotPoolPendingDelete8284F4 = 1;
        }
    }
}
