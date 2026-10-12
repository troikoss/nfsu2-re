#ifndef UG2_HUMAN_STEERING_H
#define UG2_HUMAN_STEERING_H

#include "VehicleMover.h"
#include "../../Libs/bWare/TimedAverage.h"

// Original human-input +0x220 subobject. Layout recovered from 0x405c60 and
// its consumers; unidentified flags/parameters keep their original offsets.
struct HumanSteering {
    MoverModeOwner *car0;
    unsigned char flag4;
    char unidentified5[3];
    float value8;
    float valueC;
    unsigned char flag10;
    char unidentified11[3];
    TimedAverage inputHistory14;
    TimedAverage rateHistory48;
    unsigned short angle7C;
    char unidentified7E[2];
    float value80;
    float previous84;

    HumanSteering *Construct(MoverModeOwner *car); // 0x405c60
    void Destroy(); // 0x405d00
    float CalculateRate(float value, float duration, int device); // 0x4060e0
    float CalculateMaximum(float input, float duration, int device); // 0x405dc0
    float UpdateKeyboard(float input, float duration, int device); // 0x416bb0
    float UpdateAnalog(float input, float duration, int device); // 0x4061c0
    float UpdateAlternate(float input, float duration, int device); // 0x406a40
};
bool IsAnalogInputDevice(int device); // 0x5c1000

UG2_CHECK_SIZE(HumanSteering, 0x88);
UG2_CHECK_OFFSET(HumanSteering, inputHistory14, 0x14);
UG2_CHECK_OFFSET(HumanSteering, rateHistory48, 0x48);
UG2_CHECK_OFFSET(HumanSteering, previous84, 0x84);

#endif
