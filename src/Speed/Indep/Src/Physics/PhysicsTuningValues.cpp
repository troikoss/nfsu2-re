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

extern float physicsTuningRounding;
extern float physicsTuningHundred;
extern float physicsTuningHundredth;
extern float physicsTuningInchesPerMetre;
extern float physicsTuningMetresPerInch;
extern float physicsTuningThousand;
extern float physicsTuningThousandth;
extern float physicsTuningFootPoundsPerNewtonMetre;
extern double physicsTuningNewtonMetresPerFootPound;
extern double physicsTuningThousandthDouble;
extern float physicsTuningKphPerMps;
extern float physicsTuningMpsPerKph;
extern float physicsTuningMphPerMps;
extern float physicsTuningMpsPerMph;
extern float physicsTuningRpmScale;
extern float physicsTuningLowerBlend;
extern float physicsTuningUpperBlend;

// 0x59af80, receiver is tuning +0x14. The scaled value is truncated by
// __ftol2 after adding 100.5, then clamped to 0..200 before the byte store.
void PhysicsTuningValues::SetOffset(float value, int index, int component) {
    int stored = (int)((double)value * physicsTuningHundred + physicsTuningRounding);
    if (stored <= 0) stored = 0;
    else if (stored > 200) stored = 200;
    if (index < 23) values0[index] = (unsigned char)stored;
    else if (index == 23) values0[23 + component] = (unsigned char)stored;
    else if (index == 24) values0[32 + component] = (unsigned char)stored;
}

// 0x59b080 is the per-kind test used by CountActive; kind -1 skips lookup.
bool PhysicsTuning::IsActive(int index) {
    bool active = index != -1 && FindPhysicsTuningEntry(index, selection8) != 0;
    if (index >= 10 && index <= 15)
        return active && index - 8 < recordC->drivetrain1B0.unidentified18 + 2;
    return active;
}

// 0x59b160. Entry kinds 2/3 interpolate across the pair with an unspilled
// range; 0/1 offset both values; 4 narrows them with 0x401070/0x401050.
void PhysicsTuning::ApplyEntry(int index, float *first, float *second) {
    PhysicsTuningEntry *entry = FindPhysicsTuningEntry(index, selection8);
    if (entry == 0) return;
    unsigned char kind = entry->kind6;
    if (kind == 2 || kind == 3) {
        double range = (double)*second - *first;
        *first = (float)(range * entry->value8 + *first);
        *second = (float)(range * entry->valueC + *second);
    } else if (kind == 0 || kind == 1) {
        *first = (float)((double)entry->value8 + *first);
        *second = (float)((double)entry->valueC + *second);
    } else if (kind == 4) {
        *first = EngineMinimum(*first, entry->value8);
        *second = EngineMaximum(*second, entry->valueC);
    }
}

// 0x59b4a0. Selection +4 equal to one chooses the first scale set. Products
// are returned unspilled; the imperial torque path multiplies binary64 constants.
// MSVC 7.1 reorders unparenthesized constant chains, so grouping is explicit.
double PhysicsTuning::ConvertUnits(int index, float value, bool forward) {
    switch (index) {
    case 3: case 7:
        if (value4 == 1) return forward ? (double)value * physicsTuningHundred : (double)value * physicsTuningHundredth;
        return forward ? (double)value * physicsTuningInchesPerMetre : (double)value * physicsTuningMetresPerInch;
    case 10: case 11: case 12: case 13: case 14: case 15:
        if (value4 == 1) return forward ? (double)value * physicsTuningKphPerMps : (double)value * physicsTuningMpsPerKph;
        return forward ? (double)value * physicsTuningMphPerMps : (double)value * physicsTuningMpsPerMph;
    case 23: case 24:
        if (value4 == 1) return forward ? (double)value * physicsTuningThousand : (double)value * physicsTuningThousandth;
        if (forward) return ((double)value * physicsTuningThousand) * physicsTuningFootPoundsPerNewtonMetre;
        return ((double)value * physicsTuningNewtonMetresPerFootPound) * physicsTuningThousandthDouble;
    }
    return value;
}

// 0x59b5c0. Record +0x1c0 selects or blends record +0x50/+0x80; NaN leaves
// zero. The scaled +0x1f4 value is spilled to binary32, the blend is not.
double PhysicsTuning::GetGearValue(float value, bool inverse) {
    float *record = reinterpret_cast<float *>(recordC);
    double blend = 0.0;
    float scaled = (float)((double)record[0x1f4 / 4] * physicsTuningRpmScale);
    float ratio = record[0x1b8 / 4];
    float selector = record[0x1c0 / 4];
    if (selector < physicsTuningLowerBlend) blend = record[0x50 / 4];
    else if (selector > physicsTuningUpperBlend) blend = record[0x80 / 4];
    else if (selector >= physicsTuningLowerBlend && selector <= physicsTuningUpperBlend)
        blend = (1.0 - record[0x1c0 / 4]) * record[0x50 / 4] + (double)record[0x80 / 4] * record[0x1c0 / 4];
    if (inverse) {
        if (value == 0.0f) return 0.0;
        return blend * (scaled / ((double)ratio * value));
    }
    if (value == 0.0f) return 0.0;
    return ((double)scaled / ratio) * (blend / value);
}

