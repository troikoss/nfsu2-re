#include "Drivetrain.h"
#include "Engine.h"
#include "WheelAngle.h"

extern "C" double __cdecl sqrt(double);
extern "C" double __cdecl fabs(double);
#pragma intrinsic(sqrt)
#pragma intrinsic(fabs)

// Original PC v1.2 0x5a8280. First stack argument is unused.
void DrivetrainWheel::PrepareForces(float unidentifiedArgument, int mode) {
    float first = preparationSpec1C->coefficient4 * coefficient178 * wheelPreparationScale4;
    float second = preparationSpec1C->coefficient8 * coefficient17C * wheelPreparationScale8;
    // Original stores and reloads binary32 between these two additions.
    volatile float &drive = value108;
    if (!flag10C) {
        drive += value28 > 0.0f ? -first : first;
        drive += value28 > 0.0f ? -second : second;
    }
    WheelMatrix inverse;
    WheelTranspose(&inverse, &matrix70);
    inverse.elements[3] = inverse.elements[7] = inverse.elements[11] = 0.0f;
    WheelVector local;
    WheelTransform(&local, &inverse, &vectorF0);
    float angle = WheelAngle(local.x, local.y) * engineAngleUnit;
    if (angle > wheelForcePi) {
        angle -= wheelForcePi + wheelForcePi;
    }
    if (local.x < 0.0f) {
        angle = wheelForcePi + angle;
        if (angle > wheelForcePi) {
            angle -= wheelForcePi + wheelForcePi;
        }
    }
    value118 = spec14->value10 * value28 - local.x;
    value16C = value118;
    if (-wheelPreparationThreshold > value118 && value108 < 0.0f && second == 0.0f && !mode) {
        value108 = wheelPreparationThreshold / -value118 * value108;
    }
    float slip = value118;
    float sidewaysSquared = local.y * local.y;
    float length = (float)sqrt(slip * slip + sidewaysSquared);
    if (length != 0.0f && (local.y != 0.0f || local.x != 0.0f)) {
        float coefficient = wheelSurfaces[surface114].coefficient4 * coefficient184 * coefficient180 * correctionSpec18->coefficient10;
        if (auxiliary20->mode424 == 1) {
            coefficient = GetCorrectionScale() * coefficient;
        }
        float scale = value164 / length * wheelForceLimitScale * coefficient;
        float lockInput = value164 / (float)sqrt(local.x * local.x + sidewaysSquared) * wheelForceLimitScale * coefficient * (float)fabs(local.x);
        UpdateLockState(lockInput);
        if (fabs(slip) > wheelPreparationThreshold || fabs(angle) > correctionSpec18->value0 || flag10C) {
            flag160 = 1;
            value124 = slip * coefficient190 * scale;
            value128 = -(scale * coefficient194 * local.y);
            if (auxiliary20->value3EC * wheelPreparationSpeedScale < 1.0f && coefficient > wheelPreparationCoefficientThreshold) {
                float inverseCoefficient = 1.0f / coefficient;
                value124 *= inverseCoefficient;
                value128 = inverseCoefficient * value128;
            }
            value100 = slip;
            value104 = local.y;
            return;
        }
    }
    flag160 = 0;
    value124 = coefficient190 / spec14->value10 * value108;
    float direction = local.x > 0.0f ? 1.0f : -1.0f;
    float angleDirection = angle > 0.0f ? -1.0f : 1.0f;
    float force = EvaluateLateralForce(wheelForceLimitScale * value164, (float)fabs(angle)) * coefficient194;
    value100 = 0.0f;
    value104 = local.y;
    value128 = force * angleDirection * direction;
}
