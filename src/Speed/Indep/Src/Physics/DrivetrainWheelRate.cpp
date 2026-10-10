#include "Drivetrain.h"

// Original PC v1.2 0x594840. Offset names retain unresolved wheel semantics.
float Drivetrain::GetWheelDerivedRate() {
    float result = 0.0f;
    DrivetrainSpec *data = spec;
    if (data->selector10 >= 0.001f) {
        result = ((wheels[3]->value108 - wheels[3]->spec14->value10 * wheels[3]->value124) / wheels[3]->divisor2C
                + (wheels[2]->value108 - wheels[2]->spec14->value10 * wheels[2]->value124) / wheels[2]->divisor2C)
                * 0.5f * data->coefficientC;
    }
    if (data->selector10 <= 0.999f) {
        result += ((wheels[1]->value108 - wheels[1]->spec14->value10 * wheels[1]->value124) / wheels[1]->divisor2C
                 + (wheels[0]->value108 - wheels[0]->spec14->value10 * wheels[0]->value124) / wheels[0]->divisor2C)
                 * 0.5f * data->coefficient8;
        if (data->selector10 >= 0.001f) {
            result *= 0.5f;
        }
    }
    return result * data->ratios[ratioIndex];
}
