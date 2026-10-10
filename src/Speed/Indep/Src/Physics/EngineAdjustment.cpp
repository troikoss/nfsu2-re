#include "Engine.h"

// Original PC v1.2 0x5942e0.
float Engine::GetEffectiveControl() {
    return flag88 ? 0.0f : control80 * input78;
}

// Original PC v1.2 0x5a11e0. Adjustment meaning is not established by offsets.
void Engine::AdvanceAdjustment(float duration) {
    if (adjustment8C < 0.0f || adjustment8C >= engineAdjustmentStop) {
        return;
    }
    if (drivetrain20->ratioIndex == 2 && drivetrain20->adjustment68 > 0.0f) {
        return;
    }
    float rate = spec->value4 < state14 * engineValueScale ? engineAdjustmentAboveRate : engineAdjustmentBelowRate;
    float value = adjustment8C + rate * duration;
    if (value < 0.0f) {
        value = 0.0f;
    }
    adjustment8C = value;
    if (value >= engineAdjustmentUpper) {
        adjustment8C = engineAdjustmentStop;
    }
}
