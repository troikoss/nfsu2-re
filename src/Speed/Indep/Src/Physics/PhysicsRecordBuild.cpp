#include "PhysicsUpgradeSource.h"
#include "VehicleMover.h"
#include "WheelMath.h"

extern float physicsUpgradeRounding;
extern float physicsUpgradeFractionScale;
extern float physicsInertiaScale;
extern unsigned char physicsUpgradeOverride7FC290;

// Original 0x5ac4f0. The source is CarTypeInfo +0x110. The flag chooses existing
// item flags versus reconstructing them from the ten requested grade values.
void PhysicsUpgradeSource::BuildRecord(CarPhysicsRecord *record, PhysicsUpgradeSelection *selection, char flag) {
    unsigned int *sourceWords = reinterpret_cast<unsigned int *>(this);
    unsigned int *recordWords = reinterpret_cast<unsigned int *>(record);
    for (int i = 0; i < 0xac; ++i) recordWords[i] = sourceWords[i];
    if (!flag) {
        for (int i = 0; i < 10; ++i) selection->SetGrade((int)((double)selection->values0[i] + physicsUpgradeRounding), i);
    }
    for (int i = 0; i < 10; ++i) selection->values0[i] = 0.0f;
    ApplyUpgrade0(record, selection);
    ApplyUpgrade1(record, selection);
    float weight;
    double fraction = selection->GetFraction(2, 1, &weight);
    if (fraction != 0.0) {
        float *source = reinterpret_cast<float *>(this);
        record->nitrousCapacity2A0 = (float)(((double)source[0x580 / 4] - record->nitrousCapacity2A0) * weight + record->nitrousCapacity2A0);
        float *value2AC = reinterpret_cast<float *>(reinterpret_cast<char *>(record) + 0x2ac);
        *value2AC = (float)(((double)source[0x58c / 4] - *value2AC) + *value2AC);
    }
    ApplyUpgrade3(record, selection);
    ApplyUpgrade4(record, selection);
    ApplyUpgrade5(record, selection);
    ApplyUpgrade6(record, selection);
    ApplyUpgrade7(record, selection);
    ApplyUpgrade8(record, selection);
    if (flag && !physicsUpgradeOverride7FC290) {
        int value = 9;
        // Original stores this target before clamping and passing it to search.
        volatile float desired = (float)(((double)selection->values0[8] + selection->values0[5] + selection->values0[0])
                               * physicsUpgradeFractionScale * physicsUpgradeFractionScale);
        if (desired <= 0.0f) desired = 0.0f;
        else if (desired >= 1.0f) desired = 1.0f;
        selection->SelectNearest(desired, &value, 1);
    }
    ApplyUpgrade9(record, selection);
    WheelIdentity(&record->body110.inertia10);
    double x = (double)record->body110.value4 * record->body110.value4;
    double y = (double)record->body110.value8 * record->body110.value8;
    double z = (double)record->body110.valueC * record->body110.valueC;
    record->body110.inertia10.elements[0] = (float)((z + y) * record->body110.mass0 * physicsInertiaScale);
    record->body110.inertia10.elements[5] = (float)((z + x) * record->body110.mass0 * physicsInertiaScale);
    record->body110.inertia10.elements[10] = (float)((x + y) * record->body110.mass0 * physicsInertiaScale);
}
