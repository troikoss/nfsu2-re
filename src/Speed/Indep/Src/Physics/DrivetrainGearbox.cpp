#include "Engine.h"
#include "GameModeFlags.h"

extern float engineValueScale;
extern float drivetrainFinishingInputThreshold;
extern float gearboxUnsetSpeed;
extern float gearboxManualThreshold;
extern float gearboxManualRatioScale;
extern float gearboxManualTimeout;
extern float gearboxTopMargin;
extern float gearboxDownshiftMargin;
extern float gearboxMinimumWheelLoad;
extern float gearboxShiftWeights[];
extern float gearboxMatchWindow;
extern float gearboxMatchPartial;
extern float frameDuration865154;
extern int drivetrainClock;
extern float drivetrainClockScale;

// Original 0x594670: shift point for a gear, capped by the engine endpoint.
float Drivetrain::GetShiftPoint(int gear, char useAlternate) {
    float base = gear == 3 ? variation78[2] : variation78[0];
    if (useAlternate) {
        base = rangesCC[0];
    }
    volatile float point = base + upshift84[gear];
    EngineSpec *engine = reinterpret_cast<Engine *>(component20)->spec;
    float limit = IsModeGroup2() ? engine->value8 : engine->value4;
    if (limit < point) {
        return limit;
    }
    return point;
}

// Original 0x594f20: shift-match adjustment from the distance to the upshift point.
float ShiftMatch(Drivetrain *drivetrain, float speed, int gear, char partial) {
    volatile float distance = drivetrain->upshift84[gear] - speed;
    volatile float window = gearboxShiftWeights[drivetrain->indexF0 * 9 + gear] * drivetrain->coefficientE4;
    if (!(window > 0.0f)) {
        return 0.0f;
    }
    if (distance < 0.0f) {
        float magnitude = distance < 0.0f ? -distance : distance;
        if ((1.0f - gearboxMatchWindow) * window <= magnitude) {
            return -1.0f;
        }
        return 1.0f;
    }
    volatile float match = 1.0f - distance / (gearboxMatchWindow * window);
    if (!(0.0f <= match)) {
        return 0.0f;
    }
    if (!(match > 0.0f) || !partial) {
        return match;
    }
    if (match < 0.5f) {
        return 0.5f;
    }
    return gearboxMatchPartial;
}

// Original 0x5a1700.
void Drivetrain::ApplyShiftAdjustment(float engineSpeed) {
    if (IsModeGroup2() && value38 >= ratioIndex) {
        adjustment68 = ShiftMatch(this, engineSpeed, ratioIndex, 1);
    }
}

// Original 0x5aa960: automatic (-1), timed (-2) and explicit gear requests.
int Drivetrain::SelectGear(int request, float engineSpeed) {
    Engine *engine = reinterpret_cast<Engine *>(component20);
    if (!(engineSpeed > gearboxUnsetSpeed)) {
        engineSpeed = engine->state14 * engineValueScale;
    }
    int gear = ratioIndex;
    if (request == -1) {
        value54 = 1;
        if (gear <= 1) {
            value38 = 2;
        } else {
            if (mode44 != 3) {
                return request;
            }
            volatile float up = GetShiftPoint(gear, 0);
            volatile float down = variation78[1] + downshiftA8[gear];
            float lower = 0.0f;
            if (gear != 2) {
                lower = spec->ratios[gear] / spec->ratios[gear - 1] * GetShiftPoint(gear - 1, 1)
                      - gearboxDownshiftMargin;
            }
            if (gear <= 2) {
                if (engine->input78 < drivetrainFinishingInputThreshold) {
                    down = engine->spec->value0 + 100.0f;
                } else {
                    down = 0.0f;
                }
            }
            if (!(engineSpeed < up) && gear < spec->unidentified18 + 1) {
                int ready = 1;
                for (int i = 0; i < 4; ++i) {
                    int grounded = wheels[i]->flag11C && wheels[i]->value118 < gearboxMinimumWheelLoad;
                    ready &= grounded;
                }
                if (ready) {
                    value38 = gear + 1;
                }
            } else if (!(engineSpeed > down) && !(engineSpeed > lower)) {
                value38 = gear - 1;
            }
        }
        if (mode44 == 3 && value38 > gear && gear >= 1) {
            ApplyShiftAdjustment(engineSpeed);
        }
        return request;
    }
    if (request == -2) {
        volatile float timer = frameDuration865154 + value50;
        value50 = timer;
        if (mode44 != 3) {
            return request;
        }
        float bottom = engine->spec->value0;
        float top = engine->spec->value8;
        float threshold;
        if (gear > 2) {
            threshold = spec->ratios[gear] / spec->ratios[gear - 1] * gearboxManualRatioScale * top;
        } else if (engine->input78 < drivetrainFinishingInputThreshold) {
            threshold = bottom + 100.0f;
        } else {
            threshold = 0.0f;
        }
        if (value54 == value38) {
            if (timer > gearboxManualTimeout) {
                return -1;
            }
            if (gear > value40 && !(top - gearboxTopMargin >= engineSpeed)) {
                return -1;
            }
        } else {
            value40 = gear;
            if (value54 < value38) {
                value50 = 0.0f;
                if (engineSpeed > threshold) {
                    value54 = gear;
                    value38 = gear;
                    return request;
                }
                value38 = gear - 1;
                return request;
            }
            if (value54 > value38) {
                value38 = gear + 1;
                value50 = 0.0f;
                return request;
            }
        }
        if (bottom + 100.0f <= engineSpeed || gear <= 2) {
            return request;
        }
        value38 = gear - 1;
        return -1;
    }
    if (status60 == 3 && request != gear) {
        if (mode44 != 1) {
            pending48 = 0;
            timer5C = (float)drivetrainClock * drivetrainClockScale;
            return gear;
        }
        if (pending48 != 0) {
            return request;
        }
        pending48 = 1;
        value38 = request;
        if (request > gear && gear >= 1) {
            ApplyShiftAdjustment(value6C);
        }
        return request;
    }
    if (request == gear || request == value38) {
        return request;
    }
    value38 = request;
    if (request > gear && gear >= 1) {
        ApplyShiftAdjustment(engineSpeed);
    }
    return request;
}
