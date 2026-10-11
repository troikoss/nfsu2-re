#include "HumanSteering.h"
#include "Engine.h"
#include "GameModeFlags.h"

extern "C" double __cdecl fabs(double);
#pragma intrinsic(fabs)
extern float steeringFrequency;
extern float steeringInputWindow;
extern float steeringRateWindow;
extern float steeringRateScale;
extern float steeringAlternateRateScale;
extern EngineCurve steeringDeltaRateCurve;
extern EngineCurve steeringSpeedRateCurve;
extern EngineCurve steeringHistoryRateCurve;

// Original 0x405d00 normal destructor path; exceptional SEH is unresolved.
void HumanSteering::Destroy() {
    rateHistory48.DestroyTimed();
    inputHistory14.DestroyTimed();
}

// Original 0x405c60. Allocation-failure/SEH unwinding is not recovered by this
// explicit constructor method; native ownership remains a separate seam.
HumanSteering *HumanSteering::Construct(MoverModeOwner *car) {
    inputHistory14.ConstructTimed(steeringInputWindow, steeringFrequency);
    rateHistory48.ConstructTimed(steeringRateWindow, steeringFrequency);
    car0 = car;
    angle7C = 0;
    previous84 = 0.0f;
    flag10 = 0;
    flag4 = 0;
    value80 = 45.0f;
    value8 = 1.0f;
    valueC = 7.0f;
    return this;
}

// Original 0x4060e0 (ret 12): the third argument is retained although this
// helper does not read it. Tick time is the original global clock, not duration.
float HumanSteering::CalculateRate(float value, float duration, int) {
    float time = (float)drivetrainClock * drivetrainClockScale;
    float delta = (float)fabs((value - previous84) / duration);
    float target = steeringDeltaRateCurve.Lookup(delta);
    rateHistory48.Advance(target, time);
    float scale = IsModeGroup3() || IsModeGroupE2()
        ? steeringAlternateRateScale : steeringRateScale;
    float speedScale = steeringSpeedRateCurve.Lookup(car0->speed42C) * scale;
    float rate = rateHistory48.averageC * speedScale;
    return steeringHistoryRateCurve.Lookup((float)fabs(inputHistory14.averageC)) * rate;
}
