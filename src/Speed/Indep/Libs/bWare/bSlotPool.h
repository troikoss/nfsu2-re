#ifndef UG2_BSLOTPOOL_H
#define UG2_BSLOTPOOL_H

// bWare slot pool. Blocks of equal-sized slots with an intrusive free list;
// further blocks are chained at +8 when the pool expands. Pools may be
// registered in the list at 0x828534 when 0x828530 is set.
struct bSlotPool {
    bSlotPool *next0;
    bSlotPool *previous4;
    bSlotPool *nextBlock8;
    const char *nameC;
    void *free10;
    int flags14;       // 1 expand, 2 clear on allocation, 8 leak check on delete
    int used18;
    int maximumUsed1C;
    int heapFlags20;
    int count24;
    int slotSize28;
    int total2C;
    // Slots follow at +0x30.

    void InitialiseFreeList();       // 0x43cd40
    void Expand(int count);          // 0x440bb0
    void *FindUsedSlot(int skip);    // 0x440c30
    void *Allocate();                // 0x440d40
};

struct bSlotPoolRegistry {
    int enabled0;
    bSlotPool *first4;
    bSlotPool *last8;
    void Delete(bSlotPool *pool);    // 0x440da0
};

// bWare heap (0x440390/0x440540) is a platform allocator seam.
void *bMalloc(int size, const char *file, int line, int flags);
void bFreeMemory(void *memory);

bSlotPool *bNewSlotPoolBlock(int slotSize, int count, const char *name, int heapFlags); // 0x440b40
void *bNewSlotPool(int size, int count, const char *name, int flags);   // 0x440df0
void *bOMalloc(void *pool);                                               // 0x440e50
void *bSlotPoolAllocate(void *pool);                                      // 0x43f8f0
void bFree(void *pool, void *memory);                                     // 0x43f920
int bIsSlotPoolFull(void *pool);                                          // 0x43cd10

#endif
