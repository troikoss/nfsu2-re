#include "HumanDriverInput.h"
#include "GameModeFlags.h"
#include "Engine.h"

extern EngineCurve driverPedalCurve;

extern "C" double __cdecl fabs(double);
#pragma intrinsic(fabs)

extern float driverReverseSpeed;
extern float driverReverseLower;
extern float driverReverseRelease;
extern float driverAngularPedalScale;
extern float driverHoldSpeed;
extern float driverEventSpeed;
extern float driverEventValue;
extern char driverEventName[];
extern float driverAlternateThrottleScale;
extern float driverAlternateThrottleMinimum;
extern float driverReversePedalMargin;
extern float driverThrottleRise;
extern float driverThrottleFall;
extern float driverRpmMargin;
extern float driverForwardTimer;
extern float worldTickRate;
extern float worldTickRounding;
extern int collisionFrame7FB724;
extern int *gameFlow8900D8;
extern float engineSpeedOverride;
extern float engineSpeedOverrideUnset;
extern ReplayRecorder replayRecorder;

// Original 0x405b20/0x405b30/0x405b40/0x405b50, shared by input subclasses.
int HumanDriverInput::ReadGearRequest() { return gearRequest1E4; }
int HumanDriverInput::ReadDirectGear() { return directGear1E8; }
void HumanDriverInput::StoreGearRequest(int gear) { gearRequest1E4 = gear; }
void HumanDriverInput::StoreDirectGear(int gear) { directGear1E8 = gear; }

// Original 0x407280, human input slot 0x34: modes 1 and 3 keep the current gear as the
// request; 0 and other modes clear it (0 stays 0, others become 2). A physics mover gets the
// mode at its drivetrain +0x60.
bool HumanDriverInput::ApplyTransmission(int mode) {
    if (mode == 3) {
        gearRequest1E4 = car2A8->gear434;
        transmission2C0 = 3;
    } else if (mode == 1) {
        gearRequest1E4 = car2A8->gear434;
        transmission2C0 = 1;
    } else {
        gearRequest1E4 = -1;
        transmission2C0 = mode == 0 ? 0 : 2;
    }
    if (car2A8->moverType28 == 1) {
        car2A8->mover34->drivetrain4C->status60 = transmission2C0;
        return true;
    }
    return false;
}

// Original 0x407310, human input slot 0x30.
void HumanDriverInput::SelectForward() {
    if (transmission2C0 != 1 && transmission2C0 != 3) {
        gearRequest1E4 = -1;
        return;
    }
    gearRequest1E4 = 2;
    if (IsModeGroup2()) {
        timer2CC.value0 = (int)(driverForwardTimer * worldTickRate + worldTickRounding);
    }
}

// Original 0x577310: timer decrement, sentinel and underflow handling.
int DriverTimer::Advance(float duration) {
    if (value0 != 0 && value0 != 0x7fffffff) {
        value0 = value0 - (int)(duration * worldTickRate);
        if (value0 < 0) {
            value0 = 0;
            return 0;
        }
        if (value0 != 0 && value0 != 0x7fffffff) return 1;
    }
    return 0;
}

// Original 0x407a30: human pedals and reverse/forward selection. Input capture
// supplies +0x2d4/+0x2dc/+0x2ec; this routine writes the mover's pedal fields.
void HumanDriverInput::UpdatePedals(float duration) {
    MoverModeOwner *car = car2A8;
    float signedSpeed = car->speed42C;
    if (IsModeGroupE2()) {
        signedSpeed = signedSpeed / ((float)fabs(car->State()->angularD0.z) * driverAngularPedalScale + 1.0f);
    }
    hold2BC = hold2BC - duration;
    float throttle;
    float handbrake;
    if (hold2BC <= 0.0f || fabs(signedSpeed) <= driverHoldSpeed) {
        throttle = throttle2D4;
        handbrake = handbrake2EC;
        hold2BC = 0.0f;
    } else {
        throttle = 0.0f;
        handbrake = 1.0f;
    }
    SetHandbrake(handbrake);
    // Original byte +0x2f0: retained offset-based until its producer is recovered.
    unsigned char &flag2F0 = *reinterpret_cast<unsigned char *>(reinterpret_cast<char *>(this) + 0x2f0);
    if (brake2DC > 0.6f && throttle2D4 < 0.5f) {
        flag2F0 = 1;
    } else if (signedSpeed > driverReverseRelease) {
        flag2F0 = 0;
    }
    float brake;
    float reverse;
    if (signedSpeed > driverReverseSpeed || !flag2F0) {
        brake = brake2DC;
        reverse = 0.0f;
    } else if (signedSpeed < driverReverseLower || signedSpeed < driverReverseRelease) {
        reverse = brake2DC;
        brake = throttle;
        throttle = 0.0f;
    } else {
        reverse = brake2DC;
        brake = 0.0f;
    }
    reverse = reverse + reverse;
    if (reverse > 1.0f) reverse = 1.0f;
    SetBrake(brake);
    if (handbrake210 > 0.5f && (collisionFrame7FB724 + 1) % 8 == 0 && car->speed430 > driverEventSpeed) {
        drivetrainContext->ControlEvent(driverEventValue, driverEventName);
    }
    if (IsModeGroup2() && timer2CC.Advance(duration)) {
        throttle = driverAlternateThrottleScale * throttle;
        if (throttle <= driverAlternateThrottleMinimum) throttle = driverAlternateThrottleMinimum;
        else if (throttle > 1.0f) throttle = 1.0f;
    }
    if (throttle + driverReversePedalMargin < reverse) {
        SetThrottle(reverse);
        gearRequest1E4 = 0;
    } else {
        if (IsModeGroupE2()) throttle = driverPedalCurve.Lookup(throttle);
        if (!(IsModeGroup2() && car->gear434 == 1) && !IsModeGroupE2()) {
            if (!(throttle > 0.0f)) throttle = 0.0f;
            else if (throttle > 1.0f) throttle = 1.0f;
        } else {
            float upper = driverThrottleRise * duration + throttle20C;
            float lower = throttle20C - driverThrottleFall * duration;
            if (lower < throttle) lower = throttle;
            throttle = lower;
            if (upper < lower) throttle = upper;
            if (throttle2D4 <= 0.6f) {
                if (!(throttle > 0.0f)) throttle = 0.0f;
                else if (throttle > 1.0f) throttle = 1.0f;
            } else {
                float rpm = engineSpeedOverride;
                if (rpm == engineSpeedOverrideUnset) rpm = car->engineValue440;
                // Car record engine +0x1f0 is its first scalar, not EngineSpec::divisorC.
                if (*reinterpret_cast<float *>(car->record20->engine1F0) + driverRpmMargin <= rpm || throttle >= 0.6f) {
                    if (!(throttle > 0.0f)) throttle = 0.0f;
                    else if (throttle > 1.0f) throttle = 1.0f;
                } else {
                    throttle = 0.6f;
                }
            }
        }
        SetThrottle(throttle);
        if (gearRequest1E4 == 0) UnknownSlot30();
    }
    if (replayRecorder.QueryState5F00F0() && *gameFlow8900D8 == 6) {
        SetThrottle(0.0f);
        SetBrake(0.5f);
    }
    car->State()->control3F8 = throttle20C;
}
