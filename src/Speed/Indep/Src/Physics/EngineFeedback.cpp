#include "Engine.h"

extern "C" double __cdecl fabs(double);
#pragma intrinsic(fabs)

// Original PC v1.2 0x5a0f30. State meaning remains partly unresolved.
void Engine::AdvanceFeedback(float duration) {
    if (!(capacity98 > 0.0f) || duration == 0.0f) {
        return;
    }
    float converted = state14 * engineValueScale;
    float baseline = capacity98 / engineFeedbackDivisor;
    float negative = -(baseline * engineFeedbackNegativeScale);
    switch (modeA0) {
    case 0:
        if (GetEffectiveControl() >= engineFeedbackControlThreshold) {
            modeA0 = 1;
        } else {
            value9C = EngineMinimum(1.0f, (converted - spec->value0) / (spec->value8 - spec->value0) * 2.0f) * negative;
        }
        break;
    case 1:
        if (GetEffectiveControl() >= engineFeedbackControlThreshold) {
            value9C = EngineMinimum(baseline, value9C + EngineMinimum(1.0f, GetEffectiveControl()) *
                       ((converted - spec->value0) / (spec->value8 - spec->value0)) * engineFeedbackRiseScale * duration);
        } else if (!flag88) {
            modeA0 = 2;
            timeA8 = duration;
            sampleA4 = value9C;
        }
        break;
    case 2:
        {
            float phase = timeA8 + engineFeedbackPhaseOffset;
            float square = phase * phase;
            float oscillation = EngineSine(square * square * engineFeedbackPhaseScale + engineHalfPi);
            float next = EngineClamp(sampleA4 / baseline / timeA8 * engineFeedbackOscillationScale * oscillation + negative,
                                     negative * 2.0f, sampleA4);
            bool done = false;
            if (anchorAC == 0.0f) {
                anchorAC = next;
            } else if ((next > value9C && next < anchorAC) || (next < value9C && next > anchorAC)) {
                if (fabs(value9C - anchorAC) < engineFeedbackCrossingThreshold) {
                    done = true;
                } else {
                    anchorAC = value9C;
                }
            }
            timeA8 += duration;
            value9C = next;
            if (done || timeA8 > engineFeedbackDuration) {
                modeA0 = 0;
                anchorAC = 0.0f;
            }
        }
        break;
    }
}
