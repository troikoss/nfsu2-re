#include "Engine.h"
#include "../../Libs/bWare/Strings.h"

extern float engineCurveScale9;
extern float engineCurveScale3;
extern float engineAdditionalSamples[];
extern int drivetrainComputedShifts;
extern float drivetrainShiftStep;
extern float drivetrainShiftMargin;
extern float drivetrainTopShiftMargin;
extern float drivetrainRatioEpsilon;
extern float drivetrainInitialRange;
extern float drivetrainInitialRange1;
extern float drivetrainInitialRange2;
extern float drivetrainInitialRange3;

// Original 0x457b00: largest of zero and the nine samples at spec +0x34.
float EngineSpecMaximum(EngineSpec *spec) {
    float maximum = 0.0f;
    if (spec->samples34[0] > 0.0f) {
        maximum = spec->samples34[0];
    }
    for (int i = 1; i < 9; ++i) {
        if (maximum < spec->samples34[i]) {
            maximum = spec->samples34[i];
        }
    }
    return maximum;
}

// Constructor body of original 0x5aa5a0 ("Engine"); vtable 0x7a235c is installed by the caller.
void Engine::Construct(EngineSpec *source, EngineAttributeOwner *owner) {
    float end = source->value8;
    float start = source->value0;
    curves24[0].start = start;
    curves24[0].count = 9;
    curves24[0].end = end;
    curves24[0].samples = source->samples10;
    curves24[0].inverseSpacing = engineCurveScale9 / (end - start);
    end = source->value8;
    start = source->value0;
    curves24[1].samples = source->samples58;
    curves24[1].start = start;
    curves24[1].count = 3;
    curves24[1].end = end;
    curves24[1].inverseSpacing = engineCurveScale3 / (end - start);
    end = source->value8;
    start = source->value0;
    curves24[2].count = 9;
    curves24[2].start = start;
    curves24[2].samples = source->samples34;
    curves24[2].end = end;
    curves24[2].inverseSpacing = engineCurveScale9 / (end - start);
    end = source->value4;
    start = source->value0;
    curves24[3].count = 9;
    curves24[3].start = start;
    curves24[3].samples = engineAdditionalSamples;
    curves24[3].end = end;
    curves24[3].inverseSpacing = engineCurveScale9 / (end - start);
    owner1C = owner;
    spec = source;
    flag74 = 0;
    index84 = 0;
    adjustment8C = -1.0f;
    *reinterpret_cast<int *>(unidentified94) = 0;
    control80 = 1.0f;
    coefficient90 = 1.0f;
    flag88 = 0;
    value9C = 0.0f;
    modeA0 = 0;
    timeA8 = 0.0f;
    anchorAC = 0.0f;
    sampleA4 = 0.0f;
    capacity98 = EngineSpecMaximum(source);
    drivetrain20 = 0;
    nfs_strcpy(name0C, "Engine");
}

// Original 0x5ac7d0: shift points from the torque curve and adjacent gear ratios.
void ComputeShiftPoints(DrivetrainSpec *spec, EngineSpec *engine, float *upshift, float *downshift) {
    EngineCurve curve;
    curve.end = engine->value8;
    curve.start = engine->value0;
    curve.count = 9;
    curve.samples = engine->samples10;
    curve.inverseSpacing = engineCurveScale9 / (curve.end - curve.start);
    upshift[1] = (engine->value4 - engine->value0) * spec->shiftFraction0 + engine->value0;
    int gear = 2;
    int limit = spec->unidentified18 + 1;
    for (; gear < limit; ++gear) {
        float ratio = spec->ratios[gear];
        float next = spec->ratios[gear + 1];
        float point;
        if (drivetrainComputedShifts) {
            volatile float speed = (engine->value4 + engine->value0) * 0.5f;
            if (speed < engine->value4) {
                volatile float magnitude = ratio < 0.0f ? -ratio : ratio;
                for (;;) {
                    volatile float current = curve.Lookup(speed);
                    float shifted;
                    if (magnitude > drivetrainRatioEpsilon) {
                        volatile float inverse = 1.0f / ratio;
                        shifted = curve.Lookup(inverse * speed * next) * inverse * next;
                    } else {
                        shifted = 0.0f;
                    }
                    if (shifted > current) {
                        point = speed;
                        goto store;
                    }
                    speed = drivetrainShiftStep + speed;
                    if (!(speed < engine->value4)) {
                        break;
                    }
                }
            }
        }
        point = engine->value4 - drivetrainShiftMargin;
    store:
        upshift[gear] = point;
        float magnitude = ratio < 0.0f ? -ratio : ratio;
        if (magnitude > drivetrainRatioEpsilon) {
            downshift[gear + 1] = next / ratio * upshift[gear];
        } else {
            downshift[gear + 1] = 0.0f;
        }
    }
    upshift[gear] = engine->value8 + drivetrainTopShiftMargin;
}

// Constructor body of original 0x5af9d0 ("Drvtrn"); vtable 0x7a2508 is installed by the caller.
void Drivetrain::Construct(DrivetrainSpec *source, DrivetrainTuning *tuning, DrivetrainComponent *engine,
                           DrivetrainWheel **wheelList, DrivetrainAuxiliary *auxiliary) {
    unidentified1C = tuning;
    spec = source;
    indexF0 = 3;
    component20 = engine;
    wheels[0] = wheelList[0];
    wheels[1] = wheelList[1];
    wheels[2] = wheelList[2];
    wheels[3] = wheelList[3];
    unidentified34 = auxiliary;
    ratioIndex = 1;
    value38 = 1;
    state14 = 0.0f;
    mode44 = 2;
    pending48 = 0;
    previous58 = 0.0f;
    coefficientEC = spec->selector10;
    status60 = 0;
    timer5C = 0.0f;
    timer64 = 0.0f;
    adjustment68 = 0.0f;
    value6C = 0.0f;
    ComputeShiftPoints(source, reinterpret_cast<Engine *>(engine)->spec, upshift84, downshiftA8);
    RefreshVariations();
    upshift84[1] = unidentified1C->shiftScale10 * upshift84[1];
    rangesCC[0] = drivetrainInitialRange;
    rangesCC[1] = drivetrainInitialRange1;
    rangesCC[2] = drivetrainInitialRange2;
    rangesCC[3] = drivetrainInitialRange3;
    rangesCC[4] = drivetrainInitialRange2;
    rangesCC[5] = drivetrainInitialRange3;
    coefficientE4 = 1.0f;
    coefficientE8 = 1.0f;
    flag74 = 0;
    nfs_strcpy(name0C, "Drvtrn");
}
