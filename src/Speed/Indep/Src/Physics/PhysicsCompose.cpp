#include "VehicleMover.h"
#include "PhysicsUpgradeSource.h"

extern unsigned char *carTypeInfoTable;
extern float physicsSpeedSearchStep;
extern float physicsTopSpeedLimit;
extern float physicsClassThresholds[4];
extern float engineCurveScale9;
extern float drivetrainValueScale;
extern float drivetrainInitialRange;
extern float drivetrainModeCoefficients[3];
extern float physicsPerformanceStep;
extern float physicsPerformanceMph60;
extern float physicsPerformanceMph100;
extern float physicsPerformanceDistance;
extern float physicsPerformanceSixtyScale[46];
extern float physicsPerformanceHundredScale[46];
extern float hudSpeedScaleImperial78426C;
extern float physicsUpgradeRounding;

// 0x59a350 integrates drive force minus aerodynamic drag over speed in
// 1.341-unit steps up to 44.7 or the first speed where drag exceeds the
// drive curve, returning the accumulated area. An unset class index (-1)
// refreshes the parameters before each record read.
float PhysicsParameters::GetDriveArea() {
    float total = 0.0f;
    float speed = 0.0f;
    for (;;) {
        float force = driveCurve55C.Lookup(speed);
        if (index4 == -1) Refresh(1, 1, 1);
        float drag = (float)(((double)speed * record30.aero260.coefficient0) * speed);
        if (index4 == -1) Refresh(1, 1, 1);
        double resistance = (double)drag / record30.body110.mass0;
        if (force < resistance) return total;
        total = (float)(((double)force - resistance) * physicsSpeedSearchStep + total);
        speed = (float)((double)physicsSpeedSearchStep + speed);
        if (!(speed < physicsTopSpeedLimit)) return total;
    }
}

// 0x5b06a0 simulates a standing start in 1/60 steps with the record's torque
// curve, gear ratios, shift points (0x5ac7d0) and drag. It stores the top
// drive area (0x59a350) at +0xc, the per-car scaled times to 60 and 100 mph at
// +0x14/+0x18, and the time and speed at 400.5 distance at +0x1c/+0x20.
// Shifts ramp a 0..1 factor through the mode coefficients -3 and 1.5.
void PhysicsParameters::SimulatePerformance() {
    float *record = reinterpret_cast<float *>(&record30);
    EngineSpec *engine = reinterpret_cast<EngineSpec *>(record30.engine1F0);
    DrivetrainSpec *drive = &record30.drivetrain1B0;
    EngineCurve curve;
    curve.start = engine->value0;
    curve.end = engine->value8;
    curve.samples = engine->samples10;
    curve.count = 9;
    curve.inverseSpacing = (float)(engineCurveScale9 / ((double)curve.end - curve.start));
    valueC = GetDriveArea();
    value14 = 0.0f;
    value18 = 0.0f;
    value20 = 0.0f;
    value1C = 0.0f;
    float split = drive->selector10;
    double front = 1.0 - split;
    float upshift[9];
    float downshift[9];
    float mass = record30.body110.mass0;
    float speed = 0.0f;
    float time = 0.0f;
    float distance = 0.0f;
    unsigned char engaged = 1;
    float clutch = 1.0f;
    float radius = (float)((double)split * record[0x80 / 4] + front * record[0x20 / 4]);
    float finalDrive = (float)(front * drive->coefficient8 + (double)split * drive->coefficientC);
    ComputeShiftPoints(drive, engine, upshift, downshift);
    int iteration = 0;
    bool shifting = false;
    float *threshold = &upshift[2];
    float *ratio = &drive->ratios[2];
    bool done;
    do {
        if (iteration >= 999999) return;
        double inverse = 1.0 / radius;
        float inverseRadius = (float)inverse;
        float rpm = (float)((((inverse * speed) * *ratio) * finalDrive) * drivetrainValueScale);
        double force;
        if (!shifting && (double)drivetrainInitialRange + *threshold <= rpm) {
            shifting = true;
            clutch = (float)((double)physicsPerformanceStep * drivetrainModeCoefficients[engaged ? 0 : 2] + clutch);
            if (!(clutch <= 0.0f)) {
                if (clutch >= 1.0f) {
                    shifting = false;
                    ++threshold;
                    clutch = 1.0f;
                    engaged = 1;
                    ++ratio;
                }
                force = 0.0;
            } else {
                force = 0.0;
                clutch = 0.0f;
                engaged = 0;
            }
        } else {
            shifting = false;
            force = ((((double)curve.Lookup(rpm) * *ratio) * finalDrive) * inverseRadius) / mass;
        }
        double change = (force - (((double)speed * record30.aero260.coefficient0) * speed) / record30.body110.mass0)
                * physicsPerformanceStep;
        time = (float)((double)physicsPerformanceStep + time);
        distance = (float)(((physicsUpgradeRounding * change) + speed) * physicsPerformanceStep + distance);
        speed = (float)(change + speed);
        if (value14 == 0.0f && (double)speed * hudSpeedScaleImperial78426C >= physicsPerformanceMph60)
            value14 = (float)((double)time * physicsPerformanceSixtyScale[value0]);
        if (value18 == 0.0f && (double)speed * hudSpeedScaleImperial78426C >= physicsPerformanceMph100)
            value18 = (float)((double)time * physicsPerformanceHundredScale[value0]);
        if (value1C == 0.0f && distance >= physicsPerformanceDistance) {
            value1C = time;
            value20 = speed;
        }
        done = !(value14 == 0.0f || value18 == 0.0f || value1C == 0.0f || value20 == 0.0f);
        ++iteration;
    } while (!done);
}

