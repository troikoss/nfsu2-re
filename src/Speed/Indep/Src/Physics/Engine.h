#ifndef UG2_ENGINE_H
#define UG2_ENGINE_H

#include "Drivetrain.h"

// Original 32-bit curve layout. Coordinate units come from its consumer.
struct EngineCurve {
    int count;
    float start;
    float end;
    float inverseSpacing;
    float *samples;
    float Lookup(float input);
};

struct EngineSpec {
    float value0;
    float value4;
    float value8;
    float divisorC;
    float samples10[9];
    float samples34[9];
    float samples58[3];
};

struct EngineAttributeRecord {
    char unidentified0[0x2ac];
    float coefficient2AC;
};

struct EngineAttributeOwner {
    EngineAttributeRecord *record;
};

struct Engine {
    void *vtable0;
    char unidentified4[8];
    char name0C[8];
    float state14;
    EngineSpec *spec;
    EngineAttributeOwner *owner1C;
    Drivetrain *drivetrain20;
    EngineCurve curves24[4];
    int flag74;
    float input78;
    float feedback7C;
    float control80;
    int index84;
    unsigned char flag88;
    char unidentified89[3];
    float adjustment8C;
    float coefficient90;
    char unidentified94[4];
    float capacity98;
    float value9C;
    int modeA0;
    float sampleA4;
    float timeA8;
    float anchorAC;

    void Construct(EngineSpec *source, EngineAttributeOwner *owner);
    float ComputeDriveValue();
    int WriteDerivative(float *output);
    float GetEffectiveControl();
    void AdvanceAdjustment(float duration);
    void AdvanceFeedback(float duration);
    void UpdateAfterIntegration();
};

extern float engineValueScale;
extern float engineFlagScale;
extern float engineExtraCoefficients[];
extern float engineAdjustmentThreshold;
extern float engineAdjustmentScale;
extern float engineFlagDragScale;
extern float engineRatioOneDragScale;
extern float engineOtherRatioDragScale;
extern float engineModeDivisorScale;
extern float engineAdjustmentUpper;
extern float engineAdjustmentStop;
extern float engineAdjustmentAboveRate;
extern float engineAdjustmentBelowRate;
extern float engineScalarScale;
extern float engineLimiterOffset;
extern float engineLimiterFraction;
extern float engineSubstepDuration;
extern float engineFeedbackDivisor;
extern float engineFeedbackNegativeScale;
extern float engineFeedbackControlThreshold;
extern float engineFeedbackRiseScale;
extern float engineFeedbackCrossingThreshold;
extern float engineFeedbackDuration;
extern float engineFeedbackOscillationScale;
extern float engineFeedbackPhaseScale;
extern float engineFeedbackPhaseOffset;
extern float engineHalfPi;
extern float engineAngleScale;
extern float engineAngleUnit;
extern float engineThreeHalfPi;
extern float engineTwoPi;
extern float enginePi;
extern float engineSineC3;
extern float engineSineC5;
extern float engineSineC7;
extern float engineSineC9;
float EngineSpecMaximum(EngineSpec *spec);
float EngineMinimum(float a, float b);
float EngineMaximum(float a, float b); // 0x401050
float EngineClamp(float value, float minimum, float maximum);
float EngineSineUnits(unsigned int units);
float EngineSine(float angle);

#endif
