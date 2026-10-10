#include "Drivetrain.h"

// Original virtual slots 0x04, 0x0c and 0x20 respectively.
int Drivetrain::GetValueCount() {
    return 1;
}

float *Drivetrain::GetValues() {
    return &state14;
}

int Drivetrain::BeforeIntegrate() {
    return 0;
}
