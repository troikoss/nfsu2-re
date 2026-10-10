#include "VehicleMover.h"

// Original PC v1.2 0x59c120. Preserve original component write order.
void BodyMatrixDerivative(WheelMatrix *output, WheelVector *input, WheelMatrix *matrix) {
    output->elements[0] = matrix->elements[2] * input->y - input->z * matrix->elements[1];
    output->elements[4] = matrix->elements[6] * input->y - input->z * matrix->elements[5];
    output->elements[12] = 0.0f;
    output->elements[8] = matrix->elements[10] * input->y - matrix->elements[9] * input->z;
    output->elements[1] = input->z * matrix->elements[0] - matrix->elements[2] * input->x;
    output->elements[5] = input->z * matrix->elements[4] - input->x * matrix->elements[6];
    output->elements[13] = 0.0f;
    output->elements[9] = input->z * matrix->elements[8] - matrix->elements[10] * input->x;
    output->elements[2] = input->x * matrix->elements[1] - matrix->elements[0] * input->y;
    output->elements[6] = input->x * matrix->elements[5] - matrix->elements[4] * input->y;
    float last = matrix->elements[9] * input->x - matrix->elements[8] * input->y;
    output->elements[14] = 0.0f;
    output->elements[3] = 0.0f;
    output->elements[7] = 0.0f;
    output->elements[11] = 0.0f;
    output->elements[15] = 1.0f;
    output->elements[10] = last;
}
