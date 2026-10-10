#include "WheelAngle.h"

// Original PC v1.2 0x43c570. Do not substitute atan2 or continuous angles.
unsigned short WheelAngle(float x, float y) {
    int quadrant = 0;
    if (x < 0.0f) {
        quadrant = 1;
        x = -x;
    }
    if (y < 0.0f) {
        quadrant ^= 3;
        y = -y;
    }
    int angle;
    if (x > y) {
        int index = (int)(y / x * wheelAngleRatioScale);
        angle = wheelAngleTable[-index] << 5;
    } else if (y > x) {
        int index = (int)(x / y * wheelAngleRatioScale);
        angle = 0x4000 - (wheelAngleTable[-index] << 5);
    } else {
        angle = y == 0.0f ? 0 : 0x2000;
    }
    if (quadrant == 0) {
        return (unsigned short)angle;
    }
    if (quadrant == 3) {
        return (unsigned short)-angle;
    }
    if (quadrant == 1) {
        return (unsigned short)(0x8000 - angle);
    }
    return (unsigned short)(angle + 0x8000);
}
