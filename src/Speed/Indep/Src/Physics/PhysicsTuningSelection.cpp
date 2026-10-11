#include "PhysicsTuningSelection.h"
#include "VehicleMover.h"

extern PhysicsTuningEntry *physicsTuningEntries;
extern int physicsTuningEntryCount;

// 0x59ae40: both priority and kind are signed bytes. Equal priorities retain
// the first eligible table entry. Selection value0 is an unchecked byte index.
PhysicsTuningEntry *FindPhysicsTuningEntry(int index, PhysicsUpgradeSelection *selection) {
    PhysicsTuningEntry *best = 0;
    if (physicsTuningEntries && physicsTuningEntryCount > 0) {
        for (int i = 0; i < physicsTuningEntryCount; ++i) {
            PhysicsTuningEntry *entry = physicsTuningEntries + i;
            if (index == entry->index5 && selection->selected50[entry->value0]) {
                if (!best || entry->priority4 > best->priority4) best = entry;
            }
        }
    }
    return best;
}

// 0x59b0e0. Gear kinds 10..15 also require index-8 < gear count+2.
int PhysicsTuning::CountActive() {
    value10 = 0;
    for (int i = 0; i < 25; ++i) {
        bool active = FindPhysicsTuningEntry(i, selection8) != 0;
        if (i >= 10 && i <= 15) {
            if (active && i - 8 < recordC->drivetrain1B0.unidentified18 + 2) ++value10;
        } else if (active) ++value10;
    }
    return value10;
}
