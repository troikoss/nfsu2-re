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

void BodyMatrixCopy(WheelMatrix *output, WheelMatrix *input);
void BodyMatrixProduct(WheelMatrix *output, WheelMatrix *left, WheelMatrix *right);
void WheelMultiply(WheelMatrix *output, WheelMatrix *left, WheelMatrix *right);
void WheelInvert(WheelMatrix *output, WheelMatrix *input);
void WheelOrthonormalize(WheelMatrix *output, WheelMatrix *input);

#endif
