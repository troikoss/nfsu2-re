#include "HumanSteering.h"
#include "Engine.h"
#include "DriveFeedbackCurve.h"
#include "GameModeFlags.h"

extern "C" double __cdecl fabs(double);
#pragma intrinsic(fabs)
extern "C" double __cdecl sqrt(double);
#pragma intrinsic(sqrt)

// Original steering curves are consecutive 0x14-byte EngineCurve objects.
extern EngineCurve steeringInputCurves[5];           // 0x7e9e4c; [1]/[4] keyboard, [3] analog
extern EngineCurve steeringCounterCurve;             // 0x7e9eb0
extern EngineCurve steeringCounterCurveGroup3;       // 0x7e9ec4
extern EngineCurve steeringLimitCurve;               // 0x7e9ed8
extern EngineCurve steeringAnalogLimitCurve;         // 0x7e9eec
extern EngineCurve steeringAverageLimitCurve;        // 0x7e9f00
extern EngineCurve steeringSpeedRateCurve;           // 0x7e9f14
extern DriveFeedbackCurve steeringYawCurve;          // 0x7e9f50
extern DriveFeedbackCurve steeringRecoveryCurve;     // 0x81e5b0
extern float steeringPedalScale;
extern float steeringRecoveryWindow;
extern float steeringPhysicsSpeedScale;
extern float steeringRadiansToDegrees;
extern float steeringSlipEnter;
extern float steeringSlipExit;
extern float steeringSlipEnterSpeed;
extern float steeringSlipExitSpeed;
extern float steeringAverageThreshold;
extern float steeringLimitRecovery;
extern float steeringDeadzone;
extern float steeringMinimumAngle;
extern float steeringMaximumAngle;
extern float steeringHalfTurn;
extern float steeringFullTurn;
extern float steeringDegreesPerUnit;
extern float steeringUnitsPerDegreeScale;
extern float drivetrainSlipSpeed;
extern int drivetrainClock;
extern float drivetrainClockScale;
extern void *inputDevices86FFB4[];
extern unsigned char inputDeviceFlags86E5BC[];

// 0x59fa10: no slip angle below speed 1.0.
float DrivetrainAuxiliary::GetSteeringSlip() {
    if (value3EC < drivetrainSlipSpeed) return 0.0f;
    return GetSlipAngle();
}

// 0x5c1000: input devices that report analog steering.
bool IsAnalogInputDevice(int device) {
    return inputDevices86FFB4[device] != 0 && inputDeviceFlags86E5BC[device] != 0;
}

// 0x405dc0 returns the keyboard steering limit in degrees and stores it at
// +0x80. A speed curve is scaled by pedal load and input history, eased in
// after a collision stamp (car body +0x140) outside groups 89E7D9/890104,
// replaced near a counter-steering slip, or kept from falling below the
// previous limit minus 0x783af0 (0.0) per second while steering is held; the
// result is capped at 45.
float HumanSteering::CalculateMaximum(float input, float duration, int) {
    MoverModeOwner *car = car0;
    float speed = car->speed42C;
    float limit = steeringLimitCurve.Lookup(speed);
    MoverInput *pedals = car->input30;
    float throttle = pedals->throttle20C;
    float brake = pedals->brake214;
    float handbrake = pedals->handbrake210;
    double response = steeringSpeedRateCurve.Lookup(car->speed42C);
    limit = (float)((((response * (1.0f - (((double)throttle + 1.0f) - ((double)handbrake + brake) * 0.5f) * 0.5f))
                      * steeringPedalScale) + 1.0f) * limit);
    limit = (float)((double)steeringAverageLimitCurve.Lookup((float)fabs(inputHistory14.averageC)) * limit);
    if (gameFlag89E7D9 == 0 && gameFlag890104 == 0) {
        int stamp = car->collisionBody3C->tick140;
        if (stamp != 0 && stamp != 0x7fffffff) {
            double since = (double)(drivetrainClock - stamp) * drivetrainClockScale;
            float elapsed = (float)since;
            if (since < steeringRecoveryWindow) {
                float current = car->speed42C;
                double ratio = current / ((double)car->physicsInfo10->parameters10.GetPhysicsSpeed() * steeringPhysicsSpeedScale);
                float fraction;
                if (1.0f < ratio) fraction = 1.0f;
                else fraction = (float)ratio;
                double ease = steeringRecoveryCurve.Lookup(elapsed);
                limit = (float)((((1.0f - (double)fraction) * (1.0f - ease)) + ease) * limit);
            }
        }
    }
    double slip = (double)car->State()->GetSteeringSlip() * steeringRadiansToDegrees;
    WheelVector *velocity = &car->State()->velocity30;
    float magnitude = (float)fabs(slip);
    float velocityMagnitude = (float)sqrt(((double)velocity->x * velocity->x + (double)velocity->y * velocity->y)
                                          + (double)velocity->z * velocity->z);
    bool opposing = (double)input * slip < 0.0;
    if (flag10) {
        if (magnitude < steeringSlipExit || velocityMagnitude < steeringSlipExitSpeed) flag10 = 0;
    } else if (magnitude > steeringSlipEnter && velocityMagnitude >= steeringSlipEnterSpeed) {
        flag10 = 1;
    }
    if (opposing && (flag10 || magnitude > limit)) {
        EngineCurve *curve = IsModeGroup3() ? &steeringCounterCurveGroup3 : &steeringCounterCurve;
        double counter = curve->Lookup((float)fabs(input));
        limit = (float)(magnitude * counter);
        double cap = counter * steeringMaximumAngle;
        if (limit > cap) limit = (float)cap;
    } else if (!(fabs(inputHistory14.averageC) < steeringAverageThreshold)) {
        double floor = value80 - (double)steeringLimitRecovery * duration;
        if (!(limit > floor)) limit = (float)floor;
    }
    if (!(limit < steeringMaximumAngle)) limit = 45.0f;
    value80 = limit;
    return limit;
}

