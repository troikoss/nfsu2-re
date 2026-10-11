/* Synthetic protocol fixtures, not replacement gameplay. */
#include "ug2_mod_api.h"
#include <stdio.h>
#include <fenv.h>
#include <math.h>
#ifndef TEST_ID
#define TEST_ID "fixture"
#endif
#ifndef TEST_KIND
#define TEST_KIND 0
#endif
namespace {
const Ug2HostApi *host;
Ug2Handle destroyedHandle;
void Trace(const char *name) { printf("%s:%s\n", TEST_ID, name); }
int32_t UG2_CALL Late(void *, const Ug2Event *) { Trace("late"); return UG2_OK; }
int32_t UG2_CALL Callback(void *, const Ug2Event *event) {
    printf("%s:event:%u\n", TEST_ID, event->type);
    if (event->type == UG2_VEHICLE_DESTROYING) destroyedHandle = event->vehicle;
    if (event->type == UG2_VEHICLE_CREATED && destroyedHandle) {
        Ug2VehicleState out = {}; out.struct_size = sizeof(out);
        if (host->get_vehicle(host->context, destroyedHandle, &out) != UG2_ERROR) return UG2_ERROR;
        Trace("stale-handle-rejected");
    }
    if (TEST_KIND == 5 && event->type == UG2_HOST_READY) return UG2_ERROR;
    if (TEST_KIND == 6 && event->type == UG2_HOST_READY)
        host->subscribe(host->context, UG2_HOST_READY, Late, 0);
    if (TEST_KIND == 8) fesetround(FE_DOWNWARD);
    if (TEST_KIND == 9) {
        if (!host->expert) return UG2_ERROR;
        if (event->vehicle && !host->expert->vehicle_pointer(host->context, event->vehicle)) return UG2_ERROR;
    }
    return UG2_OK;
}
int32_t UG2_CALL Create(void *, Ug2Handle, const Ug2VehicleState *) {
    Trace("create"); return TEST_KIND == 10 ? UG2_ERROR : UG2_OK;
}
void UG2_CALL Destroy(void *, Ug2Handle) { Trace("destroy"); }
int32_t UG2_CALL Step(void *, Ug2Handle, float, const Ug2Controls *, const Ug2VehicleState *previous, Ug2VehicleState *next) {
    Trace("step");
    if (TEST_KIND == 7) { next->mass = NAN; return UG2_PHYSICS_CUSTOM; }
    if (TEST_KIND != 1 && TEST_KIND != 12) return UG2_PHYSICS_ORIGINAL;
    *next = *previous;
    next->mass = 2.0f;
    for (int i = 0; i < 16; ++i) next->inertia[i] = (i % 5 == 0) ? 1.0f : 0.0f;
    next->inertia[0] = 2.0f; next->inertia[5] = 3.0f; next->inertia[10] = 4.0f;
    return UG2_PHYSICS_CUSTOM;
}
int32_t UG2_CALL Impulse(void *, Ug2Handle, const Ug2Vec3 *impulse, const Ug2Vec3 *, Ug2VehicleState *state) {
    Trace("impulse");
    if (TEST_KIND == 12) return UG2_ERROR;
    // A known synthetic result for adapter verification, without integrating a car.
    state->velocity.x += impulse->x / state->mass;
    state->velocity.y += impulse->y / state->mass;
    state->velocity.z += impulse->z / state->mass;
    return UG2_OK;
}
int32_t UG2_CALL Sync(void *, Ug2Handle, const Ug2VehicleState *) {
    Trace("sync"); return TEST_KIND == 11 ? UG2_ERROR : UG2_OK;
}
int32_t UG2_CALL Initialize(const Ug2HostApi *api) {
    host = api; Trace("init");
    if (TEST_KIND == 8) fesetround(FE_DOWNWARD);
    for (uint32_t e = UG2_HOST_READY; e <= UG2_SHUTDOWN; ++e)
        if (host->subscribe(host->context, e, Callback, 0) != UG2_OK) return UG2_ERROR;
    if (TEST_KIND == 0 || TEST_KIND == 1 || TEST_KIND == 2 || TEST_KIND == 7 || TEST_KIND >= 10) {
        Ug2PhysicsProvider p = {sizeof(p), 0, Create, Destroy, Step, Impulse, Sync};
        if (host->register_physics(host->context, &p) != UG2_OK) return UG2_ERROR;
    }
    return TEST_KIND == 2 ? UG2_ERROR : UG2_OK;
}
void UG2_CALL Shutdown() { Trace("shutdown"); host = 0; }
const Ug2ModDescriptor descriptor = {sizeof(descriptor), TEST_KIND == 3 ? 999u : UG2_MOD_API_VERSION,
    (TEST_KIND == 4 || TEST_KIND == 9) ? UG2_CAP_EXPERT : UG2_CAP_PHYSICS,
    TEST_ID, "test", TEST_KIND == 4 ? "wrong-build" : "test-build", Initialize, Shutdown};
}
extern "C" UG2_EXPORT const Ug2ModDescriptor *UG2_CALL ug2_mod_query() { return &descriptor; }
