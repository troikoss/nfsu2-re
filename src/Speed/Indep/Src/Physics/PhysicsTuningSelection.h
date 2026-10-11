#ifndef UG2_PHYSICS_TUNING_SELECTION_H
#define UG2_PHYSICS_TUNING_SELECTION_H
#include "PhysicsUpgrades.h"

struct PhysicsTuningEntry {
    int value0;
    signed char priority4;
    signed char index5;
    unsigned char kind6;
    char unidentified7;
    float value8;
    float valueC;
    float value10; // -99.99 when unset
    float value14; // -99.99 when unset
};
UG2_CHECK_SIZE(PhysicsTuningEntry, 0x18);
struct PhysicsTuningValues {
    unsigned char values0[41];
    double GetOffset(int index, int component); // 0x59aee0
    void SetOffset(float value, int index, int component); // 0x59af80
};
UG2_CHECK_SIZE(PhysicsTuningValues, 41);
PhysicsTuningEntry *FindPhysicsTuningEntry(int index, PhysicsUpgradeSelection *selection); // 0x59ae40
#endif
