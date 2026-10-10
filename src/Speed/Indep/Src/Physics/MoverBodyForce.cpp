#include "VehicleMover.h"
#include "DriveFeedbackCurve.h"

extern "C" double __cdecl sqrt(double);
#pragma intrinsic(sqrt)

// Original PC v1.2 0x5a5b60; called after wheel coupling by 0x5b0520.
void VehicleMover::ApplyOpposingBodyForce() {
    if (owner5C->state38 == 1 || owner5C->state38 == 2) {
        return;
    }
    float length = (float)sqrt(body20->vectorC0.y * body20->vectorC0.y
                            + body20->vectorC0.z * body20->vectorC0.z
                            + body20->vectorC0.x * body20->vectorC0.x);
    if (owner5C->mode424 >= 2 || !(length < moverOpposingForceThreshold)) {
        return;
    }
    float coefficient = body20->divisor164;
    float scale = -(moverOpposingForceCurve.Lookup(length) * coefficient);
    WheelVector force;
    WheelScale(&force, &body20->vectorC0, scale);
    body20->AddForce(&force);
}
