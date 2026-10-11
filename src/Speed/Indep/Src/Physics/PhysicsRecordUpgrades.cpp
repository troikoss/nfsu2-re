#include "PhysicsUpgradeSource.h"
#include "VehicleMover.h"

extern float physicsUpgradeGradeScale;
extern float physicsUpgradeRounding;

// The offset accesses below retain the original unknown tuning fields. Double
// intermediates preserve the unspilled x87 arithmetic at 53-bit precision.
void PhysicsUpgradeSource::ApplyUpgrade0(CarPhysicsRecord *record, PhysicsUpgradeSelection *selection) {
    float weight;
    double fraction = selection->GetFraction(0, 1, &weight);
    if (fraction != 0.0) {
        float *source = reinterpret_cast<float *>(this);
        record->centre0.x = (float)((double)source[0x344 / 4] * fraction + record->centre0.x);
        record->body110.inertiaScale50 = (float)(((double)source[0x348 / 4] - record->body110.inertiaScale50) * fraction + record->body110.inertiaScale50);
        record->body110.mass0 = (float)((((double)source[0x340 / 4] - 1.0) * fraction + 1.0) * record->body110.mass0);
    }
}

void PhysicsUpgradeSource::ApplyUpgrade1(CarPhysicsRecord *record, PhysicsUpgradeSelection *selection) {
    float weights[3];
    double fraction = selection->GetFraction(1, 3, weights);
    if (fraction != 0.0) {
        float *source = reinterpret_cast<float *>(this);
        int index = (int)((double)weights[2] * physicsUpgradeGradeScale + physicsUpgradeRounding);
        if (index >= 1 && index <= 3) {
            DrivetrainSpec *variant = reinterpret_cast<DrivetrainSpec *>(values0 + 0x310 + index * 0x40);
            DrivetrainSpec *drive = &record->drivetrain1B0;
            drive->coefficient8 = variant->coefficient8;
            drive->coefficientC = variant->coefficientC;
            drive->selector10 = variant->selector10;
            drive->shiftFraction0 = variant->shiftFraction0;
            *reinterpret_cast<int *>(drive->unidentified4) = *reinterpret_cast<int *>(variant->unidentified4);
            for (int i = 0; i < 8; ++i) drive->ratios[i] = variant->ratios[i];
            variant->unidentified18 = 0;
            for (int i = 2; i < 8; ++i) {
                if (variant->ratios[i] == 0.0f) break;
                ++variant->unidentified18;
            }
            drive->unidentified18 = variant->unidentified18;
        }
        record->drivetrain1B0.coefficient1C = (float)(((double)source[0x3ec / 4] - record->drivetrain1B0.coefficient1C) * weights[0] + record->drivetrain1B0.coefficient1C);
        record->drivetrain1B0.coefficient14 = (float)(((double)source[0x3e4 / 4] - record->drivetrain1B0.coefficient14) * weights[0] + record->drivetrain1B0.coefficient14);
        reinterpret_cast<EngineSpec *>(record->engine1F0)->divisorC = (float)((double)source[0x41c / 4] * weights[1] + reinterpret_cast<EngineSpec *>(record->engine1F0)->divisorC);
    }
}

void PhysicsUpgradeSource::ApplyUpgrade3(CarPhysicsRecord *record, PhysicsUpgradeSelection *selection) {
    float weight;
    double fraction = selection->GetFraction(3, 1, &weight);
    if (fraction != 0.0) {
        float *source = reinterpret_cast<float *>(this);
        EngineSpec *engine = reinterpret_cast<EngineSpec *>(record->engine1F0);
        for (int i = 0; i < 9; ++i) engine->samples10[i] = (float)((double)source[(0x420 / 4) + i] * fraction + engine->samples10[i]);
        for (int i = 0; i < 3; ++i) engine->samples58[i] = (float)((double)source[(0x444 / 4) + i] * fraction + engine->samples58[i]);
    }
}

// No zero-fraction early return: the old secondary curve is still added into
// the primary curve. Each secondary write occurs before the primary write,
// using the unrounded interpolated value for both.
void PhysicsUpgradeSource::ApplyUpgrade4(CarPhysicsRecord *record, PhysicsUpgradeSelection *selection) {
    float weight;
    double fraction = selection->GetFraction(4, 1, &weight);
    float *source = reinterpret_cast<float *>(this);
    EngineSpec *engine = reinterpret_cast<EngineSpec *>(record->engine1F0);
    for (int i = 0; i < 9; ++i) {
        double value = ((double)source[(0x510 / 4) + i] - engine->samples34[i]) * fraction + engine->samples34[i];
        engine->samples34[i] = (float)value;
        engine->samples10[i] = (float)(value + engine->samples10[i]);
    }
}

