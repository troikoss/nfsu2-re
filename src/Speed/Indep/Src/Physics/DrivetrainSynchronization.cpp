#include "Drivetrain.h"

// Original PC v1.2 0x5a12d0. Physical units remain unresolved.
void Drivetrain::SynchronizeWheelValues(float input) {
    float difference = input - GetWheelDerivedValue();
    float scaled = unidentified34->value3F0 * drivetrainSynchronizationScale;
    int average = scaled < drivetrainSynchronizationThreshold;
    if (spec->selector10 >= 0.001f) {
        if (!wheels[2]->flag11C && !wheels[3]->flag11C) {
            average = 1;
        }
        float increment = difference / (spec->ratios[ratioIndex] * spec->coefficientC);
        float left = wheels[2]->value28;
        float right = wheels[3]->value28;
        if (average) {
            right = (right + left) * 0.5f;
            left = right;
        }
        wheels[2]->value28 = left + increment;
        wheels[3]->value28 = right + increment;
    }
    average = scaled < drivetrainSynchronizationThreshold;
    if (spec->selector10 <= 0.999f) {
        if (!wheels[0]->flag11C && !wheels[1]->flag11C) {
            average = 1;
        }
        float increment = difference / (spec->ratios[ratioIndex] * spec->coefficient8);
        float left = wheels[0]->value28;
        float right = wheels[1]->value28;
        if (average) {
            right = (right + left) * 0.5f;
            left = right;
        }
        wheels[0]->value28 = left + increment;
        wheels[1]->value28 = right + increment;
    }
}
