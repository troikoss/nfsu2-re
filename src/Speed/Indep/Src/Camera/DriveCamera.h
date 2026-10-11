#ifndef UG2_DRIVE_CAMERA_H
#define UG2_DRIVE_CAMERA_H

// Recovered UG2 PC v1.2 player drive camera: the mover built by Player::SetCameraState case 1
// (0x5f1fd0), constructor 0x451660, vtable 0x787f28, Update 0x453570.
//
// Layouts are partial and offset-based; UG2_CHECK_* pin what was observed in the original.
// Platform seams (not recovered, named where they are used): the world collision pushes
// 0x444530/0x44b040, camera shake 0x452ed0/0x572a50, Camera object finalisation 0x447280, the
// cockpit blocked test 0x445fc0/0x5d39f0 and the producer of the car acceleration vector at
// car +0x80.

#include "../Physics/Ug2Layout.h"
#include "../Physics/WheelMath.h"

struct MoverModeOwner;

// Cubic Hermite follower channel, 0x30 bytes. Original 0x5749e0 advances it; every UG2 camera
// spring is a set of these. A target change (state 2) restarts the curve from the current value
// and velocity; the curve is evaluated up to limit28 (10.0 for the drive camera).
struct CameraCubicChannel {
    float value0;
    float velocity4;
    float target8;
    float targetVelocityC;
    float cubicA10;
    float cubicB14;
    float cubicC18;
    float cubicD1C;
    float time20;
    float duration24;
    float limit28;
    short state2C;
    short mode2E;
    void Update(float step, float maximumVelocity, float maximumAcceleration);
    void LimitVelocity(float maximum);
    void LimitAcceleration(float maximum);
    void Snap();
};
UG2_CHECK_SIZE(CameraCubicChannel, 0x30);
UG2_CHECK_OFFSET(CameraCubicChannel, target8, 0x8);
UG2_CHECK_OFFSET(CameraCubicChannel, duration24, 0x24);
UG2_CHECK_OFFSET(CameraCubicChannel, state2C, 0x2c);

// Three channels (x, y, z), 0x90 bytes. 0x4419a0/0x441a20 construct, 0x574df0 updates.
struct CameraCubicVector {
    CameraCubicChannel channels0[3];
    void Construct(short mode, float duration, float limit);
    void Construct(short mode, const float *durations, float limit);
    void Update(float step);
    void SetValue(float x, float y, float z);
    void SetTarget(float x, float y, float z);
    void SetDurations(const float *durations);
    void SetDuration(float duration);
    void Snap();
};
UG2_CHECK_SIZE(CameraCubicVector, 0x90);

// Moving average over the last `capacity` vectors (0x30 bytes; ctor 0x446e70, push 0x446f10).
struct CameraAverager {
    void *vtable0;
    unsigned char stride4;
    unsigned char capacity5;
    unsigned char count6;
    unsigned char head7;
    float *buffer8;
    char unidentifiedC[4];
    float sum10[3];
    char unidentified1C[4];
    float average20[3];
    char unidentified2C[4];
    void Construct(int capacity);
    void Push(const float *vector);
};
UG2_CHECK_SIZE(CameraAverager, 0x30);
UG2_CHECK_OFFSET(CameraAverager, average20, 0x20);

// A per-car camera setting (16 bytes) stored in CarTypeInfo +0x720. `x4` is the distance forward
// (negative = behind the car), `y8` the height and `zC` the lateral offset; angle is in
// 1/65536 turn units (pitch of the look direction).
struct CameraSetting {
    short type0;
    unsigned short angle2;
    float x4;
    float y8;
    float zC;
};
UG2_CHECK_SIZE(CameraSetting, 0x10);

// CarTypeInfo +0x720: two six-entry tables, the first for one view, the second when two views
// are active (view state 3). Original lookup 0x441140.
struct CameraSettingsTable {
    int count0;
    int count4;
    char unidentified8[8];
    CameraSetting entries10[6];
    CameraSetting entries70[6];
    const CameraSetting *Find(int type, int viewState) const;
    const CameraSetting *FindOrDefault(int type, int viewState) const;
};
UG2_CHECK_OFFSET(CameraSettingsTable, entries10, 0x10);
UG2_CHECK_OFFSET(CameraSettingsTable, entries70, 0x70);

// Static camera dynamics: a curve of 0xb0-byte keys per camera type (table at 0x7ec610). Every
// float is a tuning value; the drive camera samples the curve at the constant 0x787154 (0.5).
struct CameraDynamicsKey {
    float durations0[4];        // eye, target, FOV, up/heading durations in seconds
    float upScale10[4];         // up vector response to acceleration
    float upMinimum20[4];
    float upMaximum30[4];
    float eyeScale40[4];        // eye offset response to acceleration
    float eyeMinimum50[4];
    float eyeMaximum60[4];
    float targetScale70[4];     // look-at offset response to acceleration
    float targetMinimum80[4];
    float targetMaximum90[4];
    float headingDurationsA0[4];
};
UG2_CHECK_SIZE(CameraDynamicsKey, 0xb0);

