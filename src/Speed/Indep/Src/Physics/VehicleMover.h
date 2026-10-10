#ifndef UG2_VEHICLE_MOVER_H
#define UG2_VEHICLE_MOVER_H

#include "Drivetrain.h"

// Partial PC x86 layouts: only fields observed in 0x5abdf0 are identified.
struct MoverBody {
    char unidentified0[0x30];
    WheelMatrix matrix30;
    char unidentified70[0xa0];
    WheelVector vector110;
    char unidentified11C[0x3c];
    int flag158;
    char unidentified15C[8];
    float divisor164;
    char unidentified168[8];
    WheelMatrix matrix170;
    WheelMatrix matrix1B0;
};
struct MoverArm {
    char unidentified0[0x90];
    float value90;
    float value94;
};
struct MoverMode {
    char unidentified0[0x18];
    signed char value18;
};
struct MoverModeOwner {
    char unidentified0[0x3c];
    MoverMode *mode3C;
};
struct VehicleMover {
    char unidentified0[0x20];
    MoverBody *body20;
    char unidentified24[4];
    MoverArm *arms28[4];
    DrivetrainWheel *wheels38[4];
    char unidentified48[4];
    Drivetrain *drivetrain4C;
    char unidentified50[0xc];
    MoverModeOwner *owner5C;
    void CorrectWheelCoupling();
};

#endif
