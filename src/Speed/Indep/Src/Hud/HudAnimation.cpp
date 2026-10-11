#include "HudAnimation.h"
extern float hudHalf784260;

// Original 0x505150. The first component adds the baseline last; the others
// add it before the first key. Preserve that arithmetic order.
void HudLerpVector3(const float *a, const float *b, float fraction, const float *base, float *out) {
    out[0] = (b[0] - a[0]) * fraction + a[0] + base[0];
    out[1] = (b[1] - a[1]) * fraction + base[1] + a[1];
    out[2] = (b[2] - a[2]) * fraction + base[2] + a[2];
}

// Original 0x5051a0: integer colour tracks round the delta through the
// compiler's truncating conversion helper after adding 0.5, even for negatives.
void HudLerpColour(const int *a, const int *b, float fraction, const int *base, int *out) {
    out[0] = (int)((b[0] - a[0]) * fraction + hudHalf784260) + base[0] + a[0];
    out[1] = (int)((b[1] - a[1]) * fraction + hudHalf784260) + base[1] + a[1];
    out[2] = (int)((b[2] - a[2]) * fraction + hudHalf784260) + base[2] + a[2];
    out[3] = (int)((b[3] - a[3]) * fraction + hudHalf784260) + base[3] + a[3];
}
