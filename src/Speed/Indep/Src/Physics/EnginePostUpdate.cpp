#include "Engine.h"

// Original PC v1.2 0x5aa760. Do not replace the original limiter hysteresis.
void Engine::UpdateAfterIntegration() {
    float maximum = spec->value8 * engineScalarScale;
    float value = spec->value0 * engineScalarScale;
    if (state14 > value) {
        value = state14;
    }
    if (maximum < value) {
        value = maximum;
    }
    state14 = value;
    float converted = value * engineValueScale;
    feedback7C = 0.0f;
    if (flag88) {
        if (converted < spec->value8 - engineLimiterFraction * spec->value8) {
            flag88 = 0;
        }
    } else if (converted > engineLimiterOffset + spec->value8) {
        flag88 = 1;
    }
    float duration = engineSubstepDuration;
    AdvanceFeedback(duration);
    AdvanceAdjustment(duration);
}
