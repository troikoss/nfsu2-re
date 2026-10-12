#ifndef UG2_DRIVETRAIN_H
#define UG2_DRIVETRAIN_H

#include "WheelMath.h"
#include "Ug2Layout.h"

// Partial original 32-bit layouts. Names describe observed use, not debug symbols.
struct DrivetrainSpec {
    float shiftFraction0;
    char unidentified4[4];
    float coefficient8;
    float coefficientC;
    float selector10;
    float coefficient14;
    int unidentified18;
    float coefficient1C;
    float ratios[8];
};

struct DrivetrainWheelSpec {
    WheelVector position0;
    char unidentifiedC[4];
    float value10;
    float value14;
    int value18;
    char unidentified1C[0x14];
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
    float value0; // steering angle scale (0x405dc0 callers)
    float coefficient4;
    float coefficient8;
};

struct EngineCurve;
struct DrivetrainAuxiliary;

struct DrivetrainWheel {
    void *vtable0;
    char unidentified4[8];
    char name0C[8];
    DrivetrainWheelSpec *spec14;
    WheelCorrectionSpec *correctionSpec18;
    WheelPreparationSpec *preparationSpec1C;
    DrivetrainAuxiliary *auxiliary20;
    float angle24;
    float value28;
    float divisor2C;
    WheelMatrix matrix30;
    WheelMatrix matrix70;
    WheelVector basisB0;
    char unidentifiedBC[4];
    WheelVector basisC0;
    char unidentifiedCC[0x24];
    WheelVector vectorF0;
    char unidentifiedFC[4];
    float value100;
    float value104;
    float value108;
    int flag10C;
    int state110;
    int surface114;
    float value118;
    unsigned char flag11C;
    char unidentified11D[3];
    float value120;
    float value124;
    float value128;
    char unidentified12C[4];
    WheelVector vector130;
    char unidentified13C[4];
    WheelVector contactPoint140;
    char unidentified14C[4];
    WheelVector vector150;
    char unidentified15C[4];
    unsigned char flag160;
    char unidentified161;
    short value162;
    float value164;
    float value168;
    float value16C;
    float value170;
    float value174;
    float coefficient178;
    float coefficient17C;
    float coefficient180;
    float coefficient184;
    float coefficient188;
    float offset18C;
    float coefficient190;
    float coefficient194;
    int value198;

    void Construct(DrivetrainWheelSpec *spec, WheelCorrectionSpec *correction,
                   WheelPreparationSpec *preparation, DrivetrainAuxiliary *auxiliary);
    float QueryGround(WheelVector *normal, int *surface, WheelVector *point, WheelMatrix *matrix);
    float SetContact(int contact, float load);
    int WriteBlockDerivative(float *output);
    void UpdateAfterIntegration();
    void SetLocalMatrix(WheelMatrix *matrix);
    void SetWorldMatrix(WheelMatrix *matrix);
    void UpdateEffects(float duration, struct MoverModeOwner *owner);
    float GetCorrectionScale();
    void CorrectForces(float unidentifiedArgument);
    void PrepareForces(float unidentifiedArgument, int mode);
    void UpdateLockState(float input);
    float EvaluateLateralForce(float input, float angle);
    void GetBlendedBasis(WheelVector *output); // 0x59daf0
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

struct CarPhysicsRecord;

// Per-car track collider at car +0x5c4 (World/WCollision.h); state +4 points to it.
struct WCollider;

// Road surface records at 0x800ee0, stride 0x98.
struct RoadSurface {
    char unidentified0[0x14];
    float skidWidth14;
    float bumpFrequency18;
    float bumpAmplitude1C;
    char unidentified20[4];
    float coefficient24;
    float coefficient28;
    float spinDrag2C;
    float brakeScale30;
    char unidentified34[0x64];
};
extern RoadSurface roadSurfaces[];
extern float roadNoiseTable[16];
float RoadSurfaceBump(float *position, int surface);

struct MoverModeOwner;

// Skid-mark trail handle at wheel +0x198; methods 0x5e6e70 (add segment) and
// 0x5d3230 (end) belong to the skid-mark renderer.
class SkidMarkTrail {
public:
    void *trail;
    void AddSegment(MoverModeOwner *owner, WheelVector *centre, WheelVector *across, int surface,
                    float intensity);
    void End();
};

// Vehicle state shared with gameplay; it starts at owner +0x40 (see MoverModeOwner).
struct DrivetrainAuxiliary {
    CarPhysicsRecord *record0;
    WCollider *collider4;
    char unidentified8[8];
    WheelVector origin10;
    char unidentified1C[4];
    WheelVector position20;
    char unidentified2C[4];
    WheelVector velocity30;
    char unidentified3C[4];
    WheelVector value40;
    char unidentified4C[4];
    WheelMatrix matrix50;
    WheelMatrix matrix90;
    WheelVector angularD0;
    char unidentifiedDC[4];
    WheelVector valueE0;
    char unidentifiedEC[4];
    WheelMatrix wheelMatricesF0[4];
    WheelMatrix wheelMatrices1F0[4];
    float wheelSpin2F0[4];
    float wheelAngle300[4];
    float wheelLoad310[4];
    float wheelValue320[4];
    float accumulated330[4];
    float suspension340[4];
    unsigned char contacts350[4];
    int surfaces354[4];
    int maximumSurfaces364[4];
    float wheelPairs374[4][2];
    float wheelValue394[4];
    float values3A4[4];
    unsigned char renderFlags3B4[4];
    float rideOffsets3B8[4];
    unsigned char assisted3C8[4];
    unsigned short heading3CC;
    unsigned short steering3CE;
    char unidentified3D0[8];
    float averageWheel3D8;
    float slip3DC;
    float slip3E0;
    int value3E4;
    int value3E8;
    float value3EC;
    float value3F0;
    int gear3F4;
    float control3F8;
    float value3FC;
    float engineValue400;
    float value404;
    int value408;
    float value40C;
    char unidentified410[4];
    int timer414;
    int timer418;
    int timer41C;
    int timer420;
    int mode424;
    float value428;
    int selector42C;
    float value430;
    float value434;
    float value438;
    char unidentified43C[4];
    WheelMatrix matrix440;
    unsigned char flag480;
    float GetSlipAngle();
    float GetSteeringSlip(); // 0x59fa10
    void UpdateOrigin(WheelVector *centre);
    void UpdateWheelRenderMatrices(int wheel);
    void InitialiseNitrous();
    void Reset();
    int IsWheelDriven(int wheel);
    int GetNitrousIndex();
    float GetNitrousElapsed();
    float GetNitrousRemaining();
    void FinishNitrous();
};

struct DrivetrainContext {
    char unidentified0[0xa0];
    DrivetrainContextEntry *entryA0;

