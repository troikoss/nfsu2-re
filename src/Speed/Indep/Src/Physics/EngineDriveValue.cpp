#include "Engine.h"

// Original PC v1.2 0x594300. Field names are offset-based pending provenance.
float Engine::ComputeDriveValue() {
    float base = curves24[0].Lookup(state14 * engineValueScale) * coefficient90;
    if (flag74) {
        base *= engineFlagScale;
    }
    float result = base * (flag88 ? 0.0f : control80 * input78);
    if (index84 > 0) {
        float extraCoefficient = engineExtraCoefficients[index84];
        float extra = curves24[3].Lookup(state14 * engineValueScale) * owner1C->record->coefficient2AC * extraCoefficient;
        result += extra * (flag88 ? 0.0f : control80 * input78);
    }
    if (adjustment8C > engineAdjustmentThreshold) {
        float bounded = adjustment8C;
        if (bounded > 1.0f) {
            bounded = 1.0f;
        }
        result *= 1.0f - (bounded - engineAdjustmentThreshold) /
                            (1.0f - engineAdjustmentThreshold) * engineAdjustmentScale;
    }
    float control = flag88 ? 0.0f : control80 * input78;
    float drag = -(curves24[1].Lookup(state14 * engineValueScale) * (1.0f - control));
    if (flag88) {
        drag *= engineFlagDragScale;
    }
    float value = drag + result;
    if (value < 0.0f && !flag88) {
        value *= drivetrain20->ratioIndex == 1 ? engineRatioOneDragScale : engineOtherRatioDragScale;
    }
    return value;
}
