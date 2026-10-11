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
struct PhysicsTuningValues {
    unsigned char values0[41];
    double GetOffset(int index, int component); // 0x59aee0
};
UG2_CHECK_SIZE(PhysicsTuningValues, 41);
PhysicsTuningEntry *FindPhysicsTuningEntry(int index, PhysicsUpgradeSelection *selection); // 0x59ae40
#endif
