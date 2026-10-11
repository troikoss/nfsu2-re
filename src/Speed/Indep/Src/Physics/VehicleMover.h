#ifndef UG2_VEHICLE_MOVER_H
#define UG2_VEHICLE_MOVER_H

#include "Collision/Collision.h"
#include "Drivetrain.h"
#include "Engine.h"
#include "PhysicsUpgrades.h"
#include "Ug2Layout.h"

struct Engine;
struct EngineSpec;
class VehicleIntegrator;

// Body record at car record +0x110: mass, inertia matrix and inertia Z scale.
struct MoverBodySpec {
    float mass0;
    float value4;
    float value8;
    float valueC;
    WheelMatrix inertia10;
    float inertiaScale50;
    char unidentified54[0xc];
};

// Suspension record at car record +0x170 + axle * 0x20.
struct SuspensionSpec {
    float progression0;
    float stiffness4;
    float damping8[2];
    float antiRoll10;
    float upper14;
    float lower18;
    float rest1C;
};

struct MoverAeroSpec {
    float coefficient0;
    float coefficient4;
    float coefficient8;
};

// Copied from CarTypeInfo +0x110 by 0x5ac4f0 (0x2b0 bytes, 0xac dwords).
struct CarPhysicsRecord {
    WheelVector centre0;
    char unidentifiedC[4];
    DrivetrainWheelSpec wheels10[4];
    struct CorrectionBlock {
        WheelCorrectionSpec spec;
        char unidentified14[0xc];
    } correctionD0[2];
    MoverBodySpec body110;
    SuspensionSpec suspension170[2];
    DrivetrainSpec drivetrain1B0;
    char engine1F0[0x70];
    MoverAeroSpec aero260;
    float value26C;
    WheelPreparationSpec preparation270;
    float brakeBias27C;
    float value280;
    float value284;
    float value288;
    float value28C;
    float value290;
    char unidentified294[0xc];
    float nitrousCapacity2A0;
    int nitrousBottles2A4;
    char unidentified2A8[8];
};

// Partial PC x86 layouts: fields observed in recovered mover/body routines.
struct MoverBody {
    void *vtable0;
    char unidentified4[8];
    char name0C[8];
    MoverBodySpec *spec14;
    char unidentified18[8];
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
    WheelVector centre1F0;
    char unidentified1FC[4];
    float limit200;
    float value204;
    float value208;
    void Construct(MoverBodySpec *spec);
    void RefreshParameters();
    void SetPose(WheelVector *position, WheelMatrix *matrix, WheelVector *velocity,
                 WheelVector *angular, WheelMatrix *inertiaOutput);
    void MoveCentre(WheelVector *centre);
    void AddLimitedImpulse(WheelVector *impulse, WheelVector *point);
    void AddScaledImpulse(WheelVector *impulse, WheelVector *point, float angularScale);
    void AddForce(WheelVector *input);
    void ApplyForceAtPoint(WheelVector *force, WheelVector *point);
    void PostUpdate();
    float *WriteDerivative(float *output);
};
UG2_CHECK_OFFSET(CarPhysicsRecord, correctionD0, 0xd0);
UG2_CHECK_OFFSET(CarPhysicsRecord, body110, 0x110);
UG2_CHECK_OFFSET(CarPhysicsRecord, suspension170, 0x170);
UG2_CHECK_OFFSET(CarPhysicsRecord, drivetrain1B0, 0x1b0);
UG2_CHECK_OFFSET(CarPhysicsRecord, engine1F0, 0x1f0);
UG2_CHECK_OFFSET(CarPhysicsRecord, aero260, 0x260);
UG2_CHECK_OFFSET(CarPhysicsRecord, preparation270, 0x270);
UG2_CHECK_OFFSET(CarPhysicsRecord, brakeBias27C, 0x27c);
UG2_CHECK_OFFSET(CarPhysicsRecord, value290, 0x290);
UG2_CHECK_SIZE(CarPhysicsRecord, 0x2b0);
UG2_CHECK_OFFSET(MoverBody, spec14, 0x14);
UG2_CHECK_OFFSET(MoverBody, matrix170, 0x170);
UG2_CHECK_OFFSET(MoverBody, centre1F0, 0x1f0);
UG2_CHECK_OFFSET(MoverBody, value208, 0x208);

struct AckermannSteering;

