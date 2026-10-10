#include "Drivetrain.h"

// Original shared random routine 0x43c170. Returns the old seed's remainder,
// while independently advancing the stored seed with the observed xor shifts.
float GetDrivetrainRandom(float range, unsigned int *state) {
    unsigned int original = *state;
    unsigned int remainder = original % 0x7fffffffu;
    unsigned int mixed = original ^ 0x1d872b41u;
    unsigned int shifted = (mixed >> 5) ^ mixed;
    *state = (shifted << 27) ^ shifted ^ mixed;
    return (int)remainder * drivetrainRandomScale * range;
}

// Original member wrapper 0x5ea500.
float DrivetrainRandom::GetScaledValue(float range) {
    return GetDrivetrainRandom(range, &stateF8);
}

// Original 0x594710. Preserve suppression and all three random draws.
void Drivetrain::RefreshVariations() {
    variation78[0] = 0.0f;
    variation78[1] = 0.0f;
    variation78[2] = 0.0f;
    DrivetrainContextEntry *entry = drivetrainContext->entryA0;
    if (entry && entry->flag41) {
        return;
    }
    variation78[0] = drivetrainRandom->GetScaledValue(rangesCC[1] - rangesCC[0]) + rangesCC[0];
    variation78[1] = drivetrainRandom->GetScaledValue(rangesCC[3] - rangesCC[2]) + rangesCC[2];
    variation78[2] = drivetrainRandom->GetScaledValue(rangesCC[5] - rangesCC[4]) + rangesCC[4];
}