// 0x416bb0: keyboard steering. The input passes through mode curve [1] or
// [4]; the target angle (limit * yaw-adjusted scale * input, within +-45) is
// approached by the rate from 0x4060e0, a zero dead zone applies, and the
// angle is stored in 1/65536 turns at +0x7c. Returns the curve-mapped input.
float HumanSteering::UpdateKeyboard(float input, float duration, int device) {
    input = steeringInputCurves[IsModeGroup3() || IsModeGroupE2() ? 4 : 1].Lookup(input);
    float current = (float)angle7C * steeringDegreesPerUnit;
    if (!(current < steeringHalfTurn)) current = (float)((double)current - steeringFullTurn);
    MoverModeOwner *car = car0;
    float scale = car->record20->preparation270.value0;
    if (IsModeGroup3() && (double)input * car->State()->valueE0.z > 0.0) {
        float yaw;
        steeringYawCurve.Evaluate(&yaw, (float)fabs(car->State()->valueE0.z));
        scale = (float)((double)yaw * scale);
    }
    double product = ((double)CalculateMaximum(input, duration, device) * scale) * input;
    float target = (float)product;
    if (!(product > steeringMinimumAngle)) target = steeringMinimumAngle;
    else if (steeringMaximumAngle < target) target = 45.0f;
    double step = ((double)CalculateRate(input, duration, device) * scale) * duration;
    float upper = (float)((double)current + step);
    double lower = (double)current - step;
    double angle = target > lower ? (double)target : lower;
    if (upper < angle) angle = upper;
    float stored = (float)angle;
    double applied = fabs(angle) < steeringDeadzone ? 0.0 : (double)stored;
    int units = (int)(applied * steeringUnitsPerDegreeScale);
    angle7C = (unsigned short)(units / 360);
    return input;
}

// 0x4061c0: analog steering. The raw input is clamped to +-value8, mapped to
// -1..1 and through curve [3]. Unless flag4 is set, the limit is computed as
// in 0x405dc0 with curve 0x7e9eec and a spilled pedal term; counter-steering
// takes the slip magnitude (at most 45) instead of a counter curve. The angle
// is set directly from the limit, without a rate, and +0x80 is not updated.
float HumanSteering::UpdateAnalog(float input, float duration, int) {
    float limit = 45.0f;
    double range = value8;
    double low = -range;
    double clamped = input > low ? (double)input : low;
    if (range < clamped) clamped = range;
    double fraction = (clamped - low) / (range - low);
    input = steeringInputCurves[3].Lookup((float)((fraction + fraction) - 1.0f));
    MoverModeOwner *car = car0;
    if (!flag4) {
        float speed = car->speed42C;
        limit = steeringAnalogLimitCurve.Lookup(speed);
        MoverInput *pedals = car->input30;
        float pedal = (float)(1.0f - (((double)pedals->throttle20C + 1.0f)
                                      - ((double)pedals->brake214 + pedals->handbrake210) * 0.5f) * 0.5f);
        limit = (float)((((double)steeringSpeedRateCurve.Lookup(car->speed42C) * steeringPedalScale) * pedal + 1.0f) * limit);
        limit = (float)((double)steeringAverageLimitCurve.Lookup((float)fabs(inputHistory14.averageC)) * limit);
    }
    double slip = (double)car->State()->GetSteeringSlip() * steeringRadiansToDegrees;
    WheelVector *velocity = &car->State()->velocity30;
    float magnitude = (float)fabs(slip);
    float velocityMagnitude = (float)sqrt(((double)velocity->x * velocity->x + (double)velocity->y * velocity->y)
                                          + (double)velocity->z * velocity->z);
    bool opposing = (double)input * slip < 0.0;
    if (flag10) {
        if (magnitude < steeringSlipExit || velocityMagnitude < steeringSlipExitSpeed) flag10 = 0;
    } else if (magnitude > steeringSlipEnter && velocityMagnitude >= steeringSlipEnterSpeed) {
        flag10 = 1;
    }
    if (opposing && (flag10 || magnitude > limit)) {
        limit = magnitude;
        if (limit > steeringMaximumAngle) limit = 45.0f;
    } else if (!(fabs(inputHistory14.averageC) < steeringAverageThreshold)) {
        double floor = value80 - (double)steeringLimitRecovery * duration;
        if (!(limit > floor)) limit = (float)floor;
    }
    double product = ((double)limit * car->record20->preparation270.value0) * input;
    float angle = (float)product;
    double applied;
    if (!(product > steeringMinimumAngle)) applied = steeringMinimumAngle;
    else if (steeringMaximumAngle < angle) applied = steeringMaximumAngle;
    else applied = angle;
    int units = (int)(applied * steeringUnitsPerDegreeScale);
    angle7C = (unsigned short)(units / 360);
    return input;
}

