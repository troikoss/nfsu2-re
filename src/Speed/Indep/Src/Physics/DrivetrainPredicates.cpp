#include "Drivetrain.h"
// Declaration for the standard compiler intrinsic; the matching toolchain
// bundle does not include CRT headers. No replacement absolute-value formula.
extern "C" double __cdecl fabs(double);
#pragma intrinsic(fabs)

// PC v1.2 0x594e90 and 0x594ed0. Component units remain unresolved.
int Drivetrain::IsBelowComponentLimit() {
    if (component20->value78 < drivetrainComponentThreshold &&
        component20->values18[0] * drivetrainComponentScale + drivetrainComponentEpsilon >= wheelValue4C) {
        return 1;
    }
    return 0;
}

int Drivetrain::IsWithinComponentTolerance() {
    return fabs(component20->value14 - wheelValue4C) <= drivetrainDifferenceTolerance;
}
