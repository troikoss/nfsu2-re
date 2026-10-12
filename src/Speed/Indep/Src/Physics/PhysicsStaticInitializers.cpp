#include "DriveFeedbackCurve.h"
#include "Engine.h"

// C++ static initializers from the CRT table at 0x7e8264 that write vehicle-physics
// globals. Values are instruction immediates; referenced tables remain original data.
// The original runs them as separate global constructors in table order; this routine
// keeps that order for the objects that live at their original addresses.

void ConstructDriveFeedbackCurve(DriveFeedbackCurve *curve, DriveFeedbackPair *pairs, int count);

extern DriveFeedbackCurve pairCurve86B3C0;
extern DriveFeedbackCurve pairCurve86B590;
extern DriveFeedbackCurve pairCurve86B578;
extern DriveFeedbackCurve pairCurve86B498;
extern DriveFeedbackCurve yawAssistCurveB;
extern DriveFeedbackCurve yawAssistCurveA;
extern DriveFeedbackCurve pairCurve89CF24;
extern DriveFeedbackCurve pairCurve89CF30;
extern DriveFeedbackCurve pairCurve89E790;
extern DriveFeedbackCurve pairCurve89E824;
extern DriveFeedbackCurve pairCurve89CF18;
extern DriveFeedbackCurve pairCurve890150;
extern DriveFeedbackCurve pairCurve89E798;
extern DriveFeedbackCurve moverEventCurve;
extern DriveFeedbackCurve guideCurve;
extern DriveFeedbackCurve steeringRecoveryCurve;
extern DriveFeedbackPair pairTable7E9F58[];
extern DriveFeedbackPair pairTable7FC880[];
extern DriveFeedbackPair pairTable7FC8A0[];
extern DriveFeedbackPair pairTable7FC8C8[];
extern DriveFeedbackPair pairTable7FC8E0[];
extern DriveFeedbackPair pairTable7FC8F8[];
extern DriveFeedbackPair yawAssistTableB[7];
extern DriveFeedbackPair yawAssistTableA[7];
extern DriveFeedbackPair pairTable8023A0[];
extern DriveFeedbackPair pairTable8023D0[];
extern DriveFeedbackPair pairTable802404[];
extern DriveFeedbackPair pairTable802414[];
extern DriveFeedbackPair pairTable802424[];
extern DriveFeedbackPair pairTable802434[];
extern DriveFeedbackPair pairTable802444[];
extern DriveFeedbackPair pairTable802570[];
extern DriveFeedbackPair pairTable802590[];
extern float physicsValue86B4A8;
extern float physicsValue86B4BC;
extern float physicsDivisor7A17BC;
extern float lateralFallbackEnd86B510;
extern float lateralFallbackScale7859D0;
extern EngineCurve wheelLateralFallbackCurve;
extern float physicsValue89CF08;
extern float physicsDivisor7A5470;

// Original 0x576770: pair-table curve constructor.
void ConstructDriveFeedbackCurve(DriveFeedbackCurve *curve, DriveFeedbackPair *pairs, int count) {
    curve->pairs = pairs;
    curve->count = count;
}

// Reinterprets an instruction immediate as its binary32 value.
static float Bits(unsigned int bits) {
    union {
        unsigned int integer;
        float real;
    } value;
    value.integer = bits;
    return value.real;
}

static void SetPair(DriveFeedbackPair *pair, float coordinate, float value) {
    pair->coordinate = coordinate;
    pair->value = value;
}

