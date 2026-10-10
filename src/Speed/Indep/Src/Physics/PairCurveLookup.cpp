#include "DriveFeedbackCurve.h"

extern "C" double __cdecl fabs(double);
#pragma intrinsic(fabs)

// Original PC v1.2 0x576790. Unlike Evaluate, this returns the first value
// even for count <= 0 or an unmatched interval; the pointer must remain valid.
float DriveFeedbackCurve::Lookup(float input) {
    if (count > 1) {
        if (input <= pairs[0].coordinate) {
            return pairs[0].value;
        }
        if (input >= pairs[count - 1].coordinate) {
            return pairs[count - 1].value;
        }
        for (int i = 0; i < count - 1; ++i) {
            if (input >= pairs[i].coordinate && input < pairs[i + 1].coordinate) {
                double difference = (double)pairs[i + 1].value - pairs[i].value;
                double width = (double)pairs[i + 1].coordinate - pairs[i].coordinate;
                if (fabs(width) > pairCurveEpsilon) {
                    return (input - (double)pairs[i].coordinate) / width * difference + pairs[i].value;
                }
                return difference * 0.5f + pairs[i].value;
            }
        }
    }
    return pairs[0].value;
}
