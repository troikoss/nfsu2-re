#ifndef UG2_MOD_API_H
#define UG2_MOD_API_H
/* Independent C ABI. No recovered headers are needed by plugin authors. */
#include <stdint.h>
#if defined(_MSC_VER)
#define UG2_CALL __cdecl
#define UG2_EXPORT __declspec(dllexport)
#elif defined(__i386__)
#define UG2_CALL __attribute__((cdecl))
#define UG2_EXPORT __attribute__((visibility("default")))
#else
#define UG2_CALL
#define UG2_EXPORT __attribute__((visibility("default")))
#endif
#ifdef __cplusplus
extern "C" {
#endif
#define UG2_MOD_API_VERSION 1u
#define UG2_CAP_EXPERT 1u
#define UG2_CAP_HUD 2u
#define UG2_CAP_PHYSICS 4u
#define UG2_CAP_PROJECTION 8u
#define UG2_OK 0
#define UG2_ERROR (-1)
#define UG2_PHYSICS_ORIGINAL 0
#define UG2_PHYSICS_CUSTOM 1
#define UG2_HUD_LEFT (-1)
#define UG2_HUD_CENTRE 0
#define UG2_HUD_RIGHT 1
typedef uint64_t Ug2Handle;
typedef struct Ug2Vec3 { float x, y, z; } Ug2Vec3;
typedef struct Ug2WheelState {
    float local_matrix[16], world_matrix[16];
    float spin, suspension, angle;
    int32_t surface;
    uint32_t contact;
} Ug2WheelState;
/* Matrices use the original WheelMatrix/OpenGL element layout. +X forward,
   +Z up; translation is separate. Mass/inertia retain original record units. */
typedef struct Ug2VehicleState {
    uint32_t struct_size;
    Ug2Vec3 position, velocity, angular_velocity;
    float orientation[16];
    float mass, inertia[16]; /* body-local inertia; inverse exposed to collision host */
    Ug2WheelState wheels[4];
    float rpm, turbo, nitrous;
    int32_t gear;
} Ug2VehicleState;
typedef struct Ug2Controls {
    float throttle, brake, handbrake;
    uint32_t steering_units;
    int32_t gear_request;
} Ug2Controls;
typedef struct Ug2Event {
    uint32_t struct_size, type;
    Ug2Handle vehicle;
    float duration;
    uint32_t width, height;
} Ug2Event;
enum Ug2EventType {
    UG2_HOST_READY = 1, UG2_VEHICLE_CREATED, UG2_VEHICLE_DESTROYING,
    UG2_PRE_SIMULATION, UG2_POST_SIMULATION, UG2_HUD_READY,
    UG2_PRE_HUD_LAYOUT, UG2_POST_WORLD_DRAW, UG2_POST_HUD_DRAW, UG2_SHUTDOWN, UG2_PRE_WORLD_PROJECTION
};
typedef struct Ug2Projection {
    uint32_t struct_size;
    float tangent_x, tangent_y;
    float reference_tangent_x, reference_tangent_y; /* original 4:3 projection, read-only */
} Ug2Projection;
typedef struct Ug2HudObject {
    uint32_t struct_size, hash, type;
    Ug2Handle handle, parent;
} Ug2HudObject;
typedef int32_t (UG2_CALL *Ug2Callback)(void *, const Ug2Event *);
typedef struct Ug2PhysicsProvider {
    uint32_t struct_size;
    void *userdata;
    int32_t (UG2_CALL *create)(void *, Ug2Handle, const Ug2VehicleState *);
    void (UG2_CALL *destroy)(void *, Ug2Handle);
    int32_t (UG2_CALL *step)(void *, Ug2Handle, float, const Ug2Controls *, const Ug2VehicleState *, Ug2VehicleState *);
    int32_t (UG2_CALL *impulse)(void *, Ug2Handle, const Ug2Vec3 *impulse_world, const Ug2Vec3 *lever_arm_world, Ug2VehicleState *);
    int32_t (UG2_CALL *synchronize)(void *, Ug2Handle, const Ug2VehicleState *);
} Ug2PhysicsProvider;
typedef struct Ug2ExpertApi {
    uint32_t struct_size;
    void *(UG2_CALL *vehicle_pointer)(void *, Ug2Handle); /* callback-scoped NativeVehicle */
} Ug2ExpertApi;
typedef struct Ug2HostApi {
    uint32_t struct_size, api_version, capabilities;
    const char *build_id;
    void *context; /* plugin-specific host token */
    void (UG2_CALL *log)(void *, const char *);
    int32_t (UG2_CALL *subscribe)(void *, uint32_t, Ug2Callback, void *);
    int32_t (UG2_CALL *get_vehicle)(void *, Ug2Handle, Ug2VehicleState *);
    uint32_t (UG2_CALL *hud_count)(void *);
    int32_t (UG2_CALL *hud_object)(void *, uint32_t, Ug2HudObject *);
    Ug2Handle (UG2_CALL *hud_find)(void *, uint32_t);
    int32_t (UG2_CALL *hud_translate)(void *, Ug2Handle, float, float); /* virtual screen units, draw-only */
    int32_t (UG2_CALL *register_physics)(void *, const Ug2PhysicsProvider *);
    const Ug2ExpertApi *expert;
    int32_t (UG2_CALL *get_projection)(void *, Ug2Projection *);
    int32_t (UG2_CALL *set_projection)(void *, const Ug2Projection *);
} Ug2HostApi;
typedef struct Ug2ModDescriptor {
    uint32_t struct_size, api_version, capabilities;
    const char *id;
    const char *version;
    const char *required_build_id; /* required for UG2_CAP_EXPERT */
    int32_t (UG2_CALL *initialize)(const Ug2HostApi *);
    void (UG2_CALL *shutdown)(void);
} Ug2ModDescriptor;
typedef const Ug2ModDescriptor *(UG2_CALL *Ug2ModQuery)(void);
UG2_EXPORT const Ug2ModDescriptor *UG2_CALL ug2_mod_query(void);
#ifdef __cplusplus
}
#endif
#endif
