#include "Drivetrain.h"
#include "Engine.h"

extern "C" double __cdecl fabs(double);
#pragma intrinsic(fabs)

// Original PC v1.2 0x59d400. Field names retain observed offsets.
void DrivetrainWheel::UpdateLockState(float input) {
    float total = (preparationSpec1C->coefficient8 * coefficient17C * wheelPreparationScale8
                 + preparationSpec1C->coefficient4 * coefficient178 * wheelPreparationScale4) * wheelLockSumScale;
    float required = (float)fabs(value28) * wheelLockValueScale + spec14->value10 * input;
    if (total > required) {
        flag10C = total > 1.0f ? 1 : 0;
        value28 = 0.0f;
    } else {
        flag10C = 0;
    }
}

// Original PC v1.2 0x59d510. The caller supplies a nonnegative angle.
float DrivetrainWheel::EvaluateLateralForce(float input, float angle) {
    if (!wheelLateralMode) {
        return wheelLateralFallbackCurve.Lookup(angle) * value164 * wheelForceLimitScale * wheelLateralFallbackScale;
    }
    float position = angle * wheelLateralAngleScale / correctionSpec18->value0 * wheelLateralIndexScale * 0.5f;
    int index = (int)position;
    float scaledInput = wheelLateralInputScale * input / correctionSpec18->divisor4;
    if (index >= 6) {
        return wheelLateralLastCurve->Lookup(scaledInput) * correctionSpec18->coefficient8 * wheelLateralOutputScale;
    }
    float first = wheelLateralCurves[index]->Lookup(scaledInput);
    float second = wheelLateralCurves[index+1]->Lookup(scaledInput);
    return ((second - first) * (position - index) + first) * correctionSpec18->coefficient8 * wheelLateralOutputScale;
}
