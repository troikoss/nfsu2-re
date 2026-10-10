#include "VehicleMover.h"
#include "Engine.h"

extern "C" double __cdecl sqrt(double);
#pragma intrinsic(sqrt)
extern EngineCurve bodyDampingCurve;
extern EngineCurve bodyScalarCurve;

// Original PC v1.2 0x59c1e0. Output holes are deliberately left untouched.
float *MoverBody::WriteDerivative(float *output) {
    if (value204 != 0.0f) {
        double threshold = value208 * (double)value204 + bodyThresholdBase;
        if (flag15C) threshold *= bodyFlagThresholdScale;
        float tilt = matrix30.elements[2];
        double factor;
        if (tilt > 0.0f) {
            if (tilt > bodyPositiveTiltLimit) tilt = bodyPositiveTiltLimit;
            factor = 1.0 - (1.0 - bodyPositiveTiltFactor) / bodyPositiveTiltLimit * tilt;
        } else {
            tilt = -tilt;
            if (tilt > bodyNegativeTiltLimit) tilt = bodyNegativeTiltLimit;
            factor = (bodyNegativeTiltFactor - 1.0) / bodyNegativeTiltLimit * tilt + 1.0;
        }
        threshold *= factor;
        if (sqrt(vectorA0.x * vectorA0.x + vectorA0.y * vectorA0.y) * bodyHorizontalScale > threshold
            && matrix30.elements[2] * vector110.z + matrix30.elements[1] * vector110.y + matrix30.elements[0] * vector110.x > 0.0f) {
            WheelMatrix inverse;
            WheelTranspose(&inverse, &matrix30);
            WheelVector local;
            WheelTransform(&local, &inverse, &vector110);
            float response = -(bodyTiltResponseScale * tilt);
            if (response > bodyTiltResponseMaximum) response = bodyTiltResponseMaximum;
            local.x = response;
            WheelTransform(&vector110, &matrix30, &local);
        }
    }
    output[0] = vectorA0.x; output[1] = vectorA0.y; output[2] = vectorA0.z;
    BodyMatrixDerivative((WheelMatrix *)(output + 4), &vectorC0, &matrix30);
    output[20] = vector110.x; output[21] = vector110.y; output[22] = vector110.z;
    output[24] = force120.x; output[25] = force120.y; output[26] = force120.z;
    float cLength = (float)sqrt(vectorC0.x * vectorC0.x + vectorC0.y * vectorC0.y + vectorC0.z * vectorC0.z);
    if (flag154) {
        double scale = -bodyDampingCurve.Lookup(cLength);
        double x = scale * vector80.x;
        volatile float y = vector80.y * scale;
        volatile float z = vector80.z * scale;
        output[24] = x + output[24]; output[25] = y + output[25]; output[26] = z + output[26];
    }
    float aRate = (float)(sqrt(vectorA0.x * vectorA0.x + vectorA0.y * vectorA0.y + vectorA0.z * vectorA0.z) / bodyAScale);
    double cRate = cLength / (double)bodyCScale;
    if (cRate > aRate) aRate = (float)cRate;
    output[28] = bodyScalarCurve.Lookup(aRate) > value90 ? bodyScalarRise : -bodyScalarFall;
    if (flag150) {
        double angular = -(bodyAngularDamping * (double)value90);
        double x = angular * vector80.x;
        volatile float y = vector80.y * angular;
        volatile float z = vector80.z * angular;
        output[24] = x + output[24]; output[25] = y + output[25]; output[26] = z + output[26];
        double linear = -(bodyLinearDamping * (double)value90);
        x = linear * vector70.x;
        y = vector70.y * linear; z = vector70.z * linear;
        output[20] = x + output[20]; output[21] = y + output[21]; output[22] = z + output[22];
    }
    if (limit200 != 0.0f) {
        if (force120.z > limit200) output[26] = limit200;
        else if (force120.z < -limit200) output[26] = -limit200;
    }
    return output;
}