// Suspension block, vtable 0x7a210c ("Suspnsn").
struct MoverArm {
    void *vtable0;
    char unidentified4[8];
    char name0C[8];
    SuspensionSpec *spec14;
    MoverArm *pair18;
    float travel1C;
    float rate20;
    float force24;
    char unidentified28[8];
    WheelVector position30;
    char unidentified3C[4];
    WheelMatrix matrix40;
    int contact80;
    char unidentified84[4];
    float extra88;
    char unidentified8C[4];
    float value90;
    float value94;
    float value98;
    char unidentified9C[4];
    DrivetrainWheel *wheelA0;
    MoverBody *bodyA4;
    AckermannSteering *steeringA8;
    int indexAC;
    float valueB0;
    void Construct(int index, MoverBody *body, DrivetrainWheel *wheel,
                   AckermannSteering *steering, SuspensionSpec *spec);
    void SetMount(float extra, float offset);
    void WriteDerivative(float *output);
    void UpdateAfterIntegration();
    float ApplyBumpStop();
};
UG2_CHECK_OFFSET(MoverArm, contact80, 0x80);
UG2_CHECK_OFFSET(MoverArm, value90, 0x90);
UG2_CHECK_OFFSET(MoverArm, indexAC, 0xac);

// Front steering geometry, vtable 0x7a2084 ("VATwoWheelAckermanSteering").
struct AckermannSteering {
    void *vtable0;
    CarPhysicsRecord *record4;
    WheelMatrix *matrices8;
    char unidentifiedC[4];
    WheelMatrix wheelMatrices10[2];
    unsigned short angle90;
    void Construct(CarPhysicsRecord *record);
    void SetAngle(unsigned short angle);
    unsigned short GetAngle();
};
UG2_CHECK_OFFSET(AckermannSteering, wheelMatrices10, 0x10);
UG2_CHECK_OFFSET(AckermannSteering, angle90, 0x90);

// Aerodynamic/drag helper owned by the mover at +0x50.
struct MoverAero {
    // Created inline by the mover constructor from pool 0x86b2f4.
    MoverAeroSpec *spec0;
    MoverBody *body4;
    float scale8;
    float valueC;
    float control10;
    float grounded14;
    char unidentified18[8];
    WheelVector force20;
    float value2C;
    float value30;
    float value34;
    void Construct(MoverAeroSpec *spec, MoverBody *body);
    void Apply(float *output);
};

// Driver controls object at mover +4; only observed slots are named.
class MoverInput {
public:
    virtual void UnknownSlot00() = 0;
    virtual void UnknownSlot04() = 0;
    virtual void UnknownSlot08() = 0;
    virtual void UnknownSlot0C() = 0;
    virtual void UnknownSlot10() = 0;
    virtual void UnknownSlot14() = 0;
    virtual void UnknownSlot18() = 0;
    virtual void UnknownSlot1C() = 0;
    virtual int GetGearRequest() = 0;
    virtual int GetDirectGear() = 0;
    virtual void SetSelectedGear(int gear) = 0;
    virtual void SetCurrentGear(int gear) = 0;
    virtual void UnknownSlot30() = 0; virtual void UnknownSlot34() = 0; virtual void UnknownSlot38() = 0;
    virtual void UnknownSlot3C() = 0; virtual void UnknownSlot40() = 0; virtual void UnknownSlot44() = 0;
    virtual void UnknownSlot48() = 0; virtual void UnknownSlot4C() = 0; virtual void UnknownSlot50() = 0;
    virtual void UnknownSlot54() = 0; virtual void UnknownSlot58() = 0; virtual void UnknownSlot5C() = 0;
    virtual void UnknownSlot60() = 0; virtual void UnknownSlot64() = 0; virtual void UnknownSlot68() = 0;
    virtual void UnknownSlot6C() = 0; virtual void UnknownSlot70() = 0; virtual void UnknownSlot74() = 0;
    virtual void UnknownSlot78() = 0; virtual void UnknownSlot7C() = 0; virtual void UnknownSlot80() = 0;
    virtual unsigned short GetGuideHeading() = 0;
    char unidentified4[0x1dc];
    unsigned short steering1E0;
    char unidentified1E2[2];
    int gearRequest1E4;
    int directGear1E8;
    float wheelInputs1EC[4];
    float wheelTargets1FC[4];
    float throttle20C;
    float handbrake210;
    float brake214;
    void SetBrake(float value);
    void SetHandbrake(float value);
    void SetThrottle(float value);
};