// 0x5b4dc0 composes a car's physics record: stock build from its CarTypeInfo
// row (low byte of `flag`), extras, active tuning (0x5b4780), the drive
// curve, then, unless row +0x844 is set, a drive-curve score in 1.341 steps
// up to the first drag crossing that selects class 0..3 by thresholds
// 500/401/273/0 (optionally followed by 0x5b06a0). Mode 3, 5/10 and 2 apply
// their record variants and rebuild the drive curve.
void PhysicsParameters::Compose(int mode, int refresh, int flag) {
    if (value0 == -1) return;
    unsigned char *row = carTypeInfoTable + value0 * 0x890;
    PhysicsUpgradeSource *source = reinterpret_cast<PhysicsUpgradeSource *>(row + 0x110);
    source->BuildRecord(&record30, &selection2E0, (char)flag);
    source->ApplyExtra(&record30, &selection2E0);
    if (tuning374.CountActive() > 0) tuning374.ApplyAll();
    BuildPhysicsDriveCurve(&driveCurve55C, this);
    if (*reinterpret_cast<int *>(row + 0x844) == 0) {
        float score = 0.0f;
        float speed = driveCurve55C.start;
        while (speed <= driveCurve55C.end) {
            double force = driveCurve55C.Lookup(speed);
            double drag = (((double)speed * record30.aero260.coefficient0) * speed) / record30.body110.mass0;
            if (force < drag) break;
            score = (float)((force - drag) * physicsSpeedSearchStep + score);
            speed = (float)((double)physicsSpeedSearchStep + speed);
        }
        value8 = score;
        signed char index = 3;
        for (int i = 3; i >= 0; --i) {
            if (score > physicsClassThresholds[i]) {
                index = (signed char)(3 - i);
                break;
            }
        }
        index4 = index;
        if ((char)refresh) SimulatePerformance();
    } else {
        index4 = 3;
    }
    if (mode == 3) {
        ApplyPhysicsVariant3(&record30, &selection2E0);
        BuildPhysicsDriveCurve(&driveCurve55C, this);
    } else if (mode == 10 || mode == 5) {
        ApplyPhysicsVariant10(&record30, &selection2E0);
        BuildPhysicsDriveCurve(&driveCurve55C, this);
    } else if (mode == 2) {
        ApplyPhysicsVariant2(&record30, &selection2E0);
        BuildPhysicsDriveCurve(&driveCurve55C, this);
    }
}