void RunPhysicsStaticInitializers() {
    // 0x77a7e0: steering limit recovery curve read by 0x405dc0.
    ConstructDriveFeedbackCurve(&steeringRecoveryCurve, pairTable7E9F58, 4);
    // 0x77de60
    physicsValue86B4BC = Bits(0x3f800000) / physicsDivisor7A17BC;
    // 0x77de80, 0x77dea0, 0x77df40, 0x77df60
    ConstructDriveFeedbackCurve(&pairCurve86B3C0, pairTable7FC880, 4);
    ConstructDriveFeedbackCurve(&pairCurve86B590, pairTable7FC8A0, 4);
    ConstructDriveFeedbackCurve(&pairCurve86B578, pairTable7FC8C8, 3);
    ConstructDriveFeedbackCurve(&pairCurve86B498, pairTable7FC8E0, 3);
    // 0x77df80
    physicsValue86B4A8 = Bits(0x3db2b8c2);
    // 0x77df90
    ConstructDriveFeedbackCurve(&moverOpposingForceCurve, pairTable7FC8F8, 3);
    // 0x77dfb0: seven points; angle coordinates in radians.
    SetPair(&yawAssistTableB[0], Bits(0x00000000), Bits(0x00000000));
    SetPair(&yawAssistTableB[1], Bits(0x3e860a92), Bits(0x3dcccccd));
    SetPair(&yawAssistTableB[2], Bits(0x3f060a92), Bits(0x3ee66666));
    SetPair(&yawAssistTableB[3], Bits(0x3f490fdb), Bits(0x3f59999a));
    SetPair(&yawAssistTableB[4], Bits(0x3f860a92), Bits(0x3f733333));
    SetPair(&yawAssistTableB[5], Bits(0x3fc6d3f2), Bits(0x3f933333));
    SetPair(&yawAssistTableB[6], Bits(0x3fc90fdb), Bits(0x3f800000));
    // 0x77e040
    SetPair(&yawAssistTableA[0], Bits(0x00000000), Bits(0x00000000));
    SetPair(&yawAssistTableA[1], Bits(0x3e860a92), Bits(0x3dcccccd));
    SetPair(&yawAssistTableA[2], Bits(0x3f060a92), Bits(0x3ee66666));
    SetPair(&yawAssistTableA[3], Bits(0x3f490fdb), Bits(0x3f59999a));
    SetPair(&yawAssistTableA[4], Bits(0x3f860a92), Bits(0x3f733333));
    SetPair(&yawAssistTableA[5], Bits(0x3fc6d3f2), Bits(0x3f933333));
    SetPair(&yawAssistTableA[6], Bits(0x3fc90fdb), Bits(0x00000000));
    // 0x77e0d0, 0x77e0f0
    ConstructDriveFeedbackCurve(&yawAssistCurveB, yawAssistTableB, 7);
    ConstructDriveFeedbackCurve(&yawAssistCurveA, yawAssistTableA, 7);
    // 0x77e120, 0x77e130: lateral fallback curve end and inverse spacing.
    lateralFallbackEnd86B510 = Bits(0x3e64c388);
    wheelLateralFallbackCurve.end = lateralFallbackEnd86B510;
    wheelLateralFallbackCurve.inverseSpacing = lateralFallbackScale7859D0 / wheelLateralFallbackCurve.end;
    // 0x77eb00
    physicsValue89CF08 = Bits(0x3f800000) / physicsDivisor7A5470;
    // 0x77ec80 .. 0x77ed80
    ConstructDriveFeedbackCurve(&pairCurve89CF24, pairTable8023A0, 4);
    ConstructDriveFeedbackCurve(&pairCurve89CF30, pairTable8023D0, 4);
    ConstructDriveFeedbackCurve(&pairCurve89E790, pairTable802404, 2);
    ConstructDriveFeedbackCurve(&pairCurve89E824, pairTable802414, 2);
    ConstructDriveFeedbackCurve(&pairCurve89CF18, pairTable802424, 2);
    ConstructDriveFeedbackCurve(&pairCurve890150, pairTable802434, 2);
    ConstructDriveFeedbackCurve(&pairCurve89E798, pairTable802444, 2);
    ConstructDriveFeedbackCurve(&moverEventCurve, pairTable802570, 4);
    ConstructDriveFeedbackCurve(&guideCurve, pairTable802590, 3);
}
