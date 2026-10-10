#include "Engine.h"

extern "C" double __cdecl fabs(double);
#pragma intrinsic(fabs)

// Original PC v1.2 0x594cb0. Initial pass, separate from project policies.
void Drivetrain::ApplyInitialDriveValue() {
    value70 = 0.0f;
    Engine *engine = (Engine *)component20;
    if (mode44 == 3) {
        DistributeDriveValue(engine->ComputeDriveValue());
    } else if (mode44 == 0 || mode44 == 2) {
        float difference = engine->state14 - wheelValue4C;
        if (difference > drivetrainDriveDifferenceLimit) {
            difference = drivetrainDriveDifferenceLimit;
        } else if (difference < -drivetrainDriveDifferenceLimit) {
            difference = -drivetrainDriveDifferenceLimit;
        }
        float coefficient = spec->coefficient14;
        float next = (wheelValue4C - engine->state14) * engineValueScale;
        if (previous58 != 0.0f && next * previous58 < 0.0f
                && fabs(next) > drivetrainCrossingThreshold
                && fabs(previous58) > drivetrainCrossingThreshold) {
            coefficient *= drivetrainCrossingScale;
        }
        previous58 = next;
        DistributeDriveValue(coefficient * state14 * difference);
    }
}

// Original PC v1.2 0x594da0. Called once in each of five mover correction passes.
void Drivetrain::ApplyDriveCorrection() {
    if (mode44 != 3) {
        return;
    }
    Engine *engine = (Engine *)component20;
    float drive = engine->ComputeDriveValue() + engine->feedback7C;
    float divisor = engine->spec->divisorC;
    int mode = engine->drivetrain20->mode44;
    if (mode == 0 || mode == 2) {
        divisor *= engineModeDivisorScale;
    }
    float engineRate = drive / divisor;
    float difference = engineRate - GetWheelDerivedRate();
    float denominator = 1.0f / engine->spec->divisorC;
    if (spec->selector10 <= 0.999f) {
        float ratio = spec->ratios[ratioIndex] * spec->coefficient8;
        denominator += (1.0f - coefficientEC) / wheels[0]->divisor2C * ratio * ratio * 0.5f;
    }
    if (spec->selector10 >= 0.001f) {
        float ratio = spec->ratios[ratioIndex] * spec->coefficientC;
        denominator += coefficientEC / wheels[2]->divisor2C * ratio * ratio * 0.5f;
    }
    DistributeDriveValue(difference / denominator);
}
