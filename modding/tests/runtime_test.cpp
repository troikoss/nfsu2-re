#include "ModRuntime.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fenv.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "check failed: %s:%d: %s\n", __FILE__, __LINE__, #x); return 1; } } while (0)
namespace {
Ug2VehicleState state;
float offsets[2];
Ug2Projection projection;
int GetProjection(Ug2Projection *out) { *out = projection; return UG2_OK; }
int SetProjection(const Ug2Projection *out) { projection.tangent_x = out->tangent_x; projection.tangent_y = out->tangent_y; return UG2_OK; }
int Vehicle(Ug2Handle h, Ug2VehicleState *out) { if (h != 1) return UG2_ERROR; *out = state; return UG2_OK; }
uint32_t Count() { return 2; }
int Object(uint32_t i, Ug2HudObject *o) {
    if (i > 1) return UG2_ERROR;
    o->handle = i + 1; o->parent = 0; o->hash = i ? 0x5164d4eau : 0x90fe80f3u; o->type = i ? 5 : 1;
    return UG2_OK;
}
Ug2Handle Find(uint32_t h) { return h == 0x5164d4eau ? 2 : (h == 0x90fe80f3u ? 1 : 0); }
int Translate(Ug2Handle h, float x, float y) { if (h < 1 || h > 2 || y != 0) return UG2_ERROR; offsets[h - 1] = x; return UG2_OK; }
void *Expert(Ug2Handle h) { return h == 1 ? &state : 0; }
void InitializeState() {
    memset(&state, 0, sizeof(state)); state.struct_size = sizeof(state); state.mass = 1;
    for (int i = 0; i < 16; ++i) state.orientation[i] = state.inertia[i] = i % 5 == 0 ? 1 : 0;
    for (int w = 0; w < 4; ++w) for (int i = 0; i < 16; ++i)
        state.wheels[w].local_matrix[i] = state.wheels[w].world_matrix[i] = i % 5 == 0 ? 1 : 0;
}
}
int main(int argc, char **argv) {
    if (argc != 3) return 2;
    InitializeState();
    CHECK(ug2mod::Runtime::ValidateState(state));
    Ug2VehicleState invalid = state;
    invalid.mass = 0; CHECK(!ug2mod::Runtime::ValidateState(invalid));
    invalid = state; invalid.inertia[0] = -1; CHECK(!ug2mod::Runtime::ValidateState(invalid));
    invalid = state; invalid.orientation[0] = -1; CHECK(!ug2mod::Runtime::ValidateState(invalid));
    invalid = state; invalid.velocity.x = NAN; CHECK(!ug2mod::Runtime::ValidateState(invalid));
    invalid = state; invalid.wheels[0].contact = 2; CHECK(!ug2mod::Runtime::ValidateState(invalid));
    ug2mod::Adapter adapter = {Vehicle, Count, Object, Find, Translate, Expert, GetProjection, SetProjection};
    ug2mod::Runtime runtime("test-build", adapter);
    fesetround(FE_TONEAREST);
    runtime.Load(argv[1]); CHECK(fegetround() == FE_TONEAREST);
    Ug2Event event = {sizeof(event), UG2_HOST_READY, 0, 0, 0, 0};
    runtime.Dispatch(event); CHECK(fegetround() == FE_TONEAREST);
    runtime.Dispatch(event); CHECK(fegetround() == FE_TONEAREST);
    event.type = UG2_VEHICLE_CREATED; event.vehicle = 1; runtime.Dispatch(event);
    printf("provider:%s\n", runtime.ProviderId());
    if (!strcmp(argv[2], "hud")) {
        event.type = UG2_PRE_HUD_LAYOUT; event.width = 640; event.height = 480;
        runtime.Dispatch(event); CHECK(offsets[0] == 0 && offsets[1] == 0);
        event.width = 1280; event.height = 720; runtime.Dispatch(event);
        CHECK(fabsf(offsets[0] + 106.66667f) < 0.001f && fabsf(offsets[1] - 106.66667f) < 0.001f);
        runtime.Dispatch(event); CHECK(fabsf(offsets[1] - 106.66667f) < 0.001f);
        event.width = 480; event.height = 640; runtime.Dispatch(event); CHECK(offsets[0] == 0 && offsets[1] == 0);
        event.type = UG2_PRE_WORLD_PROJECTION;
        const uint32_t widths[] = {640, 1280, 1680, 2560};
        const uint32_t heights[] = {480, 720, 720, 720};
        for (int i = 0; i < 4; ++i) {
            projection = {sizeof(projection), 0.8f, 0.4f, 0.8f, 0.4f};
            event.width = widths[i]; event.height = heights[i]; runtime.Dispatch(event);
            float expected = 0.8f * (float(widths[i]) / heights[i]) / (4.0f / 3.0f);
            CHECK(fabsf(projection.tangent_x - expected) < 0.00001f && projection.tangent_y == 0.4f);
        }
    } else if (runtime.HasProvider()) {
        bool created = runtime.Create(1, state);
        if (!strcmp(argv[2], "create_failure")) CHECK(!created && runtime.Failed());
        else {
            CHECK(created); Ug2Controls controls = {}; Ug2VehicleState next = state;
            int result = runtime.Step(1, 1.0f / 60, controls, state, &next);
            if (!strcmp(argv[2], "invalid")) CHECK(result == UG2_ERROR && runtime.Failed());
            else if (!strcmp(argv[2], "custom") || !strcmp(argv[2], "impulse_failure")) {
                CHECK(result == UG2_PHYSICS_CUSTOM && next.mass == 2);
                Ug2Vec3 impulse = {4, 0, 0}, point = {0, 0, 0};
                bool applied = runtime.Impulse(1, impulse, point, &next);
                if (!strcmp(argv[2], "impulse_failure")) CHECK(!applied && runtime.Failed());
                else { CHECK(applied && next.velocity.x == 2); CHECK(runtime.Synchronize(1, next)); }
            } else {
                CHECK(result == UG2_PHYSICS_ORIGINAL);
                bool synchronized = runtime.Synchronize(1, state);
                if (!strcmp(argv[2], "sync_failure")) CHECK(!synchronized && runtime.Failed());
                else CHECK(synchronized);
            }
        }
        runtime.Destroy(1);
    }
    runtime.Shutdown(); runtime.Shutdown();
    return 0;
}
