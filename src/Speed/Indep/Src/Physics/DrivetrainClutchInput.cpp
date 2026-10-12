#include "VehicleMover.h"

extern float clutchEngagedThreshold;   // 0.95
extern float clutchReleasedThreshold;  // 0.05

// 0x594910: manual clutch input. Selects status 3 and records the input at
// +0x14 with mode +0x44: engaged (3) near or at 1, released (1) near or at 0,
// otherwise 2 while rising and 0 while falling.
void Drivetrain::SetClutchInput(float value) {
    status60 = 3;
    if (value > clutchEngagedThreshold) {
        mode44 = 3;
        state14 = 1.0f;
    } else if (value < clutchReleasedThreshold) {
        mode44 = 1;
        state14 = 0.0f;
    } else if (value == 1.0f) {
        mode44 = 3;
        state14 = value;
    } else if (value == 0.0f) {
        mode44 = 1;
        state14 = value;
    } else if (value > state14) {
        mode44 = 2;
        state14 = value;
    } else {
        state14 = value;
        mode44 = 0;
    }
}

// 0x5ecd70: a human car's drivetrain has a pending shift.
bool MoverModeOwner::HasPendingShift() {
    if (moverType28 != 1) return false;
    return mover34->drivetrain4C->pending48 != 0;
}

// 0x5ed470: per-wheel state of a human car's mover.
int MoverModeOwner::GetWheelState(int wheel) {
    if (moverType28 != 1) return 0;
    return mover34->value74[wheel];
}
