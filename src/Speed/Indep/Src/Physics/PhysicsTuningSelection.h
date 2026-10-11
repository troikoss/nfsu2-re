#ifndef UG2_PHYSICS_TUNING_SELECTION_H
#define UG2_PHYSICS_TUNING_SELECTION_H
#include "PhysicsUpgrades.h"

struct PhysicsTuningEntry {
    int value0;
    signed char priority4;
    signed char index5;
    char unidentified6[0x12];
};
UG2_CHECK_SIZE(PhysicsTuningEntry, 0x18);
PhysicsTuningEntry *FindPhysicsTuningEntry(int index, PhysicsUpgradeSelection *selection); // 0x59ae40
#endif
