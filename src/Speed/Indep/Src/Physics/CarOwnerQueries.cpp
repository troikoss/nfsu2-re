#include "VehicleMover.h"
#include "Engine.h"

extern float drivetrainFinishingInputThreshold;
extern float drivetrainSelectorUpperThreshold;
extern int ambientDisabled8A1D48;
extern unsigned char ambientForced89E7E6;
extern float ambientTime7FB718;
extern float ambientPeriod7A67B8;
extern float ambientPositionScale7A67BC;
extern float ambientFactor89F048;
extern int draftEnabled8A1F34;
extern class DraftTracker draftTracker8A1F28;

// Original 0x401020: degrees to 16-bit angle units.
int DegreesToAngleUnits(float degrees) {
    return (int)(degrees * 65536.0f) / 360;
}

// Original 0x43c370: cosine through the quantized sine with a quarter-turn offset.
float EngineCosineUnits(unsigned int units) {
    return EngineSineUnits(units + 0x4000);
}

// Original 0x592870: whether a wheel index is driven for the record's axle selector.
int DrivetrainAuxiliary::IsWheelDriven(int wheel) {
    float selector = record0->drivetrain1B0.selector10;
    if (!(selector >= drivetrainFinishingInputThreshold)) {
        return wheel == 1 || wheel == 0;
    }
    if (selector > drivetrainSelectorUpperThreshold) {
        return wheel == 2 || wheel == 3;
    }
    return 1;
}

// Original 0x5f67f0.
bool MoverModeOwner::HasWheelEffects() {
    return context14->value4 != 3 && !effectsSuppressed998 && effectsEnabled550 && colliderPack5CC;
}

// Original 0x6134e0: periodic ambient factor; also stored at 0x89f048.
float ComputeAmbientFactor(float position, float unused) {
    if (ambientDisabled8A1D48) {
        return 0.0f;
    }
    if (ambientForced89E7E6) {
        return 1.0f;
    }
    volatile float phase = ambientTime7FB718 / ambientPeriod7A67B8 + position / ambientPositionScale7A67BC;
    float value = EngineCosineUnits(DegreesToAngleUnits(phase)) * 0.5f + 0.5f;
    value = value < 0.0f ? -value : value;
    ambientFactor89F048 = value;
    return value;
}

// Original 0x61e380: draft amount for a position pair when draft tracking is enabled.
float QueryDraftValue(float first, float second) {
    volatile float ambient = ComputeAmbientFactor(first, second);
    if (!draftEnabled8A1F34) {
        return 0.0f;
    }
    draftTracker8A1F28.Update(first, second);
    if (draftTracker8A1F28.GetValue(1) < ambient) {
        return 0.0f;
    }
    return draftTracker8A1F28.GetValue(0);
}

// Original 0x5ed5d0.
float MoverModeOwner::GetDraftValue() {
    return QueryDraftValue(view40.draftPosition60, view40.draftPosition64);
}

// Original 0x5f0b40 (draft tracker at 0x8a1f28).
float DraftTracker::GetValue(int index) {
    if (table0C && offset18) {
        float *value = reinterpret_cast<float *>(table0C->offsets10[index] + offset18);
        if (!value) {
            return 0.0f;
        }
        return *value;
    }
    return 0.0f;
}
