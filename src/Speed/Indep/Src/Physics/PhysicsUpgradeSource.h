#ifndef UG2_PHYSICS_UPGRADE_SOURCE_H
#define UG2_PHYSICS_UPGRADE_SOURCE_H

#include "Ug2Layout.h"
struct CarPhysicsRecord;
struct PhysicsUpgradeSelection;

// CarTypeInfo +0x110, remaining bytes of its 0x890-byte row. Keep original
// offsets explicit until the stock/upgrade tuning fields are identified.
struct PhysicsUpgradeSource {
    char values0[0x780];
    void BuildRecord(CarPhysicsRecord *, PhysicsUpgradeSelection *, char); // 0x5ac4f0
    void ApplyUpgrade0(CarPhysicsRecord *, PhysicsUpgradeSelection *); // 0x5a6b50
    void ApplyUpgrade1(CarPhysicsRecord *, PhysicsUpgradeSelection *); // 0x5a6bd0
    void ApplyUpgrade3(CarPhysicsRecord *, PhysicsUpgradeSelection *); // 0x5a6d00
    void ApplyUpgrade4(CarPhysicsRecord *, PhysicsUpgradeSelection *); // 0x5a6e30
    void ApplyUpgrade5(CarPhysicsRecord *, PhysicsUpgradeSelection *); // 0x5a6fa0
    void ApplyUpgrade6(CarPhysicsRecord *, PhysicsUpgradeSelection *); // 0x5a7140
    void ApplyUpgrade7(CarPhysicsRecord *, PhysicsUpgradeSelection *); // 0x5a71c0
    void ApplyUpgrade8(CarPhysicsRecord *, PhysicsUpgradeSelection *); // 0x5a7360
    void ApplyUpgrade9(CarPhysicsRecord *, PhysicsUpgradeSelection *); // 0x5a74a0
};
UG2_CHECK_SIZE(PhysicsUpgradeSource, 0x780);
#endif
