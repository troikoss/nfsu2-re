#ifndef UG2_VEHICLE_MOVER_H
#define UG2_VEHICLE_MOVER_H

#include "Drivetrain.h"

// Partial PC x86 layouts: fields observed in recovered mover/body routines.
struct MoverBody {
    char unidentified0[0x30];
    WheelMatrix matrix30;
    WheelVector vector70;
    char unidentified7C[4];
    WheelVector vector80;
    char unidentified8C[4];
    float value90;
    char unidentified94[0xc];
    WheelVector vectorA0;
    char unidentifiedAC[0x14];
    WheelVector vectorC0;
    char unidentifiedCC[0x44];
    WheelVector vector110;
    char unidentified11C[4];
    WheelVector force120;
    char unidentified12C[0x24];
    int flag150;
    int flag154;
    int flag158;
    int flag15C;
    char unidentified160[4];
    float divisor164;
    char unidentified168[8];
    WheelMatrix matrix170;
    WheelMatrix matrix1B0;
    char unidentified1F0[0x10];
    float limit200;
    float value204;
    float value208;
    void AddForce(WheelVector *input);
    float *WriteDerivative(float *output);
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
    char unidentified0[0x38];
    int state38;
    MoverMode *mode3C;
    char unidentified40[0x3e4];
    int mode424;
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
    void ApplyOpposingBodyForce();
};

extern float moverOpposingForceThreshold;

extern float bodyThresholdBase;
extern float bodyFlagThresholdScale;
extern float bodyPositiveTiltLimit;
extern float bodyPositiveTiltFactor;
extern float bodyNegativeTiltLimit;
extern float bodyNegativeTiltFactor;
extern float bodyHorizontalScale;
extern float bodyTiltResponseScale;
extern float bodyTiltResponseMaximum;
extern float bodyAScale;
extern float bodyCScale;
extern float bodyScalarRise;
extern float bodyScalarFall;
extern float bodyAngularDamping;
extern float bodyLinearDamping;
void BodyMatrixDerivative(WheelMatrix *output, WheelVector *input, WheelMatrix *matrix);

#endif
