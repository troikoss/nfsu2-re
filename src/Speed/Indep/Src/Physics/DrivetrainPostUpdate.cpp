#include "Drivetrain.h"

// PC v1.2 0x5a1400. Mode names and component units await further provenance.
void Drivetrain::UpdateAfterIntegration() {
    wheelValue4C = GetWheelDerivedValue();
    if (status60 == 3) {
        component20->control80 = 1.0f;
        if (mode44 == 3) {
            value6C = component20->value14 * drivetrainValueScale;
            if (adjustment68 > 0.0 && timer64 == 0.0f) {
                timer64 = drivetrainClock * drivetrainClockScale;
            }
        } else if (mode44 == 1 && pending48) {
            pending48 = 0;
            ratioIndex = value38;
        }
    } else {
        switch (mode44) {
        case 0:
            component20->control80 = flag74 ? 1.0f : 0.0f;
            if (state14 <= 0.0f) {
                state14 = 0.0f;
                mode44 = 1;
            }
            if (flag74) {
                mode44 = 1;
            }
            break;
        case 1:
            component20->control80 = flag74 ? 1.0f : 0.0f;
            ratioIndex = value38;
            if (pending48 || !IsBelowComponentLimit()) {
                if (!flag74) {
                    mode44 = 2;
                }
                RefreshVariations();
            }
            break;
        case 2:
            component20->control80 = flag74 ? 1.0f : drivetrainControlScale;
            if (state14 >= 1.0f) {
                state14 = 1.0f;
                if (IsWithinComponentTolerance() || ratioIndex == 1) {
                    mode44 = 3;
                }
            }
            if (IsBelowComponentLimit() && ratioIndex != 1) {
                mode44 = 0;
            }
            if (flag74) {
                mode44 = 1;
            }
            break;
        case 3:
            if (adjustment68 > 0.0f && timer64 == 0.0f) {
                timer64 = drivetrainClock * drivetrainClockScale;
            }
            component20->control80 = 1.0f;
            {
                int previousRatio = ratioIndex;
                if (previousRatio != 1 && IsBelowComponentLimit()) {
                    mode44 = 0;
                }
                if (previousRatio != value38) {
                    pending48 = 1;
                    value6C = component20->value14 * drivetrainValueScale;
                    mode44 = 0;
                } else {
                    pending48 = 0;
                }
            }
            if (flag74) {
                mode44 = 1;
            }
            break;
        }
    }
    if (timer5C != 0.0f) {
        component20->control80 = 0.0f;
        if (drivetrainClock * drivetrainClockScale - timer5C >= drivetrainTimerLimit) {
            timer5C = 0.0f;
        }
        return;
    }
    if (mode44 == 3 && timer64 > 0.0f) {
        float adjustment = (drivetrainAdjustmentDuration - (drivetrainClock * drivetrainClockScale - timer64)) /
                           drivetrainAdjustmentDuration * adjustment68 * drivetrainAdjustmentScale;
        if (adjustment > 0.0f) {
            component20->control80 = adjustment + 1.0f;
        }
        if (drivetrainClock * drivetrainClockScale - timer64 >= drivetrainAdjustmentDuration) {
            timer64 = 0.0f;
            adjustment68 = 0.0f;
        }
    }
}
