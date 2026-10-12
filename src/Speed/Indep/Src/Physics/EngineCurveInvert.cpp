#include "Engine.h"

extern "C" double __cdecl fabs(double);
#pragma intrinsic(fabs)
extern float pairCurveEpsilon;

// Original 0x576560: inverse lookup. A sample within 1e-6 of the value
// returns its knot position; otherwise the first interval whose end crosses
// the value (relative to sample 0) is interpolated. Without a crossing the
// last knot position is returned.
float EngineCurve::Invert(float value) {
    float *points = samples;
    if (fabs((double)value - points[0]) < pairCurveEpsilon) return points[0];
    int below = points[0] < value;
    int i = 1;
    for (; i < count; ++i) {
        if (fabs((double)value - points[i]) < pairCurveEpsilon) return i / inverseSpacing + start;
        int current = points[i] < value;
        if (current != below) {
            return ((value - points[i - 1]) / (points[i] - points[i - 1]) + (i - 1)) / inverseSpacing + start;
        }
        below = current;
    }
    return (count - 1) / inverseSpacing + start;
}
