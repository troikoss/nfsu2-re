#include "VehicleMover.h"
#include "Engine.h"

extern "C" double __cdecl sqrt(double);
#pragma intrinsic(sqrt)

extern float engineValueScale;
extern float engineSpeedOverride;
extern float engineSpeedOverrideUnset;
extern float steeringDegreesPerUnit;
extern float steeringDisplayRate;
extern float steeringHalfTurn;
extern float steeringFullTurn;
extern float steeringUnitsPerDegreeScale;
extern float engineDisplayFallRates[];
extern float engineDisplayCurrentShare;
extern float engineDisplayPreviousShare;
extern float slipDisplayScale;
extern WheelVector zeroVector;

// Original 0x592400: geometric origin from the body position and centre offset.
void DrivetrainAuxiliary::UpdateOrigin(WheelVector *centre) {
    if (!centre) {
        centre = &record0->centre0;
    }
    WheelVector rotated;
    WheelTransform(&rotated, &matrix50, centre);
    origin10.x = position20.x - rotated.x;
    origin10.z = position20.z - rotated.z;
    origin10.y = position20.y - rotated.y;
}

// Original 0x5a5540: copy integrated body and wheel state into the shared vehicle state.
void VehicleMover::ExportState(float duration) {
    MoverBody *body = body20;
    DrivetrainAuxiliary *state = state58;
    state->position20 = body->position20;
    BodyMatrixCopy(&state58->matrix50, &body20->matrix30);
    state58->UpdateOrigin(&body20->centre1F0);
    BodyMatrixCopy(&state58->matrix90, &state58->matrix50);
    body = body20;
    state = state58;
    state->velocity30 = body->vectorA0;
    state->flag480 = 0;
    state->value3F0 = (float)sqrt(body->vectorA0.x * body->vectorA0.x + body->vectorA0.z * body->vectorA0.z
                                  + body->vectorA0.y * body->vectorA0.y);
    state58->angularD0 = body20->vectorC0;
    state58->heading3CC = WheelAngle(state58->matrix50.elements[0], state58->matrix50.elements[1]);
    state = state58;
    state->value3EC = state->velocity30.z * state->matrix50.elements[2]
                    + state->velocity30.y * state->matrix50.elements[1]
                    + state->velocity30.x * state->matrix50.elements[0];
    state = state58;
    volatile float current = (float)(int)state->steering3CE * steeringDegreesPerUnit;
    float target = (float)(int)input4->steering1E0 * steeringDegreesPerUnit;
    float rate = steeringDisplayRate * duration;
    float shown = current;
    if (!(shown < steeringHalfTurn)) {
        shown = shown - steeringFullTurn;
    }
    if (!(target < steeringHalfTurn)) {
        target = target - steeringFullTurn;
    }
    volatile float upper = rate + shown;
    volatile float lower = shown - rate;
    if (!(target > lower)) {
        target = lower;
    }
    if (!(upper >= target)) {
        target = upper;
    }
    state->steering3CE = (short)((int)(target * steeringUnitsPerDegreeScale) / 360);
    for (int i = 0; i < 4; ++i) {
        DrivetrainWheel *wheel = wheels38[i];
        state58->wheelSpin2F0[i] = wheel->value28;
        state58->wheelAngle300[i] = wheels38[i]->angle24;
        MoverArm *arm = arms28[i];
        float droop = arm->travel1C >= arm->spec14->rest1C ? arm->spec14->rest1C - arm->travel1C : 0.0f;
        state58->suspension340[i] = (arm->travel1C + droop) - state58->rideOffsets3B8[i];
        state = state58;
        unsigned char previous = state->contacts350[i];
        unsigned char contact = wheels38[i]->flag11C;
        state->contacts350[i] = contact;
        if (!previous) {
            if (contact) {
                state->value3E4 = state->value3E4 + 1;
            }
        } else if (!contact) {
            state->value3E4 = state->value3E4 - 1;
        }
        int surface = wheels38[i]->surface114;
        state58->surfaces354[i] = surface;
        if (state58->maximumSurfaces364[i] < surface) {
            state58->maximumSurfaces364[i] = surface;
        }
        if (flag84) {
            state58->wheelValue394[i] = 0.0f;
            state58->wheelPairs374[i][1] = zeroVector.y;
            state58->wheelPairs374[i][0] = zeroVector.x;
        } else {
            state58->wheelValue394[i] = wheels38[i]->value170;
            state58->wheelPairs374[i][1] = wheels38[i]->value104;
            state58->wheelPairs374[i][0] = wheels38[i]->value100;
        }
        state58->wheelLoad310[i] = wheels38[i]->value164;
        state58->wheelValue320[i] = wheels38[i]->flag11C ? wheels38[i]->value168 : 0.0f;
    }
    Engine *engine = engine48;
    state58->control3F8 = engine->flag88 ? 0.0f : engine->control80 * engine->input78;
    state58->gear3F4 = drivetrain4C->ratioIndex;
    state58->value408 = drivetrain4C->mode44 == 3;
    state58->value40C = drivetrain4C->value70;
    bool engaged = drivetrain4C->pending48 != 0 && drivetrain4C->value38 > 2;
    engine = engine48;
    float speed = engine->state14 * engineValueScale;
    int level = (int)(owner5C->physicsInfo10->parameters10.upgrades2E0[1] + 0.5f);
    volatile float fall = -engineDisplayFallRates[level + (engaged ? 1 : 0) * 4] / engine->spec->divisorC;
    if (duration > 0.0f) {
        float previous = engineSpeedOverride;
        if (engineSpeedOverride == engineSpeedOverrideUnset) {
            previous = state58->engineValue400;
        }
        if ((speed - previous) / duration < fall) {
            previous = engineSpeedOverride;
            if (engineSpeedOverride == engineSpeedOverrideUnset) {
                previous = state58->engineValue400;
            }
            volatile float limited = fall * duration + previous;
            if (!(limited < speed)) {
                speed = limited;
            }
        }
    }
    float previous = engineSpeedOverride;
    if (engineSpeedOverride == engineSpeedOverrideUnset) {
        previous = state58->engineValue400;
    }
    state58->engineValue400 = speed * engineDisplayCurrentShare + previous * engineDisplayPreviousShare;
    float a = wheels38[0]->value118 < 0.0f ? -wheels38[0]->value118 : wheels38[0]->value118;
    volatile float b = wheels38[1]->value118 < 0.0f ? -wheels38[1]->value118 : wheels38[1]->value118;
    volatile float c = wheels38[2]->value118 < 0.0f ? -wheels38[2]->value118 : wheels38[2]->value118;
    volatile float d = wheels38[3]->value118 < 0.0f ? -wheels38[3]->value118 : wheels38[3]->value118;
    float first = a > d ? a : d;
    float second = b > c ? b : c;
    float scale = 1.0f / slipDisplayScale;
    volatile float firstSlip = scale * first;
    float secondSlip = scale * second;
    if (firstSlip > 1.0f) {
        firstSlip = 1.0f;
    }
    if (secondSlip > 1.0f) {
        secondSlip = 1.0f;
    }
    state58->slip3E0 = secondSlip;
    state58->slip3DC = firstSlip;
    state58->averageWheel3D8 = (wheels38[3]->value170 + wheels38[0]->value170 + wheels38[1]->value170
                                + wheels38[2]->value170) * 0.25f;
    for (int i = 0; i < 4; ++i) {
        state58->UpdateWheelRenderMatrices(i);
    }
}
