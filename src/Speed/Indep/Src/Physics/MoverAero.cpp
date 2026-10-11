#include "VehicleMover.h"
#include "GameModeFlags.h"

extern "C" double __cdecl sqrt(double);
#pragma intrinsic(sqrt)

extern float aeroDragModeScale;
extern float aeroGroundedPointZ;
extern float aeroMinimumSpeed;
extern float aeroScale7A1E4C;
extern float aeroRearShare;
extern WheelVector aeroFrontPoint;
extern WheelVector aeroRearPoint;

// Inline constructor body from original 0x5b5cc0 (pool 0x86b2f4).
void MoverAero::Construct(MoverAeroSpec *spec, MoverBody *body) {
    spec0 = spec;
    body4 = body;
    scale8 = 1.0f;
    valueC = 1.0f;
    control10 = 0.0f;
    grounded14 = 0.0f;
    value30 = 1.0f;
}

// Original 0x59e620: velocity drag and front/rear downforce applied to the body.
void MoverAero::Apply(float *output) {
    MoverBody *body = body4;
    float vx = body->vectorA0.x, vy = body->vectorA0.y;
    volatile float vz = body->vectorA0.z;
    volatile float speed = (float)sqrt(vx * vx + vy * vy + vz * vz) / scale8;
    float drag = -(value34 * valueC * spec0->coefficient0 * speed
                   * ((aeroDragModeScale - 1.0f) * (1.0f - control10) + 1.0f) * value30);
    WheelVector point = {0.0f, 0.0f, 0.0f};
    force20.x = drag * body->vectorA0.x;
    force20.y = body->vectorA0.y * drag;
    force20.z = vz * drag;
    if (!(grounded14 < 0.5f)) {
        point.z = (1.0f - control10) * aeroGroundedPointZ;
    }
    body4->ApplyForceAtPoint(&force20, &point);
    body = body4;
    float pitch = body->matrix30.elements[10];
    if (!(pitch >= 0.0f)) {
        pitch = 0.0f;
    }
    if (!(grounded14 < 0.5f)) {
        pitch = 1.0f;
    }
    float dx = 1.0f, dy = 0.0f, dz = 0.0f;
    if (speed > aeroMinimumSpeed) {
        volatile float inverse = 1.0f / speed;
        dx = inverse * body->vectorA0.x;
        dy = body->vectorA0.y * inverse;
        dz = body->vectorA0.z * inverse;
    }
    float forward = dz * body->matrix30.elements[2] + dy * body->matrix30.elements[1]
                  + dx * body->matrix30.elements[0];
    if (!(forward >= 0.0f)) {
        forward = 0.0f;
    }
    float factor;
    if (IsModeGroup3()) {
        factor = forward * forward * forward * forward;
    } else {
        factor = forward * forward;
        if (!(aeroScale7A1E4C <= factor)) {
            factor = aeroScale7A1E4C;
        }
    }
    volatile float front, rear;
    if (grounded14 == 0.0f) {
        float total = pitch * ((spec0->coefficient8 + spec0->coefficient4) * factor) * speed;
        front = aeroScale7A1E4C * total;
        rear = total * aeroRearShare;
    } else {
        front = factor * spec0->coefficient4 * pitch * speed;
        rear = factor * spec0->coefficient8 * pitch * speed;
    }
    WheelVector frontForce = {0.0f, 0.0f, -(front * value30)};
    WheelVector rearForce = {0.0f, 0.0f, -(rear * value30)};
    WheelVector force, at;
    WheelTransform(&force, &body->matrix30, &frontForce);
    WheelTransform(&at, &body->matrix30, &aeroFrontPoint);
    body->ApplyForceAtPoint(&force, &at);
    body = body4;
    WheelTransform(&at, &body->matrix30, &rearForce);
    WheelTransform(&force, &body->matrix30, &aeroRearPoint);
    body->ApplyForceAtPoint(&at, &force);
    if (output) {
        *output = rear + front;
    }
}
