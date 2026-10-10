#include "DriftRanking.h"

// Original PC v1.2: 0x56dd90. Names describe observed behavior, not symbols.
// The function does not calculate drift points or sort this list.
int FindDriftListPosition(int identifier) {
    DriftRankNode *node = driftRankList.first;
    int position = 1;
    DriftRankNode *end = driftRankList.End();
    while (node != end) {
        if (node->identifier == identifier) {
            return position;
        }
        node = node->next;
        ++position;
    }
    return 0;
}
