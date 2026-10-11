#include "VehicleMover.h"

// Original 0x5a7970. Padding and unobserved fields retain their prior bytes.
PhysicsTuning *PhysicsTuning::Construct() {
    for (int i = 0; i < 41; ++i) values14.values0[i] = 100;
    for (int i = 0; i < 23; ++i) values40[i] = 0.0f;
    for (int i = 0; i < 9; ++i) values9C[i] = valuesC0[i] = 0.0f;
    recordC = 0;
    selection8 = 0;
    value10 = valueE4 = 0;
    value4 = 1;
    value0 = -1;
    return this;
}

// Original 0x5ac6e0, parameter subobject at outer PhysicsInfo +0x10.
PhysicsParameters *PhysicsParameters::Construct() {
    record30.body110.value4 = 1.0f;
    record30.body110.value8 = 1.0f;
    record30.body110.valueC = 1.0f;
    record30.body110.inertiaScale50 = 1.0f;
    record30.body110.mass0 = 0.1f;
    for (int i = 0; i < 10; ++i) {
        selection2E0.values0[i] = 0.0f;
        selection2E0.values28[i] = 0;
    }
    for (int i = 0; i < 68; ++i) selection2E0.selected50[i] = 0;
    tuning374.Construct();
    driveCurve55C.count = 64;
    driveCurve55C.start = 0.0f;
    // Exact binary32 literals written as immediates by the original constructor.
    driveCurve55C.end = 55.5555572509765625f; // bits 0x425e38e4
    driveCurve55C.inverseSpacing = 1.13399994373321533203125f; // bits 0x3f9126e9
    driveCurve55C.samples = curveSamples45C;
    for (int i = 0; i < 63; ++i) curveSamples45C[i] = 4.0f;
    curveSamples45C[63] = 0.0f;
    value0 = -1;
    index4 = -1;
    value8 = value10 = value14 = value18 = value20 = value1C = 0.0f;
    selection2E0.ClearGroups(-1);
    tuning374.recordC = &record30;
    tuning374.selection8 = &selection2E0;
    tuning374.value0 = -1;
    return this;
}