UG2_CHECK_OFFSET(MoverAero, force20, 0x20);
UG2_CHECK_OFFSET(MoverAero, value34, 0x34);
UG2_CHECK_OFFSET(MoverInput, steering1E0, 0x1e0);
UG2_CHECK_OFFSET(MoverInput, gearRequest1E4, 0x1e4);
UG2_CHECK_OFFSET(MoverInput, directGear1E8, 0x1e8);
UG2_CHECK_OFFSET(MoverInput, wheelInputs1EC, 0x1ec);
UG2_CHECK_OFFSET(MoverInput, throttle20C, 0x20c);

struct MoverOwnerLink {
    int value0;
    int value4;
};

struct VehicleMover;

// Car object +0x1c. Only fields used by mover routines are named.
struct MoverOwnerObject {
    char unidentified0[0x10];
    short value10;
    char unidentified12[0x1ce];
    MoverOwnerLink *link1E0;
    bool IsRunning();
};

// Original 0x5a7970; tuning fields remain offset-based until identified.
struct PhysicsTuning {
    int value0;
    int value4;
    PhysicsUpgradeSelection *selection8;
    CarPhysicsRecord *recordC;
    int value10;
    unsigned char values14[41];
    char unidentified3D[3];
    float values40[23];
    float values9C[9];
    float valuesC0[9];
    int valueE4;
    PhysicsTuning *Construct();
    int CountActive(); // 0x59b0e0
};
UG2_CHECK_SIZE(PhysicsTuning, 0xe8);

// Original 0x5ac6e0/0x5b4dc0/0x5b5930 operate on PhysicsInfo +0x10.
// Keep this subobject explicit: its record +0x30 is outer object +0x40.
struct PhysicsParameters {
    int value0;
    signed char index4;
    char unidentified5[3];
    float value8;
    char unidentifiedC[4];
    float value10;
    float value14;
    float value18;
    float value1C;
    float value20;
    char unidentified24[0xc];
    CarPhysicsRecord record30;
    PhysicsUpgradeSelection selection2E0;
    PhysicsTuning tuning374;
    float curveSamples45C[64];
    EngineCurve driveCurve55C;
    PhysicsParameters *Construct(); // 0x5ac6e0
    void Compose(int mode, int refresh, int flag);
    void Refresh(int refresh, int flag, char select);
    float GetPhysicsSpeed(); // 0x5b5930
};
UG2_CHECK_OFFSET(PhysicsParameters, record30, 0x30);
UG2_CHECK_OFFSET(PhysicsParameters, selection2E0, 0x2e0);
UG2_CHECK_OFFSET(PhysicsParameters, tuning374, 0x374);
UG2_CHECK_OFFSET(PhysicsParameters, driveCurve55C, 0x55c);
UG2_CHECK_SIZE(PhysicsParameters, 0x570);

struct PhysicsInfo {
    char unidentified0[0x10];
    PhysicsParameters parameters10;
};
UG2_CHECK_OFFSET(PhysicsInfo, parameters10, 0x10);
UG2_CHECK_SIZE(PhysicsInfo, 0x580);

void ApplyPhysicsVariant(CarPhysicsRecord *, PhysicsUpgradeSelection *, float *); // 0x5a7530
void __stdcall ApplyPhysicsVariant3(CarPhysicsRecord *, PhysicsUpgradeSelection *); // 0x5a7750
void __stdcall ApplyPhysicsVariant10(CarPhysicsRecord *, PhysicsUpgradeSelection *); // 0x5a77c0
void __stdcall ApplyPhysicsVariant2(CarPhysicsRecord *, PhysicsUpgradeSelection *); // 0x5a7830
void BuildPhysicsDriveCurve(EngineCurve *output, PhysicsParameters *parameters); // 0x5b5ab0

// Car object +0xc passed to the mover constructor; only +0xd8 is observed here.
struct MoverCarInfo {
    char unidentified0[0xd8];
    signed char valueD8;
};

