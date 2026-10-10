#include "WheelMath.h"

extern "C" double __cdecl sqrt(double);
#pragma intrinsic(sqrt)

// Original PC v1.2 0x43c780.
void WheelIdentity(WheelMatrix *output) {
    for (int i = 0; i < 16; ++i) {
        output->elements[i] = i % 5 == 0 ? 1.0f : 0.0f;
    }
}

// Original 0x43c9d0. Snapshot protects original in-place transpose behavior.
void WheelTranspose(WheelMatrix *output, WheelMatrix *input) {
    WheelMatrix snapshot = *input;
    for (int row = 0; row < 4; ++row) {
        for (int column = 0; column < 4; ++column) {
            output->elements[row * 4 + column] = snapshot.elements[column * 4 + row];
        }
    }
}

// Original 0x441000, called through the 0x43c9c0 jump thunk.
// Preserve its component-specific addition order and translation.
void WheelTransform(WheelVector *output, WheelMatrix *matrix, WheelVector *input) {
    float x = matrix->elements[8] * input->z + matrix->elements[4] * input->y
            + matrix->elements[0] * input->x + matrix->elements[12];
    float y = matrix->elements[9] * input->z + matrix->elements[1] * input->x
            + matrix->elements[5] * input->y + matrix->elements[13];
    float z = matrix->elements[10] * input->z + matrix->elements[2] * input->x
            + matrix->elements[6] * input->y + matrix->elements[14];
    output->x = x;
    output->y = y;
    output->z = z;
}

// Original 0x43d0c0.
void WheelScaleAdd(WheelVector *output, WheelVector *base, WheelVector *direction, float scale) {
    float y = scale * direction->y + base->y;
    float z = scale * direction->z + base->z;
    float x = scale * direction->x + base->x;
    output->x = x;
    output->y = y;
    output->z = z;
}

// Original 0x43cf10. A zero vector becomes (1, 0, 0).
void WheelNormalize(WheelVector *output, WheelVector *input) {
    float length = (float)sqrt(input->x * input->x + input->y * input->y + input->z * input->z);
    if (length != 0.0f) {
        float inverse = 1.0f / length;
        float z = input->z;
        float y = input->y;
        output->x = inverse * input->x;
        output->y = inverse * y;
        output->z = inverse * z;
    } else {
        output->x = 1.0f;
        output->y = 0.0f;
        output->z = 0.0f;
    }
}

// Original 0x43d1e0. Capture all components before writing aliases.
void WheelCross(WheelVector *output, WheelVector *a, WheelVector *b) {
    float y = b->x * a->z - b->z * a->x;
    float z = b->y * a->x - a->y * b->x;
    float x = b->z * a->y - b->y * a->z;
    output->x = x;
    output->y = y;
    output->z = z;
}
