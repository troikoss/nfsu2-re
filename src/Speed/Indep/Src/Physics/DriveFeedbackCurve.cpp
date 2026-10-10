#include "DriveFeedbackCurve.h"

// Original PC v1.2 0x576740. Preserve weighted-sum arithmetic and call boundary.
void __stdcall InterpolateDriveFeedback(float *output, float *next,
                                        float *previous, float fraction) {
    *output = (1.0f - fraction) * *previous + fraction * *next;
}

// Original PC v1.2 0x40c840. Empty/unmatched tables leave output unchanged.
void DriveFeedbackCurve::Evaluate(float *output, float input) {
    if (count > 1) {
        if (input <= pairs[0].coordinate) {
            *output = pairs[0].value;
            return;
        }
        if (input >= pairs[count - 1].coordinate) {
            *output = pairs[count - 1].value;
            return;
        }
        for (int i = 0; i < count - 1; ++i) {
            if (input >= pairs[i].coordinate && input < pairs[i + 1].coordinate) {
                float fraction = (input - pairs[i].coordinate) /
                                 (pairs[i + 1].coordinate - pairs[i].coordinate);
                InterpolateDriveFeedback(output, &pairs[i + 1].value,
                                         &pairs[i].value, fraction);
                return;
            }
        }
    } else if (count > 0) {
        *output = pairs[0].value;
    }
}
