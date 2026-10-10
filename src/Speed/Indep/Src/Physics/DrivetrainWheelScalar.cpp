#include "Drivetrain.h"

// Original PC v1.2 0x5947c0. This averages +0x16c; its physical unit is unresolved.
float Drivetrain::GetAverageWheelScalar() {
    float result = 0.0f;
    int count = 0;
    if (spec->selector10 >= 0.001f) {
        result = wheels[3]->value16C + wheels[2]->value16C;
        count = 2;
    }
    if (spec->selector10 <= 0.999f) {
        result += wheels[1]->value16C + wheels[0]->value16C;
        count += 2;
    }
    return result / count;
}
