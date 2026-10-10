#include "Engine.h"
#include "DriveFeedbackCurve.h"

extern "C" double __cdecl fabs(double);
#pragma intrinsic(fabs)
extern DriveFeedbackCurve drivetrainFinishingCurve;

// Original PC v1.2 0x5aa830. Includes synchronization and ratio-2 curve feedback.
void Drivetrain::FinishDriveCoupling() {
    Engine *engine = (Engine *)component20;
    if (ratioIndex != 1 && mode44 == 3 && unidentified34->value3E4 > 0) {
        float difference = GetWheelDerivedValue() - engine->state14;
        if (engine->input78 > drivetrainFinishingInputThreshold
                && GetAverageWheelScalar() > drivetrainFinishingWheelThreshold) {
            wheelValue4C = engine->state14;
            SynchronizeWheelValues(engine->state14);
        } else {
            float bound = (float)fabs(engine->ComputeDriveValue());
            engine->feedback7C += EngineClamp(difference * spec->coefficient14, -bound, bound);
        }
    }
    if (ratioIndex == 2) {
        float drive = engine->ComputeDriveValue();
        float sum = engine->feedback7C + drive;
        if (engine->feedback7C < drive && sum < 0.0f) {
            float factor;
            drivetrainFinishingCurve.Evaluate(&factor, sum);
            engine->feedback7C += factor * drivetrainFinishingCoefficients[indexF0] * sum;
        }
    }
}
