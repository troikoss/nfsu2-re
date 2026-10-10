#include "VehicleMover.h"

extern "C" double __cdecl sqrt(double);
#pragma intrinsic(sqrt)

// Original PC v1.2 0x59bb00. Force and torque have separate magnitude caps.
void MoverBody::ApplyForceAtPoint(WheelVector *force, WheelVector *point) {
    double x = force->x;
    double y = force->y;
    volatile float z = force->z;
    double length = sqrt(force->z * force->z + force->y * force->y + force->x * force->x);
    if (length > bodyPointForceLimit) {
        double scale = bodyPointForceLimit / length;
        x *= scale;
        y *= scale;
        z = (float)(z * scale);
    }
    vector110.x = (float)(x + vector110.x);
    vector110.y = (float)(y + vector110.y);
    vector110.z = z + vector110.z;

    // The executable rereads the original force after accumulation. Do not
    // replace it with the capped vector; aliases also observe the earlier writes.
    WheelVector torque;
    WheelCross(&torque, point, force);
    x = torque.x;
    y = torque.y;
    double torqueZ = torque.z;
    length = sqrt(torque.x * torque.x + torque.y * torque.y + torque.z * torque.z);
    if (length > bodyPointTorqueLimit) {
        double scale = bodyPointTorqueLimit / length;
        x *= scale;
        y *= scale;
        torqueZ *= scale;
    }
    force120.x = (float)(x + force120.x);
    force120.y = (float)(y + force120.y);
    force120.z = (float)(torqueZ + force120.z);
}
