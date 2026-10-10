#include "Drivetrain.h"

// Original PC v1.2 0x5949b0. Source field meanings remain partly unresolved.
int Drivetrain::WriteDerivative(float *output) {
    if (status60 != 3) {
        float result = spec->coefficient1C * coefficientE8;
        if (adjustment68 > 0.0f) {
            result *= adjustment68 + 1.0f;
        } else if (adjustment68 < 0.0f && value38 >= 3) {
            result *= drivetrainNegativeScale;
        }
        *output = result * drivetrainModeCoefficients[mode44];
    } else {
        *output = 0.0f;
    }
    return 0;
}
