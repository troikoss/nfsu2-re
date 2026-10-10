#include "VehicleMover.h"

extern "C" double __cdecl sqrt(double);
#pragma intrinsic(sqrt)

// Original PC v1.2 0x59c5f0, body vtable slot 0x1c.
void MoverBody::PostUpdate() {
    vectorA0.x = value168 * vector70.x;
    vectorA0.y = value168 * vector70.y;
    vectorA0.z = value168 * vector70.z;
    double length = sqrt(vectorA0.x * vectorA0.x + vectorA0.y * vectorA0.y + vectorA0.z * vectorA0.z);
    volatile float storedLength = (float)length;
    if (length > bodyPostSpeedLimit && divisor164 > bodyPostMassThreshold) {
        double scale = bodyPostSpeedLimit / storedLength;
        vector70.x = (float)(scale * vector70.x);
        vector70.y = (float)(scale * vector70.y);
        vector70.z = (float)(scale * vector70.z);
        vectorA0.x = value168 * vector70.x;
        vectorA0.y = value168 * vector70.y;
        vectorA0.z = value168 * vector70.z;
    }
    double response = value168 * bodyPostResponseScale;
    vectorB0.x = (float)(response * vector110.x);
    vectorB0.y = (float)(response * vector110.y);
    vectorB0.z = (float)(response * vector110.z);
    WheelMatrix transpose, first, second;
    WheelTranspose(&transpose, &matrix30);
    WheelMultiply(&first, &matrix30, flag158 ? &matrix1B0 : &matrix170);
    WheelMultiply(&second, &first, &transpose);
    WheelTransform(&vectorC0, &second, &vector80);
    length = sqrt(vectorC0.x * vectorC0.x + vectorC0.y * vectorC0.y + vectorC0.z * vectorC0.z);
    if (length > bodyPostAngularLimit) {
        double scale = bodyPostAngularLimit / length;
        vectorC0.x = (float)(scale * vectorC0.x);
        vectorC0.y = (float)(scale * vectorC0.y);
        vectorC0.z = (float)(scale * vectorC0.z);
    }
    if (value90 < 0.0f) value90 = 0.0f;
    for (int i = 0; i < 12; ++i) matrixD0.elements[i] = matrix30.elements[i];
    matrixD0.elements[12] = position20.x;
    matrixD0.elements[13] = position20.y;
    matrixD0.elements[14] = position20.z;
    matrixD0.elements[15] = 1.0f;
    saved130 = vector110;
    saved140 = force120;
    vector110.x = vector110.y = vector110.z = 0.0f;
    force120.x = force120.y = force120.z = 0.0f;
    if (value160 > 0.0f) {
        WheelVector force = {0.0f, 0.0f, -(divisor164 * value160)};
        WheelVector point = {0.0f, 0.0f, 0.0f};
        ApplyForceAtPoint(&force, &point);
    }
}
