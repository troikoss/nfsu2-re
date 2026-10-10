#include "WheelMath.h"

// Original PC v1.2 0x4012b0. Snapshot each four-element group before writes.
// Forward partial overlaps retain the original behavior, not memmove semantics.
void BodyMatrixCopy(WheelMatrix *output, WheelMatrix *input) {
    for (int row = 0; row < 4; ++row) {
        int base = row * 4;
        float a = input->elements[base];
        float b = input->elements[base + 1];
        float c = input->elements[base + 2];
        float d = input->elements[base + 3];
        output->elements[base + 1] = b;
        output->elements[base] = a;
        if (row == 3) {
            output->elements[base + 3] = d;
            output->elements[base + 2] = c;
        } else {
            output->elements[base + 2] = c;
            output->elements[base + 3] = d;
        }
    }
}

// Original 0x4013f0. Preserve each component's observed addition order.
// All products are captured before the separate copying call.
void BodyMatrixProduct(WheelMatrix *output, WheelMatrix *left, WheelMatrix *right) {
    WheelMatrix result;
    result.elements[0] = left->elements[0] * right->elements[0] + left->elements[3] * right->elements[12] + left->elements[2] * right->elements[8] + left->elements[1] * right->elements[4];
    result.elements[1] = left->elements[3] * right->elements[13] + left->elements[1] * right->elements[5] + left->elements[2] * right->elements[9] + left->elements[0] * right->elements[1];
    result.elements[2] = left->elements[2] * right->elements[10] + left->elements[0] * right->elements[2] + left->elements[3] * right->elements[14] + left->elements[1] * right->elements[6];
    result.elements[3] = left->elements[3] * right->elements[15] + left->elements[2] * right->elements[11] + left->elements[0] * right->elements[3] + left->elements[1] * right->elements[7];
    result.elements[4] = left->elements[6] * right->elements[8] + left->elements[7] * right->elements[12] + left->elements[5] * right->elements[4] + left->elements[4] * right->elements[0];
    result.elements[5] = left->elements[6] * right->elements[9] + left->elements[4] * right->elements[1] + left->elements[5] * right->elements[5] + left->elements[7] * right->elements[13];
    result.elements[6] = left->elements[7] * right->elements[14] + left->elements[6] * right->elements[10] + left->elements[5] * right->elements[6] + left->elements[4] * right->elements[2];
    result.elements[7] = left->elements[7] * right->elements[15] + left->elements[6] * right->elements[11] + left->elements[4] * right->elements[3] + left->elements[5] * right->elements[7];
    result.elements[8] = left->elements[10] * right->elements[8] + left->elements[11] * right->elements[12] + left->elements[9] * right->elements[4] + left->elements[8] * right->elements[0];
    result.elements[9] = left->elements[10] * right->elements[9] + left->elements[8] * right->elements[1] + left->elements[9] * right->elements[5] + left->elements[11] * right->elements[13];
    result.elements[10] = left->elements[11] * right->elements[14] + left->elements[10] * right->elements[10] + left->elements[9] * right->elements[6] + left->elements[8] * right->elements[2];
    result.elements[11] = left->elements[11] * right->elements[15] + left->elements[10] * right->elements[11] + left->elements[8] * right->elements[3] + left->elements[9] * right->elements[7];
    result.elements[12] = left->elements[14] * right->elements[8] + left->elements[15] * right->elements[12] + left->elements[13] * right->elements[4] + left->elements[12] * right->elements[0];
    result.elements[13] = left->elements[14] * right->elements[9] + left->elements[12] * right->elements[1] + left->elements[13] * right->elements[5] + left->elements[15] * right->elements[13];
    result.elements[14] = left->elements[15] * right->elements[14] + left->elements[14] * right->elements[10] + left->elements[13] * right->elements[6] + left->elements[12] * right->elements[2];
    result.elements[15] = left->elements[15] * right->elements[15] + left->elements[14] * right->elements[11] + left->elements[12] * right->elements[3] + left->elements[13] * right->elements[7];
    BodyMatrixCopy(output, &result);
}

// Original 0x43c990 wrapper swaps the product operands.
void WheelMultiply(WheelMatrix *output, WheelMatrix *left, WheelMatrix *right) {
    BodyMatrixProduct(output, right, left);
}
