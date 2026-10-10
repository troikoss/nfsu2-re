#include "Drivetrain.h"

extern "C" double __cdecl fabs(double);
#pragma intrinsic(fabs)

// Original PC v1.2 0x594a30. Preserve asymmetric pair weighting and neutral no-op.
void Drivetrain::DistributeDriveValue(float input) {
    if (ratioIndex == 1) {
        return;
    }
    value70 += input;
    float feedbackScale = unidentified34->value3E4 * 0.25f;
    if (feedbackScale < 0.25f) {
        feedbackScale = 0.25f;
    }
    component20->feedback7C -= feedbackScale * input;
    float ratioInput = input * spec->ratios[ratioIndex];
    float front = (1.0f - coefficientEC) * spec->coefficient8 * ratioInput;
    float rear = spec->coefficientC * coefficientEC * ratioInput;
    float threshold = wheels[0]->flag11C && wheels[1]->flag11C ? drivetrainPairThreshold : 0.5f;
    float weight;
    if (unidentified34->selector42C == 1) {
        weight = 1.0f - threshold;
    } else if (unidentified34->selector42C == 2) {
        weight = threshold;
    } else {
        float sum = wheels[0]->value28 + wheels[1]->value28;
        if (fabs(sum) > drivetrainSumEpsilon) {
            weight = wheels[1]->value28 / sum;
            if (weight < threshold) {
                weight = 1.0f - drivetrainPairThreshold * 0.5f;
            } else if (weight > 1.0f - threshold) {
                weight = drivetrainPairThreshold * 0.5f;
            }
        } else {
            weight = 0.5f;
        }
    }
    if (!wheels[0]->flag10C) {
        wheels[0]->value108 += (1.0f - weight) * front;
    }
    if (!wheels[1]->flag10C) {
        wheels[1]->value108 += weight * front;
    }
    threshold = wheels[2]->flag11C && wheels[3]->flag11C ? drivetrainPairThreshold : 0.5f;
    if (unidentified34->selector42C == 1) {
        weight = 1.0f - threshold;
    } else if (unidentified34->selector42C == 2) {
        weight = threshold;
    } else {
        float sum = wheels[3]->value28 + wheels[2]->value28;
        if (fabs(sum) > drivetrainSumEpsilon) {
            weight = wheels[2]->value28 / sum;
            if (weight < threshold) {
                weight = 1.0f - drivetrainPairThreshold * 0.5f;
            } else if (weight > 1.0f - threshold) {
                weight = drivetrainPairThreshold * 0.5f;
            }
        } else {
            weight = 0.5f;
        }
    }
    if (!wheels[2]->flag10C) {
        wheels[2]->value108 += weight * rear;
    }
    if (!wheels[3]->flag10C) {
        wheels[3]->value108 += (1.0f - weight) * rear;
    }
}
