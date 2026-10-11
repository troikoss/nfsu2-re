#include "VehicleMover.h"
#include "../../Libs/bWare/Strings.h"

extern "C" double __cdecl sqrt(double);
#pragma intrinsic(sqrt)

extern float bodyGravity;
extern float bodyImpulseLimit;
extern float bodyAngularImpulseLimit;

// Original PC v1.2 0x43c7c0: affine inverse through the 3x3 adjugate.
// Cofactors, determinant reciprocal and outputs cross the observed binary32 stores.
void WheelInvert(WheelMatrix *output, WheelMatrix *input) {
    float *m = input->elements;
    float a00 = m[0], a01 = m[1], a02 = m[2];
    float a10 = m[4], a11 = m[5], a12 = m[6];
    float a20 = m[8], a21 = m[9], a22 = m[10];
    float tx = m[12], ty = m[13], tz = m[14];
    float *o = output->elements;
    o[3] = 0.0f;
    double c00 = a11 * a22 - a21 * a12;
    volatile float c10 = a22 * a10 - a20 * a12;
    volatile float c20 = a21 * a10 - a20 * a11;
    volatile float inverse = 1.0f / (c00 * a00 - c10 * a01 + c20 * a02);
    float r0 = (float)(c00 * inverse);
    o[0] = r0;
    float r1 = -((a22 * a01 - a21 * a02) * inverse);
    o[1] = r1;
    float r2 = (a12 * a01 - a11 * a02) * inverse;
    o[2] = r2;
    float r4 = -(c10 * inverse);
    o[4] = r4;
    float r5 = (a22 * a00 - a20 * a02) * inverse;
    o[7] = 0.0f;
    o[11] = 0.0f;
    o[15] = 1.0f;
    o[5] = r5;
    float r6 = -((a12 * a00 - a10 * a02) * inverse);
    o[6] = r6;
    float r8 = c20 * inverse;
    o[8] = r8;
    float r9 = -((a21 * a00 - a20 * a01) * inverse);
    o[9] = r9;
    float r10 = (a11 * a00 - a10 * a01) * inverse;
    o[10] = r10;
    o[12] = -(r8 * tz + r4 * ty + r0 * tx);
    o[13] = -(r9 * tz + r5 * ty + r1 * tx);
    o[14] = -(r10 * tz + r6 * ty + r2 * tx);
}

// Original 0x5bb0d0: rebuild an orthonormal rotation from rows 0 and 1.
void WheelOrthonormalize(WheelMatrix *output, WheelMatrix *input) {
    WheelVector x, y, z;
    WheelNormalize(&x, reinterpret_cast<WheelVector *>(&input->elements[0]));
    WheelCross(&z, &x, reinterpret_cast<WheelVector *>(&input->elements[4]));
    WheelNormalize(&z, &z);
    WheelCross(&y, &z, &x);
    float *o = output->elements;
    o[0] = x.x;
    o[2] = x.z;
    o[1] = x.y;
    o[5] = y.y;
    o[4] = y.x;
    o[6] = y.z;
    o[9] = z.y;
    o[8] = z.x;
    o[10] = z.z;
    o[3] = 0.0f;
    o[7] = 0.0f;
    o[11] = 0.0f;
    o[12] = 0.0f;
    o[13] = 0.0f;
    o[14] = 0.0f;
    o[15] = 1.0f;
}

// Constructor body of original 0x5a7f30 ("CRigid"); vtable 0x7a20c4 is installed by the caller.
void MoverBody::Construct(MoverBodySpec *spec) {
    spec14 = spec;
    position20.x = 0.0f;
    position20.y = 0.0f;
    position20.z = 0.0f;
    WheelIdentity(&matrix30);
    vector70.x = vector70.y = vector70.z = 0.0f;
    vector80.x = vector80.y = vector80.z = 0.0f;
    value90 = 0.0f;
    vectorA0.x = vectorA0.y = vectorA0.z = 0.0f;
    vectorB0.x = vectorB0.y = vectorB0.z = 0.0f;
    vectorC0.x = vectorC0.y = vectorC0.z = 0.0f;
    vector110.x = vector110.y = vector110.z = 0.0f;
    force120.x = force120.y = force120.z = 0.0f;
    saved130 = vector110;
    saved140 = force120;
    limit200 = 0.0f;
    value204 = 0.0f;
    flag15C = 0;
    value208 = 1.0f;
    flag150 = 1;
    flag154 = 1;
    flag158 = 1;
    value160 = bodyGravity;
    RefreshParameters();
    nfs_strcpy(name0C, "CRigid");
}

