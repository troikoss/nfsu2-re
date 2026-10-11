#include "PhysicsUpgradeSource.h"
#include "VehicleMover.h"

// 0x5997e0. Unknown source fields retain their offsets. The engine's first two
// stores round to float without popping the x87 accumulator: later additions
// continue with its unrounded value. Volatile stores preserve those writes.
void PhysicsUpgradeSource::ApplyExtra(CarPhysicsRecord *record, PhysicsUpgradeSelection *selection) {
    float *source = reinterpret_cast<float *>(this);
    float *target = reinterpret_cast<float *>(record);
    if (selection->values28[0]) target[0x110 / 4] = (float)((1.0 - source[0x2b0 / 4]) * target[0x110 / 4]);
    if (selection->values28[1])
        *reinterpret_cast<unsigned int *>(target + 0x1c0 / 4) = *reinterpret_cast<unsigned int *>(source + 0x2b4 / 4);
    if (selection->values28[2]) target[0x2ac / 4] = (float)((double)source[0x2b8 / 4] + target[0x2ac / 4]);
    if (selection->values28[6]) target[0x274 / 4] = (float)((double)source[0x2bc / 4] + target[0x274 / 4]);
    if (selection->values28[8]) {
        target[0xdc / 4] = (float)((double)source[0x2c0 / 4] + target[0xdc / 4]);
        target[0xe0 / 4] = (float)((double)source[0x2c8 / 4] + target[0xe0 / 4]);
        target[0xfc / 4] = (float)((double)source[0x2c4 / 4] + target[0xfc / 4]);
        target[0x100 / 4] = (float)((double)source[0x2cc / 4] + target[0x100 / 4]);
    }
    for (int i = 0; i < 9; ++i) {
        volatile float *primary = target + 0x200 / 4 + i;
        double value = (double)(selection->values28[3] ? source[0x2d0 / 4 + i] : 0.0f) + *primary;
        *primary = (float)value;
        value = (double)(selection->values28[7] ? source[0x2f4 / 4 + i] : 0.0f) + value;
        *primary = (float)value;
        value += selection->values28[4] ? source[0x318 / 4 + i] : 0.0f;
        *primary = (float)value;
        target[0x224 / 4 + i] = (float)((double)(selection->values28[4] ? source[0x318 / 4 + i] : 0.0f) + target[0x224 / 4 + i]);
    }
}

// 0x599d90 removes adjustments before tuning. It does not restore the selector
// copied by flag 1, and its float writes need not invert prior rounding exactly.
void PhysicsUpgradeSource::RemoveExtra(CarPhysicsRecord *record, PhysicsUpgradeSelection *selection) {
    float *source = reinterpret_cast<float *>(this);
    float *target = reinterpret_cast<float *>(record);
    if (selection->values28[0]) target[0x110 / 4] = (float)((double)target[0x110 / 4] / (1.0 - source[0x2b0 / 4]));
    if (selection->values28[2]) target[0x2ac / 4] = (float)((double)target[0x2ac / 4] - source[0x2b8 / 4]);
    if (selection->values28[6]) target[0x274 / 4] = (float)((double)target[0x274 / 4] - source[0x2bc / 4]);
    if (selection->values28[8]) {
        target[0xdc / 4] = (float)((double)target[0xdc / 4] - source[0x2c0 / 4]);
        target[0xe0 / 4] = (float)((double)target[0xe0 / 4] - source[0x2c8 / 4]);
        target[0xfc / 4] = (float)((double)target[0xfc / 4] - source[0x2c4 / 4]);
        target[0x100 / 4] = (float)((double)target[0x100 / 4] - source[0x2cc / 4]);
    }
    for (int i = 0; i < 9; ++i) {
        volatile float *primary = target + 0x200 / 4 + i;
        double value = (double)*primary - (selection->values28[3] ? source[0x2d0 / 4 + i] : 0.0f);
        *primary = (float)value;
        value -= selection->values28[7] ? source[0x2f4 / 4 + i] : 0.0f;
        *primary = (float)value;
        value -= selection->values28[4] ? source[0x318 / 4 + i] : 0.0f;
        *primary = (float)value;
        target[0x224 / 4 + i] = (float)((double)target[0x224 / 4 + i] - (selection->values28[4] ? source[0x318 / 4 + i] : 0.0f));
    }
}
