#include "PhysicsUpgrades.h"

extern float physicsUpgradeGradeScale;
extern float physicsUpgradeSearchEpsilon;
extern double physicsUpgradeSearchBias;
extern "C" double __cdecl fabs(double);
#pragma intrinsic(fabs)

// Original 0x591130. Count non-sentinel fields, then sum the first count
// fields; the original does not skip interspersed sentinel values in this sum.
double PhysicsUpgradeItem::AverageWeight() {
    unsigned int count = 0;
    for (int i = 0; i < 4; ++i) {
        if (*reinterpret_cast<unsigned int *>(&weights30[i]) != 0xbf800000u) ++count;
    }
    double total = 0.0;
    for (unsigned int i = 0; i < count; ++i) total = total + (double)weights30[i];
    return total / (double)count;
}

// Original 0x5a6210, cdecl. Descriptors must exist for every requested pair;
// the original dereferences a missing lookup. Only the current candidate's
// average is spilled as binary32 before comparing with the retained best mean.
PhysicsUpgradeItem *FindPhysicsUpgradeCandidate(PhysicsUpgradeSelection *selection, int level,
                                                int *values, int count, char smallest) {
    PhysicsUpgradeItem *best = 0;
    int first = level ? level : 1;
    int last = level ? level : 3;
    for (int grade = first; grade <= last; ++grade) {
        for (int i = 0; i < count; ++i) {
            PhysicsUpgradeGroup *group = FindPhysicsUpgradeGroup(values[i], grade);
            for (unsigned int j = 0; j < group->count8; ++j) {
                PhysicsUpgradeItem *item = &group->itemsC[j];
                if (selection->selected50[item->value0]) continue;
                if (!best) best = item;
                else {
                    float current = (float)item->AverageWeight();
                    double previous = best->AverageWeight();
                    if (smallest ? previous > (double)current : previous < (double)current) best = item;
                }
            }
        }
    }
    return best;
}

// Original 0x5a6880. Preserve three search orders (descending, ascending,
// alternating), snapshot restoration, partial grade application and the
// binary32 best-mean/error comparisons. The final query operates on the best
// temporary after it has been copied back; it does not rewrite this grade.
double PhysicsUpgradeSelection::SelectNearest(float fraction, int *values, int count) {
    if (!count) count = 10;
    float total = 0.0f;
    if (count > 0) {
        int maximum = (int)((double)fraction * physicsUpgradeGradeScale + physicsUpgradeSearchBias) + 1;
        if (maximum > 3) maximum = 3;
        for (int i = 0; i < count; ++i) {
            int value = values ? values[i] : i;
            ClearGroups(value);
            PhysicsUpgradeSelection working = *this;
            PhysicsUpgradeSelection best = *this;
            double initial = GetFraction(value, 0, 0);
            float bestMean = (float)initial;
            bool close = fabs(initial - (double)fraction) < (double)physicsUpgradeSearchEpsilon;
            for (int level = 1; level <= maximum && !close; ++level) {
                for (int order = 0; order < 3 && !close; ++order) {
                    bool smallest = order != 0;
                    PhysicsUpgradeItem *item = FindPhysicsUpgradeCandidate(&working, level, &value, 1, smallest);
                    while (item && !close) {
                        if (working.SetItem(item, 1, 0)) {
                            double mean = working.GetFraction(value, 0, 0);
                            double error = fabs(mean - (double)fraction);
                            float savedError = (float)error;
                            if (error < fabs((double)bestMean - (double)fraction)) {
                                bestMean = (float)mean;
                                best = working;
                                close = savedError < physicsUpgradeSearchEpsilon;
                            }
                        }
                        if (order == 2) smallest = !smallest;
                        item = FindPhysicsUpgradeCandidate(&working, level, &value, 1, smallest);
                    }
                    working = *this;
                }
                if (!close && level < maximum) {
                    SetGrade(level, value);
                    working = *this;
                }
            }
            *this = best;
            total = (float)(best.GetFraction(value, 0, 0) + (double)total);
        }
    }
    return (double)total / (double)count;
}
