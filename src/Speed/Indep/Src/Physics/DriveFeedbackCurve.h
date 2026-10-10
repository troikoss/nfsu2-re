#ifndef UG2_DRIVE_FEEDBACK_CURVE_H
#define UG2_DRIVE_FEEDBACK_CURVE_H

// Original pair table at 0x7fc8c0 used by drivetrain finishing.
struct DriveFeedbackPair {
    float coordinate;
    float value;
};

struct DriveFeedbackCurve {
    DriveFeedbackPair *pairs;
    int count;
    void Evaluate(float *output, float input);
};

void __stdcall InterpolateDriveFeedback(float *output, float *next,
                                        float *previous, float fraction);

#endif
