// Headless driving check for the native build (platform scaffolding).
#include "NativeVehicle.h"
#include "NativeWorld.h"

#include <fenv.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern float engineValueScale;

int main(int argc, char **argv) {
    const char *game = argc > 1 ? argv[1] : "Need for Speed Underground 2";
    const char *car = argc > 2 ? argv[2] : "SKYLINE";
    int frames = argc > 3 ? atoi(argv[3]) : 1200;
    char error[512];
    unsigned short control = 0x027f;
    __asm__ volatile("fldcw %0" : : "m"(control));
    if (getenv("UG2_FPE")) {
        feenableexcept(FE_INVALID | FE_DIVBYZERO);
    }
    if (!InitialiseNativeRuntime(game, error, sizeof(error))) {
        fprintf(stderr, "ug2sim: %s\n", error);
        return 1;
    }
    NativeTrack track;
    if (!LoadNativeTrack(game, "L4RA", &track, error, sizeof(error))) {
        fprintf(stderr, "ug2sim: %s\n", error);
        return 1;
    }
    printf("track: %d collision packs, spawn (%.1f %.1f %.1f)\n", track.packCount, track.spawn[0],
           track.spawn[1], track.spawn[2]);
    NativeVehicle vehicle;
    if (!CreateNativeVehicle(&vehicle, game, car, track.spawn[0], track.spawn[1], track.spawn[2] + 0.45f,
                             track.heading, error, sizeof(error))) {
        fprintf(stderr, "ug2sim: %s\n", error);
        return 1;
    }
    const float duration = 1.0f / 60.0f;
    for (int frame = 0; frame <= frames; ++frame) {
        float seconds = frame * duration;
        float throttle = seconds > 1.0f && seconds < 12.0f ? 1.0f : 0.0f;
        float brake = seconds >= 14.0f ? 1.0f : 0.0f;
        if (getenv("UG2_REVERSE")) {
            throttle = 0.0f;
            brake = seconds > 1.0f ? 1.0f : 0.0f;
        }
        float steer = seconds > 6.0f && seconds < 9.0f ? 0.3f : 0.0f;
        SetNativeControls(&vehicle, throttle, brake, 0.0f, steer);
        StepNativeVehicle(&vehicle, duration);
        if (frame % 30 == 0) {
            DrivetrainAuxiliary *state = vehicle.owner->State();
            MoverBody *body = vehicle.mover->body20;
            printf("t=%5.2f pos=(%8.2f %8.2f %6.3f) speed=%6.1f km/h rpm=%6.0f gear=%d grounded=%d travel=%.3f\n",
                   seconds, body->position20.x, body->position20.y, body->position20.z,
                   state->value3F0 * 3.6f, vehicle.mover->engine48->state14 * engineValueScale,
                   vehicle.mover->drivetrain4C->ratioIndex, state->value3E4,
                   vehicle.mover->arms28[0]->travel1C);
            WCollider *collider = vehicle.owner->State()->collider4;
            WheelVector probe = body->position20;
            int surface = -1;
            char found = 0;
            WheelVector normal;
            float ground = collider->QueryGround(&probe, &surface, &normal, &found);
            printf("      pack=%d tracker=%d radius=%.1f ground=%.2f found=%d surface=%d normal=(%.2f %.2f %.2f)\n",
                   collider->pack8 ? collider->pack8->section30 : -1,
                   collider->trackerC.section8 ? collider->trackerC.section8->id8 : -1, collider->trackerC.radiusC,
                   ground, found, surface, normal.x, normal.y, normal.z);
            if (!isfinite(body->position20.x) || !isfinite(body->position20.z)) {
                fprintf(stderr, "ug2sim: non-finite state\n");
                return 2;
            }
        }
    }
    return 0;
}
