#ifndef UG2_NATIVE_MOD_BRIDGE_H
#define UG2_NATIVE_MOD_BRIDGE_H
#include "NativeVehicle.h"
#include "../../../modding/runtime/ModRuntime.h"
void NativeModsStart(const char *directory, bool disabled);
void NativeModsStop();
bool NativeModsFailed();
CarCollisionBody *NativeModsCollisionBody(MoverModeOwner *owner, int id);
void NativeModsVehicleCreated(NativeVehicle *vehicle);
void NativeModsVehicleDestroying(NativeVehicle *vehicle);
void NativeModsBeforeStep(NativeVehicle *vehicle, float duration);
void NativeModsAfterStep(NativeVehicle *vehicle, float duration);
void NativeModsProjection(CameraProjection *projection, const CameraProjection &reference, uint32_t width, uint32_t height);
void NativeModsEvent(uint32_t event, float duration = 0, uint32_t width = 0, uint32_t height = 0);
void NativeModsHudAdapter(uint32_t (*count)(), int (*object)(uint32_t, Ug2HudObject *),
                          Ug2Handle (*find)(uint32_t), int (*translate)(Ug2Handle, float, float));
struct NativeModSession {
    NativeVehicle *vehicle;
    bool finished;
    NativeModSession(const char *directory, bool disabled) : vehicle(0), finished(false) { NativeModsStart(directory, disabled); }
    void Finish() {
        if (finished) return;
        finished = true;
        if (vehicle && vehicle->owner) DestroyNativeVehicle(vehicle);
        NativeModsStop();
    }
    ~NativeModSession() { Finish(); }
};
#endif
