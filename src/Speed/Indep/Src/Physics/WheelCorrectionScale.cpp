#include "Drivetrain.h"

// Original PC v1.2 0x59d490. Offsets retained until original semantics are known.
float DrivetrainWheel::GetCorrectionScale() {
    if (flag10C || value28 < 0.0f || spec14->value18 < 2) {
        return 1.0f;
    }
    float fraction = (auxiliary20->value3EC - wheelCorrectionOffset) * wheelCorrectionRangeScale;
    if (fraction > 1.0f) {
        fraction = 1.0f;
    } else if (fraction < 0.0f) {
        fraction = 0.0f;
    }
    return (1.0f - wheelCorrectionMinimum) * fraction + wheelCorrectionMinimum;
}
