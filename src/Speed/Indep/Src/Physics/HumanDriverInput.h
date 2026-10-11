#ifndef UG2_HUMAN_DRIVER_INPUT_H
#define UG2_HUMAN_DRIVER_INPUT_H

#include "VehicleMover.h"
#include "HumanSteering.h"

// Partial timer view used at human input +0x2cc. Original 0x577310.
struct DriverTimer {
    int value0;
    int Advance(float duration);
};
UG2_CHECK_SIZE(DriverTimer, 4);

// Human input vtable 0x784fc8, constructor 0x425fc0. Steering subobject
// +0x220 has a recovered constructor/rate helper; full input construction,
// device capture and steering update remain separate recovery work.
class HumanDriverInput : public MoverInput {
public:
    char unidentified218[8];
    HumanSteering steering220;
    MoverModeOwner *car2A8;
    int device2AC;
    float value2B0;
    unsigned char flag2B4;
    char unidentified2B5[3];
    int value2B8;
    float hold2BC;
    int transmission2C0;
    char unidentified2C4[8];
    DriverTimer timer2CC;
    char unidentified2D0[4];
    float throttle2D4;
    char unidentified2D8[4];
    float brake2DC;
    char unidentified2E0[12];
    float handbrake2EC;
    char unidentified2F0[0x24];

    int ReadGearRequest();
    int ReadDirectGear();
    void StoreGearRequest(int gear);
    void StoreDirectGear(int gear);
    void SelectForward();
    void UpdatePedals(float duration);
};

UG2_CHECK_OFFSET(HumanDriverInput, car2A8, 0x2a8);
UG2_CHECK_OFFSET(HumanDriverInput, steering220, 0x220);
UG2_CHECK_OFFSET(HumanDriverInput, transmission2C0, 0x2c0);
UG2_CHECK_OFFSET(HumanDriverInput, throttle2D4, 0x2d4);
UG2_CHECK_OFFSET(HumanDriverInput, brake2DC, 0x2dc);
UG2_CHECK_OFFSET(HumanDriverInput, handbrake2EC, 0x2ec);
UG2_CHECK_SIZE(HumanDriverInput, 0x314);

#endif
