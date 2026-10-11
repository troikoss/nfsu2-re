#ifndef UG2_NATIVE_VEHICLE_H
#define UG2_NATIVE_VEHICLE_H

#include "../../Speed/Indep/Src/Physics/VehicleMover.h"
#include "../../Speed/Indep/Src/Physics/Engine.h"
#include "../../Speed/Indep/Src/Camera/DriveCamera.h"

// Platform scaffolding: driver controls. The original control filtering that fills
// these fields (speed-sensitive steering, input smoothing) is not yet recovered.
class NativeControls : public MoverInput {
public:
    int selectedGear;
    int currentGear;
    int requestedGear;
    void UnknownSlot00();
    void UnknownSlot04();
    void UnknownSlot08();
    void UnknownSlot0C();
    void UnknownSlot10();
    void UnknownSlot14();
    void UnknownSlot18();
    void UnknownSlot1C();
    int GetGearRequest();
    int GetDirectGear();
    void SetSelectedGear(int gear);
    void SetCurrentGear(int gear);
    void UnknownSlot30(); void UnknownSlot34(); void UnknownSlot38(); void UnknownSlot3C();
    void UnknownSlot40(); void UnknownSlot44(); void UnknownSlot48(); void UnknownSlot4C();
    void UnknownSlot50(); void UnknownSlot54(); void UnknownSlot58(); void UnknownSlot5C();
    void UnknownSlot60(); void UnknownSlot64(); void UnknownSlot68(); void UnknownSlot6C();
    void UnknownSlot70(); void UnknownSlot74(); void UnknownSlot78(); void UnknownSlot7C();
    void UnknownSlot80();
    unsigned short GetGuideHeading();
};

struct NativeVehicle {
    VehicleMover *mover;
    MoverModeOwner *owner;
    NativeControls *controls;
    PhysicsInfo *info;
    MoverCarInfo *carInfo;
    char carName[32];
    // Platform scaffolding: CarTypeInfo +0x720 camera table, and the previous velocity used to
    // produce the acceleration vector (car +0x80) whose original producer is not recovered.
    CameraSettingsTable cameraSettings;
    float previousVelocity[3];
    bool haveVelocity;
};

struct NativeSkidSegment {
    WheelVector centre;
    WheelVector across;
    float intensity;
    int surface;
    bool connected;
};
enum { NATIVE_SKID_CAPACITY = 8192 };
extern NativeSkidSegment nativeSkidSegments[NATIVE_SKID_CAPACITY];
extern int nativeSkidCount;

void NativeTrap(const char *what);
bool InitialiseNativeRuntime(const char *gameDirectory, char *error, int errorSize);
bool CreateNativeVehicle(NativeVehicle *vehicle, const char *gameDirectory, const char *carName,
                         float startX, float startY, float startZ, float heading, char *error,
                         int errorSize);
void SetNativeControls(NativeVehicle *vehicle, float throttle, float brake, float handbrake, float steer);
float StepNativeVehicle(NativeVehicle *vehicle, float duration);
void DestroyNativeVehicle(NativeVehicle *vehicle);

#endif
