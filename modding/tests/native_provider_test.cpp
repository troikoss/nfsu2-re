// Real native collision adapter with a synthetic frozen-state provider.
#include "NativeModBridge.h"
#include "NativeWorld.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "native provider check failed line %d: %s\n", __LINE__, #x); return 1; } } while (0)
int main(int argc, char **argv) {
    if (argc != 3) return 2;
    unsigned short control = 0x027f; __asm__ volatile("fldcw %0" : : "m"(control));
    char error[512];
    CHECK(InitialiseNativeRuntime(argv[1], error, sizeof(error)));
    NativeModSession session(argv[2], false);
    NativeTrack track; CHECK(LoadNativeTrack(argv[1], "L4RA", &track, error, sizeof(error)));
    NativeVehicle vehicle = {}; session.vehicle = &vehicle;
    CHECK(CreateNativeVehicle(&vehicle, argv[1], "SKYLINE", track.spawn[0], track.spawn[1], track.spawn[2] + 0.45f,
                              track.heading, error, sizeof(error)));
    CarCollisionBody *body = vehicle.owner->collisionBody3C;
    body->Advance(1.0f / 60);
    CHECK(!NativeModsFailed());
    CHECK(fabsf(body->GetMass() - 0.5f) < 1e-6f);
    WheelMatrix *inverse = body->GetInverseInertia();
    // Unequal body-local moments must be inverted and rotated into world axes.
    const float moments[3] = {0.5f, 1.0f / 3.0f, 0.25f};
    const float *rotation = vehicle.mover->body20->matrix30.elements;
    for (int row = 0; row < 3; ++row) for (int col = 0; col < 3; ++col) {
        float expected = 0;
        for (int axis = 0; axis < 3; ++axis)
            expected += rotation[axis * 4 + row] * moments[axis] * rotation[axis * 4 + col];
        CHECK(fabsf(inverse->elements[col * 4 + row] - expected) < 1e-5f);
    }
    float previous = body->GetVelocity()->x;
    WheelVector impulse = {4, 0, 0}, point = {0, 0, 0};
    body->ApplyImpulse(&impulse, &point); body->SyncMover();
    CHECK(!NativeModsFailed()); CHECK(fabsf(body->GetVelocity()->x - previous - 2) < 1e-6f);
    CHECK(fabsf(vehicle.owner->State()->velocity30.x - body->GetVelocity()->x) < 1e-6f);
    WheelVector separated = *body->GetPosition(); separated.x += 0.25f;
    body->SetPosition(&separated); body->SyncMover();
    CHECK(!NativeModsFailed());
    CHECK(fabsf(body->GetPosition()->x - separated.x) < 1e-6f);
    body->Rewind(0.0f); body->SyncMover();
    StepNativeVehicle(&vehicle, 1.0f / 60); CHECK(!NativeModsFailed());
    DestroyNativeVehicle(&vehicle);
    CHECK(CreateNativeVehicle(&vehicle, argv[1], "SKYLINE", track.spawn[0], track.spawn[1], track.spawn[2] + 0.45f,
                              track.heading, error, sizeof(error)));
    StepNativeVehicle(&vehicle, 1.0f / 60); CHECK(!NativeModsFailed());
    printf("native collision-provider exchange and respawn passed\n");
    return 0;
}
