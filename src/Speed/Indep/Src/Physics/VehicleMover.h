#ifndef UG2_VEHICLE_MOVER_H
#define UG2_VEHICLE_MOVER_H

#include "Drivetrain.h"

// Partial PC x86 layouts: fields observed in recovered mover/body routines.
struct MoverBody {
    char unidentified0[0x20];
    WheelVector position20;
    char unidentified2C[4];
    WheelMatrix matrix30;
    WheelVector vector70;
    char unidentified7C[4];
    WheelVector vector80;
    char unidentified8C[4];
    float value90;
    char unidentified94[0xc];
    WheelVector vectorA0;
    char unidentifiedAC[4];
    WheelVector vectorB0;
    char unidentifiedBC[4];
    WheelVector vectorC0;
    char unidentifiedCC[4];
    WheelMatrix matrixD0;
    WheelVector vector110;
    char unidentified11C[4];
    WheelVector force120;
    char unidentified12C[4];
    WheelVector saved130;
    char unidentified13C[4];
    WheelVector saved140;
    char unidentified14C[4];
    int flag150;
    int flag154;
    int flag158;
    int flag15C;
    float value160;
    float divisor164;
    float value168;
    char unidentified16C[4];
    WheelMatrix matrix170;
    WheelMatrix matrix1B0;
    char unidentified1F0[0x10];
    float limit200;
    float value204;
    float value208;
    void AddForce(WheelVector *input);
    void ApplyForceAtPoint(WheelVector *force, WheelVector *point);
    void PostUpdate();
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
extern float bodyPostSpeedLimit;
extern double bodyPostMassThreshold;
extern float bodyPostResponseScale;
extern float bodyPostAngularLimit;
extern float bodyPointForceLimit;
extern float bodyPointTorqueLimit;
void BodyMatrixDerivative(WheelMatrix *output, WheelVector *input, WheelMatrix *matrix);

#endif

