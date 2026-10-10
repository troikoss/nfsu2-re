#ifndef UG2_DRIVETRAIN_H
#define UG2_DRIVETRAIN_H

#include "WheelMath.h"

// Partial original 32-bit layouts. Names describe observed use, not debug symbols.
struct DrivetrainSpec {
    char unidentified0[8];
    float coefficient8;
    float coefficientC;
    float selector10;
    float coefficient14;
    int unidentified18;
    float coefficient1C;
    float ratios[8];
};

struct DrivetrainWheelSpec {
    char unidentified0[0x10];
    float value10;
    char unidentified14[4];
    int value18;
};

struct WheelCorrectionSpec {
    float value0;
    float divisor4;
    float coefficient8;
    float coefficientC;
    float coefficient10;
};

struct WheelSurface {
    float coefficient;
    float coefficient4;
    char unidentified8[0x90];
};

struct WheelPreparationSpec {
    char unidentified0[4];
    float coefficient4;
    float coefficient8;
};

struct EngineCurve;
struct DrivetrainAuxiliary;

struct DrivetrainWheel {
    char unidentified0[0x14];
    DrivetrainWheelSpec *spec14;
    WheelCorrectionSpec *correctionSpec18;
    WheelPreparationSpec *preparationSpec1C;
    DrivetrainAuxiliary *auxiliary20;
    char unidentified24[4];
    float value28;
    float divisor2C;
    char unidentified30[0x40];
    WheelMatrix matrix70;
    char unidentifiedB0[0x40];
    WheelVector vectorF0;
    char unidentifiedFC[4];
    float value100;
    float value104;
    float value108;
    int flag10C;
    char unidentified110[4];
    int surface114;
    float value118;
    unsigned char flag11C;
    char unidentified11D[7];
    float value124;
    float value128;
    char unidentified12C[4];
    WheelVector vector130;
    char unidentified13C[0x14];
    WheelVector vector150;
    char unidentified15C[4];
    unsigned char flag160;
    char unidentified161[3];
    float value164;
    char unidentified168[4];
    float value16C;
    float value170;
    char unidentified174[4];
    float coefficient178;
    float coefficient17C;
    float coefficient180;
    float coefficient184;
    float coefficient188;
    char unidentified18C[4];
    float coefficient190;
    float coefficient194;

    float GetCorrectionScale();
    void CorrectForces(float unidentifiedArgument);
    void PrepareForces(float unidentifiedArgument, int mode);
    void UpdateLockState(float input);
    float EvaluateLateralForce(float input, float angle);
};

struct DrivetrainComponent {
    char unidentified0[0x14];
    float value14;
    float *values18;
    char unidentified1C[0x5c];
    float value78;
    float feedback7C;
    float control80;
};

struct DrivetrainContextEntry {
    char unidentified0[0x41];
    unsigned char flag41;
};

struct DrivetrainAuxiliary {
    char unidentified0[0x3e4];
    int value3E4;
    char unidentified3E8[4];
    float value3EC;
    float value3F0;
    char unidentified3F4[0x30];
    int mode424;
    char unidentified428[4];
    int selector42C;
};

struct DrivetrainContext {
    char unidentified0[0xa0];
    DrivetrainContextEntry *entryA0;
};

struct DrivetrainRandom {
    char unidentified0[0xf8];
    unsigned int stateF8;
    float GetScaledValue(float range);
};

struct Drivetrain {
    char unidentifiedBeforeState[0x14];
    float state14;
    DrivetrainSpec *spec;
    void *unidentified1C;
    DrivetrainComponent *component20;
    DrivetrainWheel *wheels[4];
    DrivetrainAuxiliary *unidentified34;
    int value38;
    int ratioIndex;
    int unidentified40;
    int mode44;
    int pending48;
    float wheelValue4C;
    char unidentified50[8];
    float previous58;
    float timer5C;
    int status60;
    float timer64;
    float adjustment68;
    float value6C;
    float value70;
    unsigned char flag74;
    char unidentified75[3];
    float variation78[3];
    char unidentified84[0x48];
    float rangesCC[6];
    float coefficientE4;
    float coefficientE8;
    float coefficientEC;
    int indexF0;

    float GetWheelDerivedValue();
    float GetWheelDerivedRate();
    float GetAverageWheelScalar();
    void DistributeDriveValue(float input);
    void ApplyInitialDriveValue();
    void ApplyDriveCorrection();
    void SynchronizeWheelValues(float input);
    void FinishDriveCoupling();
    int WriteDerivative(float *output);
    int GetValueCount();
    float *GetValues();
    int BeforeIntegrate();
    int IsBelowComponentLimit();
    int IsWithinComponentTolerance();
    void RefreshVariations();
    void UpdateAfterIntegration();
};

extern float drivetrainNegativeScale;
extern float drivetrainModeCoefficients[];
extern float drivetrainComponentThreshold;
extern float drivetrainComponentScale;
extern float drivetrainComponentEpsilon;
extern float drivetrainDifferenceTolerance;
extern float drivetrainValueScale;
extern float drivetrainControlScale;
extern float drivetrainClockScale;
extern float drivetrainTimerLimit;
extern float drivetrainAdjustmentDuration;
extern float drivetrainAdjustmentScale;
extern int drivetrainClock;
extern DrivetrainContext *drivetrainContext;
extern DrivetrainRandom *drivetrainRandom;
extern float drivetrainRandomScale;
extern float drivetrainPairThreshold;
extern float drivetrainSumEpsilon;
extern float drivetrainDriveDifferenceLimit;
extern float drivetrainCrossingThreshold;
extern float drivetrainCrossingScale;
extern float drivetrainSynchronizationScale;
extern float drivetrainSynchronizationThreshold;
extern float drivetrainFinishingInputThreshold;
extern float drivetrainFinishingWheelThreshold;
extern float drivetrainFinishingCoefficients[];
extern float wheelCorrectionOffset;
extern float wheelCorrectionRangeScale;
extern float wheelCorrectionMinimum;
extern float wheelPreparationThreshold;
extern float wheelPreparationSpeedScale;
extern float wheelPreparationCoefficientThreshold;
extern float wheelPreparationScale4;
extern float wheelPreparationScale8;
extern float wheelLockSumScale;
extern float wheelLockValueScale;
extern int wheelLateralMode;
extern EngineCurve wheelLateralFallbackCurve;
extern EngineCurve *wheelLateralCurves[];
extern EngineCurve *wheelLateralLastCurve;
extern float wheelLateralInputScale;
extern float wheelLateralOutputScale;
extern float wheelLateralFallbackScale;
extern float wheelLateralAngleScale;
extern float wheelLateralIndexScale;
extern WheelSurface wheelSurfaces[];
extern float wheelDriveForceScale;
extern float wheelForceLimitScale;
extern float wheelForceAngleScale;
extern float wheelForcePi;
extern int wheelForceBasisMode;
float GetDrivetrainRandom(float range, unsigned int *state);

#endif