// 0x59b6c0 ignores its receiver; kinds 3, 7 and 9 have a negative sense.
float PhysicsTuning::GetSign(int index) {
    float sign = 1.0f;
    if (index == 3 || index == 7 || index == 9) return physicsTuningMinimum;
    return sign;
}

extern float physicsTuningHalf;

// 0x591320 widens a two-float range; unordered comparisons take the value.
// A replaced maximum is copied as raw bits.
void ExpandPhysicsTuningRange(float *range, float value) {
    range[0] = range[0] < value ? range[0] : value;
    if (!(range[1] > value))
        *reinterpret_cast<unsigned int *>(range + 1) = *reinterpret_cast<unsigned int *>(&value);
}

// 0x5a79f0. Kinds 0/4, 2/6, 3/7 and 21/22 are linked; others read only
// themselves. Negative-sense kinds take the minimum, others the maximum.
float PhysicsTuning::GetLinkedExtreme(int index, int component) {
    int linked[2];
    linked[1] = -1;
    switch (index) {
    case 0: case 4: linked[0] = 0; linked[1] = 4; break;
    case 2: case 6: linked[0] = 2; linked[1] = 6; break;
    case 21: case 22: linked[0] = 21; linked[1] = 22; break;
    case 3: case 7: linked[0] = 3; linked[1] = 7; break;
    default: linked[0] = index; break;
    }
    float result = -10000000.0f;
    if (index == 3 || index == 7 || index == 9) result = 10000000.0f;
    for (int i = 0; i < 2; ++i) {
        int kind = linked[i];
        if (kind == -1) continue;
        float value = 0.0f;
        if (kind < 23) value = values40[kind];
        else if (kind == 23) value = values9C[component];
        else if (kind == 24) value = valuesC0[component];
        if (index == 3 || index == 7 || index == 9) {
            if (!(result < value)) result = value;
        } else if (!(result > value)) result = value;
    }
    return result;
}

// 0x5a7b80: stock bounds for kinds 19/20 come from CarTypeInfo +0x1ec/+0x65c
// pairs and are adjusted by the selected entries before selection/averaging.
void PhysicsTuning::GetBalanceRange(int index, int component, bool average, float *minimum, float *maximum) {
    float *source = reinterpret_cast<float *>(carTypeInfoTable + value0 * 0x890 + 0x110);
    float minima[4];
    float maxima[4];
    minima[0] = source[0xdc / 4];
    maxima[0] = source[0x54c / 4];
    ApplyEntry(19, &minima[0], &maxima[0]);
    minima[1] = source[0xe0 / 4];
    maxima[1] = source[0x550 / 4];
    ApplyEntry(19, &minima[1], &maxima[1]);
    minima[2] = source[0xfc / 4];
    maxima[2] = source[0x56c / 4];
    ApplyEntry(20, &minima[2], &maxima[2]);
    minima[3] = source[0x100 / 4];
    maxima[3] = source[0x570 / 4];
    ApplyEntry(20, &minima[3], &maxima[3]);
    int pair = index != 19;
    if (average) {
        *minimum = (float)(((double)minima[pair * 2 + 1] + minima[pair * 2]) * physicsTuningHalf);
        *maximum = (float)(((double)maxima[pair * 2 + 1] + maxima[pair * 2]) * physicsTuningHalf);
    } else {
        *minimum = minima[component + pair * 2];
        *maximum = maxima[component + pair * 2];
    }
}

// 0x5a7cb0: the same range selection for kinds 1/5 from CarTypeInfo
// +0x288/+0x6d8 and +0x2a8/+0x6f8, built per component.
void PhysicsTuning::GetPairRange(int index, int component, bool average, float *minimum, float *maximum) {
    float *source = reinterpret_cast<float *>(carTypeInfoTable + value0 * 0x890 + 0x110);
    float minima[4];
    float maxima[4];
    for (int i = 0; i < 2; ++i) {
        minima[i] = source[0x178 / 4 + i];
        maxima[i] = source[0x5c8 / 4 + i];
        ApplyEntry(1, &minima[i], &maxima[i]);
        minima[2 + i] = source[0x198 / 4 + i];
        maxima[2 + i] = source[0x5e8 / 4 + i];
        ApplyEntry(5, &minima[2 + i], &maxima[2 + i]);
    }
    int pair = index != 1;
    if (average) {
        *minimum = (float)(((double)minima[pair * 2 + 1] + minima[pair * 2]) * physicsTuningHalf);
        *maximum = (float)(((double)maxima[pair * 2 + 1] + maxima[pair * 2]) * physicsTuningHalf);
    } else {
        *minimum = minima[component + pair * 2];
        *maximum = maxima[component + pair * 2];
    }
}
