#include "VehicleMover.h"
#include "PhysicsUpgradeSource.h"

extern float physicsTuningCentre;
extern float physicsTuningScale;
extern float physicsTuningMinimum;
extern float physicsTuningMaximum;
extern float physicsTuningEngineScale;
extern unsigned char *carTypeInfoTable;

// 0x59aee0, receiver is tuning +0x14. Lower comparison uses the unrounded
// result; the upper comparison and return use the preceding binary32 spill.
double PhysicsTuningValues::GetOffset(int index, int component) {
    double value = 0.0;
    if (index < 23) value = values0[index];
    else if (index == 23) value = values0[23 + component];
    else if (index == 24) value = values0[32 + component];
    value = (value - physicsTuningCentre) * physicsTuningScale;
    float rounded = (float)value;
    if (value > physicsTuningMinimum) {
        if (physicsTuningMaximum < rounded) return physicsTuningMaximum;
        return rounded;
    }
    return physicsTuningMinimum;
}

// 0x59b200. Every caller passes the tuning object in ecx, which the original
// ignores. This is address selection only; components are not validated.
// Unknown record fields retain their byte offsets.
float *PhysicsTuning::GetField(void *record, int index, int component) {
    int offset;
    switch (index) {
    case 0: offset = 0x174; break;
    case 1: offset = 0x178 + component * 4; break;
    case 2: offset = 0x180; break;
    case 3: offset = 0x18 + component * 0x30; break;
    case 4: offset = 0x194; break;
    case 5: offset = 0x198 + component * 4; break;
    case 6: offset = 0x1a0; break;
    case 7: offset = 0x78 + component * 0x30; break;
    case 8: offset = 0x270; break;
    case 9: offset = 0x1b8 + component * 4; break;
    case 10: case 11: case 12: case 13: case 14: case 15:
        offset = 0x1b0 + index * 4; break;
    case 16: offset = 0x2a0; break;
    case 17: offset = 0x2ac; break;
    case 18: offset = 0x27c; break;
    case 19: offset = component == 0 ? 0xdc : 0xe0; break;
    case 20: offset = component == 0 ? 0xfc : 0x100; break;
    case 21: offset = 0x264; break;
    case 22: offset = 0x268; break;
    case 23: offset = 0x200 + component * 4; break;
    case 24: offset = 0x224 + component * 4; break;
    default: return 0;
    }
    return reinterpret_cast<float *>(static_cast<char *>(record) + offset);
}

// 0x59b3c0: neither the nine-sample sum nor the common correction is spilled
// to binary32. Re-read the record pointer for each original field write.
void PhysicsTuning::NormalizeEngine(float total) {
    float *samples = reinterpret_cast<float *>(recordC) + 0x200 / 4;
    double sum = samples[0];
    for (int i = 1; i < 9; ++i) sum += samples[i];
    double correction = ((double)total - sum) * physicsTuningEngineScale;
    for (int i = 0; i < 9; ++i) {
        float *sample = reinterpret_cast<float *>(recordC) + 0x200 / 4 + i;
        *sample = (float)(correction + *sample);
    }
}

// 0x5a7da0 brackets reads with removal/application only at nesting depth zero.
// It stores the queried value as binary32 before reapplying extra adjustments.
double PhysicsTuning::GetValue(int index, int component) {
    if (valueE4 == 0) {
        PhysicsUpgradeSource *source = reinterpret_cast<PhysicsUpgradeSource *>(carTypeInfoTable + value0 * 0x890 + 0x110);
        source->RemoveExtra(recordC, selection8);
    }
    ++valueE4;
    CarPhysicsRecord *record = recordC;
    float value = 0.0f;
    switch (index) {
    case 1: case 5: case 19: case 20:
        for (int i = 0; i < 2; ++i) value = (float)((double)value + *GetField(record, index, i));
        value = (float)((double)value * 0.5);
        break;
    case 3: case 7: {
        double current = *GetField(record, index, 0);
        PhysicsUpgradeSource *source = reinterpret_cast<PhysicsUpgradeSource *>(carTypeInfoTable + value0 * 0x890 + 0x110);
        value = (float)-(current - *GetField(source, index, 0));
        break;
    }
    case 10: case 11: case 12: case 13: case 14: case 15:
        value = *GetField(record, index, 0);
        break;
    case 23:
        value = (float)((double)*GetField(record, index, component)
                - reinterpret_cast<float *>(record)[0x224 / 4 + component]);
        break;
    default:
        value = *GetField(record, index, component);
        break;
    }
    --valueE4;
    if (valueE4 == 0) {
        PhysicsUpgradeSource *source = reinterpret_cast<PhysicsUpgradeSource *>(carTypeInfoTable + value0 * 0x890 + 0x110);
        source->ApplyExtra(record, selection8);
    }
    return value;
}
