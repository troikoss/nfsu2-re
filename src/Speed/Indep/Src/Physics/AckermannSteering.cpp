#include "VehicleMover.h"
#include "Engine.h"

extern float steeringAngleUnit;
extern float steeringPi;

// Inline constructor body from original 0x5b5cc0; vtable 0x7a2084 is installed by the caller.
void AckermannSteering::Construct(CarPhysicsRecord *record) {
    record4 = record;
    matrices8 = wheelMatrices10;
}

// Original 0x59def0, vtable slot 4: front wheel frames from wheelbase and track.
void AckermannSteering::SetAngle(unsigned short angle) {
    CarPhysicsRecord *record = record4;
    angle90 = angle;
    volatile float wheelbase = record->wheels10[0].position0.x - record->wheels10[3].position0.x;
    volatile float track = record->wheels10[0].position0.y - record->wheels10[1].position0.y;
    int positive = 1;
    float theta = (float)(int)angle * steeringAngleUnit;
    if (theta > steeringPi) {
        theta = theta - (steeringPi + steeringPi);
    }
    if (!(theta >= 0.0f)) {
        positive = 0;
        theta = -theta;
    }
    volatile float inner = (wheelbase * theta) / (theta * track + wheelbase);
    volatile float first, second;
    if (positive) {
        first = inner;
        second = theta;
    } else {
        first = -theta;
        second = -inner;
    }
    WheelMatrix *left = &wheelMatrices10[0];
    WheelIdentity(left);
    volatile float cosine = EngineSine(first + engineHalfPi);
    float sine = EngineSine(first);
    left->elements[0] = cosine;
    left->elements[4] = -sine;
    left->elements[1] = sine;
    left->elements[5] = cosine;
    WheelMatrix *right = &wheelMatrices10[1];
    WheelIdentity(right);
    cosine = EngineSine(second + engineHalfPi);
    sine = EngineSine(second);
    right->elements[4] = -sine;
    right->elements[0] = cosine;
    right->elements[1] = sine;
    right->elements[5] = cosine;
}

// Original 0x5a4760, vtable slot 8.
unsigned short AckermannSteering::GetAngle() {
    return angle90;
}