// Original 0x59b940: mass reciprocal and the two inverse inertia matrices.
void MoverBody::RefreshParameters() {
    float mass = spec14->mass0;
    divisor164 = mass;
    value168 = 1.0f / mass;
    WheelInvert(&matrix170, &spec14->inertia10);
    WheelMatrix scaled;
    BodyMatrixCopy(&scaled, &spec14->inertia10);
    scaled.elements[10] = scaled.elements[10] * spec14->inertiaScale50;
    WheelInvert(&matrix1B0, &scaled);
}

// Original 0x59b9b0: place the body and derive momenta from supplied velocities.
void MoverBody::SetPose(WheelVector *position, WheelMatrix *matrix, WheelVector *velocity,
                        WheelVector *angular, WheelMatrix *inertiaOutput) {
    position20 = *position;
    BodyMatrixCopy(&matrix30, matrix);
    float mass = divisor164;
    vector70.x = mass * velocity->x;
    vector70.y = velocity->y * mass;
    vector70.z = velocity->z * mass;
    WheelMatrix inertia, transpose, rotated, world;
    WheelIdentity(&inertia);
    inertia.elements[0] = 1.0f / matrix170.elements[0];
    inertia.elements[5] = 1.0f / matrix170.elements[5];
    inertia.elements[10] = 1.0f / matrix170.elements[10];
    WheelTranspose(&transpose, &matrix30);
    WheelMultiply(&rotated, &matrix30, &inertia);
    WheelMultiply(&world, &rotated, &transpose);
    WheelTransform(&vector80, &world, angular);
    if (inertiaOutput) {
        BodyMatrixCopy(inertiaOutput, &world);
    }
    vectorC0 = *angular;
    vectorA0 = *velocity;
}

// Original 0x59b8b0: move the centre of mass without moving the body's frame.
void MoverBody::MoveCentre(WheelVector *centre) {
    WheelVector next, previous;
    WheelTransform(&next, &matrix30, centre);
    WheelTransform(&previous, &matrix30, &centre1F0);
    centre1F0 = *centre;
    position20.x = (next.x - previous.x) + position20.x;
    position20.y = (next.y - previous.y) + position20.y;
    position20.z = (next.z - previous.z) + position20.z;
}

// Original 0x59bc60: add capped linear and angular momentum changes.
void MoverBody::AddLimitedImpulse(WheelVector *impulse, WheelVector *point) {
    volatile float x = impulse->x, y = impulse->y, z = impulse->z;
    double length = sqrt(y * y + z * z + x * x);
    if (length > bodyImpulseLimit) {
        double scale = bodyImpulseLimit / length;
        x = x * scale;
        y = y * scale;
        z = z * scale;
    }
    vector70.x = x + vector70.x;
    vector70.y = y + vector70.y;
    vector70.z = z + vector70.z;
    WheelVector linear = {x, y, z};
    WheelVector angular;
    WheelCross(&angular, point, &linear);
    double ax = angular.x, ay = angular.y, az = angular.z;
    length = sqrt(ax * ax + ay * ay + az * az);
    if (length > bodyAngularImpulseLimit) {
        double scale = bodyAngularImpulseLimit / length;
        ax = scale * ax;
        ay = ay * scale;
        az = scale * az;
    }
    vector80.x = (float)(ax + vector80.x);
    vector80.y = (float)(ay + vector80.y);
    vector80.z = (float)(az + vector80.z);
}

// Original 0x59bd80: like 0x59bc60, with the angular change scaled after its cap.
void MoverBody::AddScaledImpulse(WheelVector *impulse, WheelVector *point, float angularScale) {
    volatile float x = impulse->x, y = impulse->y, z = impulse->z;
    double length = sqrt(z * z + y * y + x * x);
    if (length > bodyImpulseLimit) {
        double scale = bodyImpulseLimit / length;
        x = x * scale;
        y = y * scale;
        z = z * scale;
    }
    vector70.x = x + vector70.x;
    vector70.y = y + vector70.y;
    vector70.z = z + vector70.z;
    WheelVector linear = {x, y, z};
    WheelVector angular;
    WheelCross(&angular, point, &linear);
    volatile float ax = angular.x, ay = angular.y, az = angular.z;
    length = sqrt(ay * ay + az * az + ax * ax);
    if (length > bodyAngularImpulseLimit) {
        double scale = bodyAngularImpulseLimit / length;
        ax = scale * ax;
        ay = ay * scale;
        az = az * scale;
    }
    vector80.x = ax * angularScale + vector80.x;
    vector80.y = ay * angularScale + vector80.y;
    vector80.z = az * angularScale + vector80.z;
}