struct CameraDynamicsCurve {
    int count0;
    float start4;
    float unknown8;
    float inverseSpacingC;
    CameraDynamicsKey *keys10;
    void Sample(CameraDynamicsKey *output, float position);
};
UG2_CHECK_SIZE(CameraDynamicsCurve, 0x14);

// Piecewise linear (x, y) curve used by the chase heading (0x576790); unused by the drive
// camera but part of the same class.
struct CameraPairCurve {
    float *points0;
    int count4;
    float Lookup(float input);
};

// Observed output of the original drive camera, handed to the platform renderer. In the original
// these are fields of the Camera object (view +0x40) written through 0x447280.
struct DriveCameraView {
    float eye[3];
    float target[3];
    float up[3];
    unsigned short fov;      // Camera +0xc4, full horizontal angle in 1/65536 turn
    float nearPlane;         // Camera +0xbc
    float farPlane;          // Camera +0xc0
    bool attachedToCar;      // Camera +0x280 set (cockpit types)
    bool chase;              // chase types run the world collision pushes
};

// Camera types stored in CameraSetting::type0.
enum DriveCameraType {
    DRIVE_CAMERA_FAR = 0,
    DRIVE_CAMERA_CLOSE = 1,
    DRIVE_CAMERA_BUMPER = 2,
    DRIVE_CAMERA_DRIVER = 3,
    DRIVE_CAMERA_HOOD = 4,
    DRIVE_CAMERA_SUPER_FAR = 5
};

// Projection of the main view (eView type 1), original 0x5c7ec0: tangent of the horizontal and
// vertical half angles and the clip planes. `aspect` is viewport height / width. The matrix the
// original builds is D3D left-handed with a flipped Y; these are its magnitudes.
struct CameraProjection {
    float tangentX;
    float tangentY;
    float nearPlane;
    float farPlane;
};
void ComputeCameraProjection(CameraProjection *output, unsigned short fov, float aspect,
                             float nearPlane, float farPlane);

// Original 0x502500: maps the profile's drive camera option (0 bumper, 1 hood, 2 close, 3 far,
// 4 super far) to the camera type.
int DriveCameraTypeForOption(int option);

// Original static initialisers 0x77be00 (0x82a4e0 curve) and 0x77be70/0x77be80/0x77be60.
void InitialiseDriveCameraStatics();

// The mover, 0x60 bytes in the original. Slots the original dispatches through its vtable
// (0x24, 0x44) are direct calls here: slot 0x24 is `xor al, al` for this class.
class DriveCameraMover {
public:
    void *vtable0;
    char base4[0x28];                    // camera mover base 0x44ab90 (view, Camera, player)
    MoverModeOwner *car2C;
    int unknown30;
    CameraCubicChannel *fov34;
    CameraCubicVector *eye38;
    CameraCubicVector *target3C;
    CameraCubicVector *heading40;
    CameraCubicVector *up44;
    int type48;
    int previousType4C;
    unsigned char swing50;               // use the acceleration swing (0 under !param_5)
    unsigned char lookBack51;
    unsigned char snap52;
    unsigned char flag53;
    unsigned char active54;
    char unidentified55[3];
    int groundTime58;
    CameraAverager *averager5C;

    // Constructor 0x451660 reduced to the state the update consumes. `type` is a DriveCameraType.
    // `disableSwing` is the original's fifth argument (swing50 = !disableSwing) and `noDefaults`
    // the seventh (+0x53).
    void Construct(MoverModeOwner *car, const CameraSettingsTable *settings, int viewState,
                   int type, bool disableSwing, bool lookBack, bool noDefaults);
    void Destroy();
    // Slot 0x20 (0x445b10) and slot 0x14 (0x445a30).
    void SetType(int type);
    void SetLookBack(bool lookBack);
    // Slot 0x04 (0x453570). `step` is the global frame time 0x865154, `clock` the tick 0x86518c.
    void Update(float step, int clock, DriveCameraView *view);

    // Pose 0x44de90, setting read 0x445eb0 and car frame 0x445e00.
    void ComputePose(WheelMatrix *carFrame, const CameraSetting *setting, const CameraDynamicsKey *key,
                     int clock);
    void ReadSetting(float *offset, unsigned short *angle, const CameraSetting *setting);
    void GetCarFrame(WheelMatrix *output);

    const CameraSettingsTable *settings;     // platform: CarTypeInfo +0x720 of the car
    int viewState;                           // global 0x8707dc
};

#endif