extern EngineCurve steeringAlternateAnalogCurve;     // 0x7e9f78
extern EngineCurve steeringAlternateCurve;           // 0x7e9f8c
extern EngineCurve steeringAlternateRateCurve;       // 0x7e9fa0
extern EngineCurve steeringAlternateShapeCurve;      // 0x7e9fb4
extern EngineCurve steeringAlternateSlipCurve;       // 0x7e9fc8
extern EngineCurve steeringAlternateCounterCurve;    // 0x7e9fdc
extern float steeringAlternateCounter;               // .data 0x7e93b4, written here
extern float steeringAlternateRateScale9470;
extern float steeringAlternateReturnScale;
extern float steeringAlternateMinimumStep;

// 0x406a40: alternate steering (global 0x81cb8c). The limit comes from a
// speed curve chosen by device and slip state and a throttle-minus-brake
// term; counter-steering scales the slip magnitude by global 0x7e93b4, which
// this routine sets. The current angle is inverted through shape curve
// 0x7e9fb4, moved toward the input by a speed-dependent step (doubled when
// moving away from centre, at least 0.173), and mapped back. Stores the
// angle and limit; returns the raw input.
float HumanSteering::UpdateAlternate(float input, float duration, int device) {
    float current = (float)angle7C * steeringDegreesPerUnit;
    if (!(current < steeringHalfTurn)) current = (float)((double)current - steeringFullTurn);
    MoverModeOwner *car = car0;
    double slip = (double)car->State()->GetSlipAngle() * steeringRadiansToDegrees;
    float signedSlip = (float)slip;
    double magnitude = fabs(slip);
    float slipMagnitude = (float)magnitude;
    WheelVector *velocity = &car->State()->velocity30;
    float velocityMagnitude = (float)sqrt(((double)velocity->x * velocity->x + (double)velocity->y * velocity->y)
                                          + (double)velocity->z * velocity->z);
    if (flag10) {
        if (magnitude < steeringSlipExit || velocityMagnitude < steeringSlipExitSpeed) flag10 = 0;
    } else if (magnitude > steeringSlipEnter && velocityMagnitude >= steeringSlipEnterSpeed) {
        flag10 = 1;
    }
    EngineCurve *curve;
    if (IsAnalogInputDevice(device)) curve = &steeringAlternateAnalogCurve;
    else if (IsModeGroup3() && flag10) curve = &steeringAlternateSlipCurve;
    else curve = &steeringAlternateCurve;
    double base = curve->Lookup(car->speed42C);
    MoverInput *pedals = car->input30;
    float limit = (float)((((1.0f - (((double)pedals->throttle20C - pedals->brake214) + 1.0f) * 0.5f))
                           * steeringPedalScale + 1.0f) * base);
    if ((double)signedSlip * input < 0.0 && (flag10 || slipMagnitude > limit)) {
        if (IsAnalogInputDevice(device)) {
            steeringAlternateCounter = 1.0f;
        } else {
            EngineCurve *counter = IsModeGroup3() ? &steeringCounterCurveGroup3 : &steeringAlternateCounterCurve;
            steeringAlternateCounter = counter->Lookup((float)fabs(input));
        }
        limit = (float)((double)steeringAlternateCounter * slipMagnitude);
        double cap = (double)steeringAlternateCounter * steeringMaximumAngle;
        if (limit > cap) limit = (float)cap;
    }
    double ratio = (double)current / limit;
    float shaped = (float)ratio;
    float position = steeringAlternateShapeCurve.Invert((float)fabs(ratio));
    if (shaped < 0.0f) position = -position;
    float delta = (float)((double)input - position);
    double step = (double)steeringAlternateRateCurve.Lookup(car->speed42C) * steeringAlternateRateScale9470;
    if ((position > 0.0f && position > input) || (position < 0.0f && position < input))
        step = step * steeringAlternateReturnScale;
    step = step * duration;
    if (step < steeringAlternateMinimumStep) step = steeringAlternateMinimumStep;
    if (delta > step) delta = (float)step;
    else if (delta < -step) delta = (float)-step;
    position = (float)((double)delta + position);
    double angle = (double)steeringAlternateShapeCurve.Lookup((float)fabs(position)) * limit;
    if (position < 0.0f) angle = -angle;
    angle = angle * car->record20->preparation270.value0;
    value80 = limit;
    int units = (int)(angle * steeringUnitsPerDegreeScale);
    angle7C = (unsigned short)(units / 360);
    return input;
}
