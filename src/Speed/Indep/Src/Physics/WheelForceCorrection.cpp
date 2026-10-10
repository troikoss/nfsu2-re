#include "Drivetrain.h"
#include "Engine.h"
#include "WheelAngle.h"

extern "C" double __cdecl sqrt(double);
extern "C" double __cdecl fabs(double);
#pragma intrinsic(sqrt)
#pragma intrinsic(fabs)

// Original PC v1.2 0x59d600. The stack argument is unused by this routine.
void DrivetrainWheel::CorrectForces(float unidentifiedArgument) {
    bool scaled = false;
    if (value108 > 0.0f && !flag10C) {
        value124 *= wheelDriveForceScale;
        scaled = true;
    }
    float length = (float)sqrt(value128 * value128 + value124 * value124);
    float limit = wheelSurfaces[surface114].coefficient * correctionSpec18->coefficientC
                * coefficient188 * coefficient184 * coefficient180 * value164 * wheelForceLimitScale;
    if (auxiliary20->mode424 == 1) {
        limit = GetCorrectionScale() * limit;
    }
    if (length > limit && length > 0.001f) {
        float scale = limit / length;
        value124 = scale * value124;
        value128 = value128 * scale;
    } else if (scaled) {
        value124 /= wheelDriveForceScale;
    }
    if (limit > 0.001f) {
        value170 = length / limit;
        if (value170 > 1.0f) {
            value170 = 1.0f;
        }
    } else {
        value170 = 1.0f;
    }
    if (flag160 || flag10C) {
        value170 = 1.0f;
    }
    if (!flag11C) {
        value170 = 0.0f;
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
    EngineSine(angle + engineHalfPi); // Original evaluates and discards this result.
    float correction = EngineSine(angle) * value128;
    if (fabs(local.x) > 1.0f) {
        value124 -= correction * wheelForceAngleScale;
    }
    WheelMatrix basis;
    WheelIdentity(&basis);
    WheelVector *first = (WheelVector *)&matrix70.elements[0];
    WheelVector *second = (WheelVector *)&matrix70.elements[4];
    WheelVector *outFirst = (WheelVector *)&basis.elements[0];
    WheelVector *outSecond = (WheelVector *)&basis.elements[4];
    WheelVector *outThird = (WheelVector *)&basis.elements[8];
    if (wheelForceBasisMode == 1) {
        for (int i = 0; i < 12; ++i) {
            basis.elements[i] = matrix70.elements[i];
        }
    } else if (wheelForceBasisMode == 2) {
        float scale = -(vector150.z * first->z + vector150.y * first->y + first->x * vector150.x);
        WheelScaleAdd(outFirst, first, &vector150, scale);
        basis.elements[3] = 0.0f;
        scale = -(vector150.z * second->z + vector150.y * second->y + second->x * vector150.x);
        WheelScaleAdd(outSecond, second, &vector150, scale);
        basis.elements[7] = 0.0f;
        *outThird = vector150;
        basis.elements[11] = 0.0f;
    } else if (wheelForceBasisMode == 3) {
        float scale = -(first->z * vector150.z + first->y * vector150.y + vector150.x * first->x);
        WheelScaleAdd(outFirst, first, &vector150, scale);
        WheelNormalize(outFirst, outFirst);
        *outThird = vector150;
        basis.elements[3] = basis.elements[11] = 0.0f;
        WheelCross(outSecond, outThird, outFirst);
        basis.elements[7] = 0.0f;
    } else if (wheelForceBasisMode == 4) {
        for (int i = 0; i < 8; ++i) {
            basis.elements[i] = matrix70.elements[i];
        }
        *outThird = vector150;
        basis.elements[11] = 0.0f;
    }
    WheelVector forces = {value124, value128, value164};
    WheelTransform(&vector130, &basis, &forces);
}
