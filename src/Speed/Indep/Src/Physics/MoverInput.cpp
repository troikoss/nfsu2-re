#include "VehicleMover.h"

// Original 0x5eb8f0/0x5eb900/0x5eb910: direct control setters.
void MoverInput::SetBrake(float value) { brake214 = value; }
void MoverInput::SetHandbrake(float value) { handbrake210 = value; }
void MoverInput::SetThrottle(float value) { throttle20C = value; }
