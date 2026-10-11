#include "ug2_mod_api.h"
namespace {
int32_t UG2_CALL Create(void *, Ug2Handle, const Ug2VehicleState *) { return UG2_OK; }
void UG2_CALL Destroy(void *, Ug2Handle) {}
int32_t UG2_CALL Step(void *, Ug2Handle, float, const Ug2Controls *, const Ug2VehicleState *, Ug2VehicleState *) {
    // A dispatch request, not an additional call: host runs recovered Advance once.
    return UG2_PHYSICS_ORIGINAL;
}
int32_t UG2_CALL Impulse(void *, Ug2Handle, const Ug2Vec3 *, const Ug2Vec3 *, Ug2VehicleState *) {
    return UG2_ERROR; // Delegated vehicles use the original collision impulse routine.
}
int32_t UG2_CALL Synchronize(void *, Ug2Handle, const Ug2VehicleState *) { return UG2_OK; }
int32_t UG2_CALL Initialize(const Ug2HostApi *host) {
    if (host->api_version != UG2_MOD_API_VERSION || !(host->capabilities & UG2_CAP_PHYSICS)) return UG2_ERROR;
    Ug2PhysicsProvider provider = {sizeof(provider), 0, Create, Destroy, Step, Impulse, Synchronize};
    return host->register_physics(host->context, &provider);
}
void UG2_CALL Shutdown() {}
const Ug2ModDescriptor descriptor = {sizeof(descriptor), UG2_MOD_API_VERSION, UG2_CAP_PHYSICS,
                                    "original_physics", "1.0", 0, Initialize, Shutdown};
}
extern "C" UG2_EXPORT const Ug2ModDescriptor *UG2_CALL ug2_mod_query() { return &descriptor; }
