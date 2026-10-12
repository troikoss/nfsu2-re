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
    unsigned char action2C4[2];
    char unidentified2C6[2];
    float steering2C8;
    DriverTimer timer2CC;
    unsigned char action2D0[3];
    char unidentified2D3;
    float throttle2D4;
    unsigned char action2D8[3];
    char unidentified2DB;
    float brake2DC;
    unsigned char action2E0[2];
    char unidentified2E2[2];
    float value2E4;
    unsigned char action2E8[2];
    char unidentified2EA[2];
    float handbrake2EC;
    unsigned char flag2F0;
    char unidentified2F1[3];
    float clutch2F4;
    unsigned char steeringEnabled2F8;
    unsigned char wheelControl2F9;
    unsigned char wheelRequest2FA;
    unsigned char wheelMode2FB;
    signed char wheelState2FC;
    char unidentified2FD[3];
    int wheelStamp300;
    float wheelInput304;
    float wheelInput308;
    float wheelInput30C;
    float wheelInput310;

    int ReadGearRequest();
    int ReadDirectGear();
    void StoreGearRequest(int gear);
    void StoreDirectGear(int gear);
    void SelectForward();
    void UpdatePedals(float duration);
    void Update(float duration); // 0x41e2c0
    void ClearActions(); // 0x4076c0
    bool CanUseWheelControl(bool second); // 0x407370
    void UpdateWheelControl(float duration); // 0x407400
};

int IsInputDeviceAssigned(int device); // 0x571a60
void HumanInputHook4022C0(MoverModeOwner *car); // 0x4022c0, empty

UG2_CHECK_OFFSET(HumanDriverInput, car2A8, 0x2a8);
UG2_CHECK_OFFSET(HumanDriverInput, steering220, 0x220);
UG2_CHECK_OFFSET(HumanDriverInput, transmission2C0, 0x2c0);
UG2_CHECK_OFFSET(HumanDriverInput, throttle2D4, 0x2d4);
UG2_CHECK_OFFSET(HumanDriverInput, brake2DC, 0x2dc);
UG2_CHECK_OFFSET(HumanDriverInput, handbrake2EC, 0x2ec);
UG2_CHECK_OFFSET(HumanDriverInput, steering2C8, 0x2c8);
UG2_CHECK_OFFSET(HumanDriverInput, clutch2F4, 0x2f4);
UG2_CHECK_OFFSET(HumanDriverInput, wheelStamp300, 0x300);
UG2_CHECK_SIZE(HumanDriverInput, 0x314);

#endif