// Car object +0x2c; only virtual slot 0x78 is observed by the mover constructor.
class RaceParticipant {
public:
    virtual void Slot00() = 0; virtual void Slot04() = 0; virtual void Slot08() = 0;
    virtual void Slot0C() = 0; virtual void Slot10() = 0; virtual void Slot14() = 0;
    virtual void Slot18() = 0; virtual void Slot1C() = 0; virtual void Slot20() = 0;
    virtual void Slot24() = 0; virtual void Slot28() = 0; virtual void Slot2C() = 0;
    virtual void Slot30() = 0; virtual void Slot34() = 0; virtual void Slot38() = 0;
    virtual void Slot3C() = 0; virtual void Slot40() = 0; virtual void Slot44() = 0;
    virtual void Slot48() = 0; virtual void Slot4C() = 0; virtual void Slot50() = 0;
    virtual void Slot54() = 0; virtual void Slot58() = 0; virtual void Slot5C() = 0;
    virtual void Slot60() = 0; virtual void Slot64() = 0; virtual void Slot68() = 0;
    virtual void Slot6C() = 0; virtual void Slot70() = 0; virtual void Slot74() = 0;
    virtual int GetTuningLevel() = 0;
};

struct MoverContext {
    char unidentified0[4];
    int value4;
    char unidentified8[0x8f8];
    unsigned char flag900;
};

// The car object; its +0x40 region is DrivetrainAuxiliary.
struct MoverModeOwner {
    char unidentified0[0x10];
    PhysicsInfo *physicsInfo10;
    MoverContext *context14;
    char unidentified18[4];
    MoverOwnerObject *object1C;
    CarPhysicsRecord *record20;
    int value24;
    int moverType28;
    RaceParticipant *participant2C;
    MoverInput *input30;
    struct VehicleMover *mover34;
    int state38;
    CarCollisionBody *collisionBody3C;
    union {
        char unidentified40[0x3e4];
        struct {
            char state40[0x20];
            float draftPosition60;
            float draftPosition64;
        } view40;
    };
    int mode424;
    int value428;
    float speed42C;
    float speed430;
    int gear434;
    char unidentified438[8];
    float engineValue440;
    char unidentified444[4];
    int value448;
    char unidentified44C[0x1c];
    float value468;
    char unidentified46C[0x70];
    float value4DC;
    char unidentified4E0[4];
    float value4E4;
    char unidentified4E8[0x28];
    WheelVector previousPosition510;
    char unidentified51C[4];
    WheelVector previousVelocity520;
    char unidentified52C[4];
    WheelVector previousAngular530;
    char unidentified53C[0x14];
    unsigned char effectsEnabled550;
    char unidentified551[0x7b];
    int colliderPack5CC;
    char unidentified5D0[0x388];
    int forceFeedback958;
    unsigned char mask95C;
    char unidentified95D[0xf];
    int resetRequested96C;
    char unidentified970[4];
    int resetTick974;
    float resetDuration978;
    char unidentified97C[0x1c];
    unsigned char effectsSuppressed998;
    DrivetrainAuxiliary *State() {
        return reinterpret_cast<DrivetrainAuxiliary *>(unidentified40);
    }
    bool HasWheelInputs();
    int GetMoverWheelState(int wheel);
    bool IsResetPending();
    int HasContactBodies();
    int AreWheelsSettled();
    void RequestReset();
    void ApplyGuideForce();
    float GetEventCurveValue();
    float GetDraftValue();
    bool HasWheelEffects();
    // Recovered prefixes only: remaining bodies of 0x5fbbe0/0x604740 are unresolved.
    void SnapshotFrameKinematics(float duration);
    void UpdateFrameAcceleration(float duration);
};
UG2_CHECK_OFFSET(MoverModeOwner, previousPosition510, 0x510);
UG2_CHECK_OFFSET(MoverModeOwner, previousVelocity520, 0x520);
UG2_CHECK_OFFSET(MoverModeOwner, previousAngular530, 0x530);
UG2_CHECK_OFFSET(MoverModeOwner, participant2C, 0x2c);
UG2_CHECK_OFFSET(MoverModeOwner, mode424, 0x424);
UG2_CHECK_OFFSET(MoverModeOwner, engineValue440, 0x440);
UG2_CHECK_OFFSET(MoverModeOwner, value4E4, 0x4e4);
UG2_CHECK_OFFSET(MoverModeOwner, mask95C, 0x95c);
UG2_CHECK_OFFSET(MoverModeOwner, resetTick974, 0x974);
UG2_CHECK_OFFSET(MoverModeOwner, effectsEnabled550, 0x550);
UG2_CHECK_OFFSET(MoverModeOwner, colliderPack5CC, 0x5cc);
UG2_CHECK_OFFSET(MoverModeOwner, effectsSuppressed998, 0x998);
UG2_CHECK_OFFSET(MoverModeOwner, input30, 0x30);
UG2_CHECK_OFFSET(MoverOwnerObject, link1E0, 0x1e0);

