#include "VehicleMover.h"
#include "GameModeFlags.h"

extern float physicsDriveCurveSamples[];
extern float physicsCurveSpeedStep;
extern float physicsSpeedSearchStep;
extern float physicsSpeedLimitScale;
extern float engineCurveScale9;

// Original 0x5b5ab0, cdecl (curve, PhysicsInfo +0x10). Preserve the original
// one-gear-per-sample transition: the curve input is not recomputed on a shift.
void BuildPhysicsDriveCurve(EngineCurve *output, PhysicsParameters *parameters) {
    CarPhysicsRecord *record = &parameters->record30;
    DrivetrainSpec *drive = &record->drivetrain1B0;
    EngineSpec *engine = reinterpret_cast<EngineSpec *>(record->engine1F0);
    float selector = drive->selector10;
    float other = 1.0f - selector;
    float radius = selector * record->wheels10[2].value10 + other * record->wheels10[0].value10;
    float coefficient = selector * drive->coefficientC + other * drive->coefficient8;
    float mass = record->body110.mass0;
    float upshift[9];
    float downshift[9];
    ComputeShiftPoints(drive, engine, upshift, downshift);
    EngineCurve engineCurve;
    engineCurve.count = 9;
    engineCurve.start = engine->value0;
    engineCurve.end = engine->value8;
    engineCurve.samples = engine->samples10;
    engineCurve.inverseSpacing = engineCurveScale9 / (engineCurve.end - engineCurve.start);
    float inverseRadius = 1.0f / radius;
    int gear = 2;
    int generated = 0;
    float speed = 0.0f;
    do {
        float input = inverseRadius * speed * drive->ratios[gear] * coefficient * drivetrainValueScale;
        if (input >= upshift[gear]) {
            ++gear;
            if (gear >= drive->unidentified18 + 2) break;
        }
        physicsDriveCurveSamples[generated] = engineCurve.Lookup(input) * drive->ratios[gear]
                                          * coefficient * inverseRadius / mass;
        ++generated;
        speed = speed + physicsCurveSpeedStep;
    } while (generated < 92);
    int trimmed = generated - 1;
    for (int i = trimmed; i < 92; ++i) physicsDriveCurveSamples[i] = 0.0f;
    EngineCurve sampled;
    sampled.count = generated;
    sampled.start = 0.0f;
    sampled.end = speed;
    sampled.inverseSpacing = (float)trimmed / speed;
    sampled.samples = physicsDriveCurveSamples;
    for (int i = 0; i < output->count; ++i) {
        output->samples[i] = sampled.Lookup((float)i * speed / (float)(output->count - 1));
    }
    output->start = 0.0f;
    output->end = speed;
    output->inverseSpacing = (float)(output->count - 1) / speed;
    parameters->value10 = speed;
}

// Original 0x5b5930. Composition and refresh boundaries are retained; the
// native demo must populate this layout before integrating its steering caller.
float PhysicsParameters::GetPhysicsSpeed() {
    if (index4 == -1) Refresh(1, 1, 1);
    float speed = driveCurve55C.start;
    while (speed <= driveCurve55C.end) {
        float force = driveCurve55C.Lookup(speed);
        if (index4 == -1) Refresh(1, 1, 1);
        volatile float drag = speed * record30.aero260.coefficient0 * speed;
        if (index4 == -1) Refresh(1, 1, 1);
        if (drag / record30.body110.mass0 > force) break;
        // 0x5b59d7 spills the next speed but compares the still-unrounded
        // x87 value against the endpoint before entering another iteration.
        double nextSpeed = (double)physicsSpeedSearchStep + (double)speed;
        speed = (float)nextSpeed;
        if (nextSpeed > (double)driveCurve55C.end) break;
    }
    if (index4 == -1 && value0 != -1) {
        int mode = 0;
        if (IsModeGroup3()) mode = 3;
        else if (gameFlag89E7E3 || gameFlag89010C) mode = 10;
        else if (IsModeGroup2()) mode = 2;
        Compose(mode, 1, 1);
    }
    if (record30.value290 > 0.0f) {
        if (index4 == -1) Refresh(1, 1, 1);
        float limit = record30.value290 * physicsSpeedLimitScale;
        if (limit < speed) return limit;
    }
    return speed;
}
