#ifndef UG2_WHEEL_MATH_H
#define UG2_WHEEL_MATH_H

struct WheelVector { float x, y, z; };
struct WheelMatrix { float elements[16]; };

void WheelIdentity(WheelMatrix *output);
void WheelTranspose(WheelMatrix *output, WheelMatrix *input);
void WheelTransform(WheelVector *output, WheelMatrix *matrix, WheelVector *input);
void WheelScaleAdd(WheelVector *output, WheelVector *base, WheelVector *direction, float scale);
void WheelNormalize(WheelVector *output, WheelVector *input);
void WheelCross(WheelVector *output, WheelVector *a, WheelVector *b);

void WheelScale(WheelVector *output, WheelVector *input, float scale);

#endif
