#ifndef UG2_DRIVE_FEEDBACK_CURVE_H
#define UG2_DRIVE_FEEDBACK_CURVE_H

// Observed pointer/count pair-table layout; original type names unresolved.
struct DriveFeedbackPair {
    float coordinate;
    float value;
};

struct DriveFeedbackCurve {
    DriveFeedbackPair *pairs;
    int count;
    void Evaluate(float *output, float input);
    float Lookup(float input);
};

void __stdcall InterpolateDriveFeedback(float *output, float *next,
                                        float *previous, float fraction);

extern float pairCurveEpsilon;
extern DriveFeedbackCurve moverOpposingForceCurve;

#endif
