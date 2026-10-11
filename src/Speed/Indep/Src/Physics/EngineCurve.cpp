#include "Engine.h"

// Original PC v1.2 0x5764d0. Includes original endpoint and knot handling.
// __ftol2 truncates the unspilled position; later tests use its binary32 copy.
float EngineCurve::Lookup(float input) {
    double unspilled = ((double)input - start) * inverseSpacing;
    float position = (float)unspilled;
    int index = (int)unspilled;
    if (index < 0 || position < 0.0f) {
        return samples[0];
    }
    if (index >= count - 1) {
        return samples[count - 1];
    }
    float integral = (float)index;
    if (integral > position) {
        integral -= 1.0f;
    }
    float fraction = position - integral;
    return (1.0f - fraction) * samples[index] + fraction * samples[index + 1];
}
