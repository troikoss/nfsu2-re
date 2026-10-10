#include "Engine.h"

// Original PC v1.2 0x5a1290.
int Engine::WriteDerivative(float *output) {
    float result = ComputeDriveValue() + feedback7C;
    float divisor = spec->divisorC;
    int mode = drivetrain20->mode44;
    if (mode == 0 || mode == 2) {
        divisor *= engineModeDivisorScale;
    }
    *output = result / divisor;
    return 0;
}
