#ifndef UG2_DRIFT_RANKING_H
#define UG2_DRIFT_RANKING_H

// Recovered PC v1.2 layouts used by 0x56dd90. Further fields are unidentified.
struct DriftRankNode {
    DriftRankNode *next;
    DriftRankNode *previous;
    int identifier;
};

struct DriftRankList {
    DriftRankNode *first;
    DriftRankNode *last;
    __declspec(noinline) DriftRankNode *End();
};

extern DriftRankList driftRankList;
int FindDriftListPosition(int identifier);

#endif