// PhysicsMover, 0x130 bytes, vtable 0x7a25e0; integrator subobject at +8.
struct VehicleMover {
    void *vtable0;
    MoverInput *input4;
    char integrator8[0x14];
    int stateCount1C;
    MoverBody *body20;
    AckermannSteering *steering24;
    MoverArm *arms28[4];
    DrivetrainWheel *wheels38[4];
    Engine *engine48;
    Drivetrain *drivetrain4C;
    MoverAero *aero50;
    CarPhysicsRecord *record54;
    DrivetrainAuxiliary *state58;
    MoverModeOwner *owner5C;
    int value60;
    unsigned char flag64;
    char unidentified65[3];
    int value68;
    float value6C;
    float value70;
    int value74[4];
    unsigned char flag84;
    char unidentified85[0xb];
    WheelVector vector90;
    char unidentified9C[0x44];
    WheelVector vectorE0;
    char unidentifiedEC[4];
    WheelVector vectorF0;
    char unidentifiedFC[4];
    WheelVector sum100;
    char unidentified10C[4];
    WheelVector sum110;
    char unidentified11C[8];
    int value124;
    char unidentified128[8];

    VehicleIntegrator *Integrator() {
        return reinterpret_cast<VehicleIntegrator *>(integrator8);
    }
    void Construct(CarPhysicsRecord *record, MoverCarInfo *carInfo, MoverInput *input,
                   MoverModeOwner *owner);
    float Update(float duration);
    void InitialiseFromState();
    void RefreshLoads();
    void RefreshModes();
    void TransferControls();
    void DistributeBrakes(float duration);
    void ApplyLaunchAssist();
    void ExportState(float duration);
    void UpdateAfterIntegration();
    void FinishIntegration();
    void ApplyControllerForces();
    void ApplyYawAssist();
    void ApplyTractionAssist();
    void ApplyAirborneAssist();
    void ApplyNetworkCorrection();
    void SetResponseScale(float scale);
    void ResolveRestAndBumpStops();
    void ApplyWheelLift();
    // Original PhysicsMover vtable slot 0x0c (0x503b00).
    MoverBody *GetBody() {
        return body20;
    }
    void CorrectWheelCoupling();
    void ApplyOpposingBodyForce();
};
UG2_CHECK_OFFSET(VehicleMover, body20, 0x20);
UG2_CHECK_OFFSET(VehicleMover, record54, 0x54);
UG2_CHECK_OFFSET(VehicleMover, flag84, 0x84);
UG2_CHECK_OFFSET(VehicleMover, vectorE0, 0xe0);
UG2_CHECK_OFFSET(VehicleMover, sum100, 0x100);
UG2_CHECK_OFFSET(VehicleMover, value124, 0x124);
UG2_CHECK_SIZE(VehicleMover, 0x130);

struct DraftTable {
    char unidentified0[0x10];
    int *offsets10;
};

// Draft tracker object at 0x8a1f28; 0x5fa540 (Update) is not yet recovered.
class DraftTracker {
public:
    char unidentified0[0xc];
    DraftTable *table0C;
    char unidentified10[8];
    int offset18;
    void Update(float first, float second);
    float GetValue(int index);
};

// Replay/ghost recorder object at 0x89cf48; only two queried methods are observed.
class ReplayRecorder {
public:
    char unidentified0[0x21];
    unsigned char flag21;
    bool QueryState5EFE70();
    bool QueryState5F00F0();
};
UG2_CHECK_OFFSET(ReplayRecorder, flag21, 0x21);
extern int *raceState890118;
unsigned short WheelAngle(float x, float y);
float ComputeTractionFactor(float slip, float handbrake, float speed, float base, float extra,
                            int grounded, float *tilt, float event);
int UpdateMoverCentre(MoverModeOwner *owner, MoverBody *body, unsigned char *pending);

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
