#include "Drivetrain.h"

// Original PC v1.2 0x5945f0. Do not substitute continuous drive blending.
float Drivetrain::GetWheelDerivedValue() {
    float result = 0.0f;
    DrivetrainSpec *data = spec;
    if (data->selector10 >= 0.001f) {
        result = (wheels[3]->value28 + wheels[2]->value28) * 0.5f * data->coefficientC;
    }
    if (data->selector10 <= 0.999f) {
        result += (wheels[1]->value28 + wheels[0]->value28) * 0.5f * data->coefficient8;
        if (data->selector10 >= 0.001f) {
            result *= 0.5f;
        }
    }
    return result * data->ratios[ratioIndex];
}
