#include "VehicleMover.h"
#include "WheelMath.h"

extern float physicsUpgradeFractionScale;
extern float physicsVariantLowerThreshold;
extern float physicsVariantUpperThreshold;
extern float physicsInertiaScale;
extern float physicsVariant3Rows[];
extern float physicsVariant10Rows[];
extern float physicsVariant2Rows[];

// Original 0x5a7530, cdecl. The original retains interpolation and dimension
// products on its x87 stack until the field writes. Double intermediates retain
// that order under the demo's 53-bit precision without adding binary32 spills.
void ApplyPhysicsVariant(CarPhysicsRecord *record, PhysicsUpgradeSelection *selection, float *row) {
    double fraction = (double)selection->values0[9] * physicsUpgradeFractionScale;
    record->aero260.coefficient4 = (float)((((double)row[13] - row[2]) * fraction + row[2]) * record->aero260.coefficient4);
    record->aero260.coefficient8 = (float)((((double)row[14] - row[3]) * fraction + row[3]) * record->aero260.coefficient8);
    fraction = (double)selection->values0[0] * physicsUpgradeFractionScale;
    record->body110.inertiaScale50 = (float)((((double)row[15] - row[4]) * fraction + row[4]) * record->body110.inertiaScale50);
    record->body110.mass0 = (float)((((double)row[16] - row[5]) * fraction + row[5]) * record->body110.mass0);
    record->centre0.z = (float)((double)record->centre0.z - ((double)selection->values0[5] * physicsUpgradeFractionScale * ((double)row[17] - row[6]) + row[6]));
    fraction = (double)selection->values0[8] * physicsUpgradeFractionScale;
    for (int axle = 0; axle < 2; ++axle) {
        WheelCorrectionSpec *spec = &record->correctionD0[axle].spec;
        spec->coefficientC = (float)((((double)row[axle + 18] - row[axle + 7]) * fraction + row[axle + 7]) * spec->coefficientC);
        spec->coefficient10 = (float)((((double)row[axle + 20] - row[axle + 9]) * fraction + row[axle + 9]) * spec->coefficient10);
    }
    record->brakeBias27C = (float)((double)selection->values0[6] * physicsUpgradeFractionScale * ((double)row[11] - row[0]) + record->brakeBias27C + row[0]);
    record->drivetrain1B0.selector10 = (float)((double)selection->values0[1] * physicsUpgradeFractionScale * ((double)row[12] - row[1]) + row[1] + record->drivetrain1B0.selector10);
    if (record->centre0.z < 0.0f) record->centre0.z = 0.0f;
    if (record->brakeBias27C <= 0.0f) record->brakeBias27C = 0.0f;
    else if (record->brakeBias27C >= 1.0f) record->brakeBias27C = 1.0f;
    WheelIdentity(&record->body110.inertia10);
    double x = (double)record->body110.value4 * record->body110.value4;
    double y = (double)record->body110.value8 * record->body110.value8;
    double z = (double)record->body110.valueC * record->body110.valueC;
    record->body110.inertia10.elements[0] = (float)((z + y) * record->body110.mass0 * physicsInertiaScale);
    record->body110.inertia10.elements[5] = (float)((z + x) * record->body110.mass0 * physicsInertiaScale);
    record->body110.inertia10.elements[10] = (float)((y + x) * record->body110.mass0 * physicsInertiaScale);
}

// Original 0x5a7750 / 0x5a77c0 / 0x5a7830, stdcall with two arguments.
// Each mode uses a distinct table anchor with 0x144-byte class strides.
void __stdcall ApplyPhysicsVariant3(CarPhysicsRecord *record, PhysicsUpgradeSelection *selection) {
    int index = record->drivetrain1B0.selector10 < physicsVariantLowerThreshold ? 0 : 1;
    if (record->drivetrain1B0.selector10 > physicsVariantUpperThreshold) index = 2;
    ApplyPhysicsVariant(record, selection, physicsVariant3Rows + index * 81);
    record->nitrousCapacity2A0 = 0.0f;
}
void __stdcall ApplyPhysicsVariant10(CarPhysicsRecord *record, PhysicsUpgradeSelection *selection) {
    int index = record->drivetrain1B0.selector10 < physicsVariantLowerThreshold ? 0 : 1;
    if (record->drivetrain1B0.selector10 > physicsVariantUpperThreshold) index = 2;
    ApplyPhysicsVariant(record, selection, physicsVariant10Rows + index * 81);
    record->nitrousCapacity2A0 = 0.0f;
}
void __stdcall ApplyPhysicsVariant2(CarPhysicsRecord *record, PhysicsUpgradeSelection *selection) {
    int index = record->drivetrain1B0.selector10 < physicsVariantLowerThreshold ? 0 : 1;
    if (record->drivetrain1B0.selector10 > physicsVariantUpperThreshold) index = 2;
    ApplyPhysicsVariant(record, selection, physicsVariant2Rows + index * 81);
}
