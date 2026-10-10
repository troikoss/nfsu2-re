#include "VehicleMover.h"

// Original PC v1.2 0x4011f0. Read Y/Z before output writes for alias safety.
void WheelScale(WheelVector *output, WheelVector *input, float scale) {
    float y = input->y;
    float z = input->z;
    output->x = scale * input->x;
    output->y = y * scale;
    output->z = z * scale;
}

// Original PC v1.2 0x59bc20. Snapshot Y/Z before modifying the accumulator.
void MoverBody::AddForce(WheelVector *input) {
    float y = input->y;
    float z = input->z;
    force120.x = force120.x + input->x;
    force120.y = y + force120.y;
    force120.z = z + force120.z;
}