void PhysicsUpgradeSource::ApplyUpgrade5(CarPhysicsRecord *record, PhysicsUpgradeSelection *selection) {
    float weights[3];
    double fraction = selection->GetFraction(5, 3, weights);
    if (fraction != 0.0) {
        float *source = reinterpret_cast<float *>(this);
        for (int axle = 0; axle < 2; ++axle) {
            float *variant = source + (0x5c0 / 4) + axle * 8;
            SuspensionSpec *spec = &record->suspension170[axle];
            spec->rest1C = (float)(((double)variant[7] - spec->rest1C) * weights[0] + spec->rest1C);
            spec->progression0 = (float)(((double)variant[0] - spec->progression0) * weights[0] + spec->progression0);
            spec->stiffness4 = (float)(((double)variant[1] - spec->stiffness4) * weights[0] + spec->stiffness4);
            spec->upper14 = (float)(((double)variant[5] - spec->upper14) * weights[0] + spec->upper14);
            spec->damping8[0] = (float)(((double)variant[2] - spec->damping8[0]) * weights[0] + spec->damping8[0]);
            spec->lower18 = (float)(((double)variant[6] - spec->lower18) * weights[0] + spec->lower18);
            spec->damping8[1] = (float)(((double)variant[3] - spec->damping8[1]) * weights[0] + spec->damping8[1]);
            spec->antiRoll10 = (float)(((double)variant[4] - spec->antiRoll10) * weights[1] + spec->antiRoll10);
        }
        record->wheels10[0].position0.z = (float)((double)weights[0] * source[0x604 / 4] + record->wheels10[0].position0.z);
        record->wheels10[1].position0.z = (float)((double)weights[0] * source[0x604 / 4] + record->wheels10[1].position0.z);
        record->wheels10[2].position0.z = (float)((double)weights[0] * source[0x608 / 4] + record->wheels10[2].position0.z);
        record->wheels10[3].position0.z = (float)((double)weights[0] * source[0x608 / 4] + record->wheels10[3].position0.z);
        record->centre0.z = (float)((double)record->centre0.z - (double)source[0x600 / 4] * weights[0]);
        float *value270 = reinterpret_cast<float *>(&record->preparation270);
        *value270 = (float)(((double)source[0x5bc / 4] - *value270) * weights[2] + *value270);
    }
}

void PhysicsUpgradeSource::ApplyUpgrade6(CarPhysicsRecord *record, PhysicsUpgradeSelection *selection) {
    float weight;
    double fraction = selection->GetFraction(6, 1, &weight);
    if (fraction != 0.0) {
        float *source = reinterpret_cast<float *>(this);
        record->brakeBias27C = (float)(((double)source[0x5b8 / 4] - record->brakeBias27C) * fraction + record->brakeBias27C);
        record->preparation270.coefficient8 = (float)(((double)source[0x5b4 / 4] - record->preparation270.coefficient8) * fraction + record->preparation270.coefficient8);
        record->preparation270.coefficient4 = (float)(((double)source[0x5b0 / 4] - record->preparation270.coefficient4) * fraction + record->preparation270.coefficient4);
    }
}

void PhysicsUpgradeSource::ApplyUpgrade7(CarPhysicsRecord *record, PhysicsUpgradeSelection *selection) {
    float weight;
    double fraction = selection->GetFraction(7, 1, &weight);
    if (selection->selected50[45] || selection->selected50[46] || selection->selected50[51]) record->value290 = 0.0f;
    if (fraction != 0.0) {
        float *source = reinterpret_cast<float *>(this);
        EngineSpec *engine = reinterpret_cast<EngineSpec *>(record->engine1F0);
        int index = (int)((double)selection->values0[7] + physicsUpgradeRounding);
        if (index >= 1 && index <= 3) {
            engine->value0 = (float)((double)source[(0x410 / 4) + index * 16] + engine->value0);
            engine->value4 = (float)((double)source[(0x414 / 4) + index * 16] + engine->value4);
            engine->value8 = (float)((double)source[(0x418 / 4) + index * 16] + engine->value8);
        }
        for (int i = 0; i < 9; ++i) engine->samples10[i] = (float)((double)source[(0x4e0 / 4) + i] * fraction + engine->samples10[i]);
        for (int i = 0; i < 3; ++i) engine->samples58[i] = (float)((double)source[(0x504 / 4) + i] * fraction + engine->samples58[i]);
    }
}

void PhysicsUpgradeSource::ApplyUpgrade8(CarPhysicsRecord *record, PhysicsUpgradeSelection *selection) {
    float weight;
    double fraction = selection->GetFraction(8, 1, &weight);
    if (fraction != 0.0) {
        float *source = reinterpret_cast<float *>(this);
        // Original write order: coefficient8, divisor4, value0, coefficient10,
        // coefficientC for each axle. Preserve it even though the rows are disjoint.
        const int order[5] = {2, 1, 0, 4, 3};
        for (int axle = 0; axle < 2; ++axle) {
            float *spec = reinterpret_cast<float *>(&record->correctionD0[axle].spec);
            for (int i = 0; i < 5; ++i) {
                int field = order[i];
                spec[field] = (float)(((double)source[(0x540 / 4) + axle * 8 + field] - spec[field]) * fraction + spec[field]);
            }
        }
    }
}

void PhysicsUpgradeSource::ApplyUpgrade9(CarPhysicsRecord *record, PhysicsUpgradeSelection *selection) {
    float weights[2];
    double fraction = selection->GetFraction(9, 2, weights);
    if (fraction != 0.0) {
        float *source = reinterpret_cast<float *>(this);
        record->aero260.coefficient4 = (float)(((double)source[0x590 / 4] + source[0x5a0 / 4]) * weights[0] + record->aero260.coefficient4);
        record->aero260.coefficient8 = (float)(((double)source[0x594 / 4] + source[0x5a4 / 4]) * weights[1] + record->aero260.coefficient8);
        record->value26C = (float)(((double)source[0x598 / 4] + source[0x5a8 / 4]) * fraction + record->value26C);
    }
}
