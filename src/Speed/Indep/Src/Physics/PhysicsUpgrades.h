#ifndef UG2_PHYSICS_UPGRADES_H
#define UG2_PHYSICS_UPGRADES_H

#include "Ug2Layout.h"

// GLOBAL chunk 0x34a15 descriptors. Semantic names beyond the observed
// selection dependencies are not established; retain the original offsets.
struct PhysicsUpgradeItem {
    int value0;
    char unidentified4[0x2c];
    float weights30[4];
    int exclusions40[5];
    int requirements54[2];
};
struct PhysicsUpgradeGroup {
    int value0;
    int value4;
    unsigned int count8;
    PhysicsUpgradeItem itemsC[4];
};
struct PhysicsUpgradeSelection {
    float values0[10];
    float values28[10];
    unsigned char selected50[68];
    int SetItem(PhysicsUpgradeItem *item, char selected, char recursive); // 0x5a6320
    int SetIndex(int index, char selected, char recursive); // 0x599680
    int SetGroup(PhysicsUpgradeGroup *group, char selected); // 0x5a6400
    int ClearGroups(int value); // 0x5a6450
    int SetGrade(int grade, int value); // 0x5a64a0
    double GetFraction(int value, int count, float *output); // 0x5a6580
};
UG2_CHECK_OFFSET(PhysicsUpgradeItem, weights30, 0x30);
UG2_CHECK_SIZE(PhysicsUpgradeItem, 0x5c);
UG2_CHECK_SIZE(PhysicsUpgradeGroup, 0x17c);
UG2_CHECK_SIZE(PhysicsUpgradeSelection, 0x94);
UG2_CHECK_OFFSET(PhysicsUpgradeSelection, selected50, 0x50);
int GetPhysicsUpgradeGroupCount(); // 0x500070
PhysicsUpgradeGroup *GetPhysicsUpgradeGroup(int index); // 0x500080
PhysicsUpgradeItem *GetPhysicsUpgradeItem(int index); // 0x500150
PhysicsUpgradeGroup *FindPhysicsUpgradeGroup(int value, int level); // 0x500100
void AccumulatePhysicsUpgradeWeights(float *output, PhysicsUpgradeSelection *selection,
                                     PhysicsUpgradeGroup *group, int count); // 0x5a61a0

#endif