    // Original 0x578e90; dispatch implementation remains a platform seam.
    void ControlEvent(float value, const char *name);
};

struct DrivetrainRandom {
    char unidentified0[0xf8];
    unsigned int stateF8;
    float GetScaledValue(float range);
};

// Global drivetrain tuning rows at 0x7fc2a8, stride 0x144, chosen by drivetrain selector.
struct DrivetrainTuning {
    char unidentified0[0x10];
    float shiftScale10;
};

struct EngineSpec;
void ComputeShiftPoints(DrivetrainSpec *spec, EngineSpec *engine, float *upshift, float *downshift);

struct Drivetrain {
    void *vtable0;
    char unidentified4[8];
    char name0C[8];
    float state14;
    DrivetrainSpec *spec;
    DrivetrainTuning *unidentified1C;
    DrivetrainComponent *component20;
    DrivetrainWheel *wheels[4];
    DrivetrainAuxiliary *unidentified34;
    int value38;
    int ratioIndex;
    int value40;
    int mode44;
    int pending48;
    float wheelValue4C;
    float value50;
    int value54;
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
    float upshift84[9];
    float downshiftA8[9];
    float rangesCC[6];
    float coefficientE4;
    float coefficientE8;
    float coefficientEC;
    int indexF0;

    void Construct(DrivetrainSpec *spec, DrivetrainTuning *tuning, DrivetrainComponent *engine,
                   DrivetrainWheel **wheels, DrivetrainAuxiliary *auxiliary);
    int SelectGear(int request, float engineSpeed);
    void ApplyShiftAdjustment(float engineSpeed);
    float GetShiftPoint(int gear, char useAlternate);
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
    void SetClutchInput(float value); // 0x594910
};

UG2_CHECK_OFFSET(DrivetrainWheel, matrix30, 0x30);
UG2_CHECK_OFFSET(DrivetrainWheel, value108, 0x108);
UG2_CHECK_OFFSET(DrivetrainWheel, contactPoint140, 0x140);
UG2_CHECK_OFFSET(DrivetrainWheel, value162, 0x162);
UG2_CHECK_OFFSET(DrivetrainWheel, value174, 0x174);
UG2_CHECK_OFFSET(DrivetrainWheel, offset18C, 0x18c);
UG2_CHECK_OFFSET(DrivetrainWheel, value198, 0x198);
UG2_CHECK_OFFSET(DrivetrainAuxiliary, position20, 0x20);
UG2_CHECK_OFFSET(DrivetrainAuxiliary, matrix50, 0x50);
UG2_CHECK_OFFSET(RoadSurface, spinDrag2C, 0x2c);
UG2_CHECK_SIZE(RoadSurface, 0x98);
UG2_CHECK_OFFSET(DrivetrainAuxiliary, wheelSpin2F0, 0x2f0);
UG2_CHECK_OFFSET(DrivetrainAuxiliary, contacts350, 0x350);
UG2_CHECK_OFFSET(DrivetrainAuxiliary, wheelValue394, 0x394);
UG2_CHECK_OFFSET(DrivetrainAuxiliary, renderFlags3B4, 0x3b4);
UG2_CHECK_OFFSET(DrivetrainAuxiliary, heading3CC, 0x3cc);
UG2_CHECK_OFFSET(DrivetrainAuxiliary, value3E4, 0x3e4);
UG2_CHECK_OFFSET(DrivetrainAuxiliary, engineValue400, 0x400);
UG2_CHECK_OFFSET(DrivetrainAuxiliary, timer420, 0x420);
UG2_CHECK_OFFSET(DrivetrainAuxiliary, mode424, 0x424);
UG2_CHECK_OFFSET(DrivetrainAuxiliary, selector42C, 0x42c);
UG2_CHECK_OFFSET(DrivetrainAuxiliary, flag480, 0x480);

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
