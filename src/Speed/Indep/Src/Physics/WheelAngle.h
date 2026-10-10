#ifndef UG2_WHEEL_ANGLE_H
#define UG2_WHEEL_ANGLE_H

// PC v1.2's byte lookup table at 0x7eacb0 is a separate original data input.
extern unsigned char wheelAngleTable[];
extern float wheelAngleRatioScale;
unsigned short WheelAngle(float x, float y);

#endif
