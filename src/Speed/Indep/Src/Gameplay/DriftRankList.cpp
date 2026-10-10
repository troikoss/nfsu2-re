#include "DriftRanking.h"

// Original PC v1.2: 0x502fd0. Intrusive lists use the list object as sentinel.
DriftRankNode *DriftRankList::End() {
    return reinterpret_cast<DriftRankNode *>(this);
}
