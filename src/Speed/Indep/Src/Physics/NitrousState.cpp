#include "VehicleMover.h"

extern int drivetrainClock;
extern int realClock8651AC;
extern float drivetrainClockScale;
extern float nitrousTicksPerSecond;
extern float nitrousDivisor;
extern float nitrousThresholds[3];
extern int nitrousResetMode;
extern ReplayRecorder replayRecorder;

static inline bool IsTickSet(int tick) {
    return tick != 0 && tick != 0x7fffffff;
}

// Original 0x592480: stored capacity minus time since activation, in seconds.
float DrivetrainAuxiliary::GetNitrousRemaining() {
    float used;
    if (IsTickSet(timer420)) {
        int *clock = replayRecorder.QueryState5F00F0() ? &realClock8651AC : &drivetrainClock;
        used = (float)*clock * drivetrainClockScale - (float)timer420 * drivetrainClockScale;
    } else {
        used = 0.0f;
    }
    if (IsTickSet(timer41C)) {
        return (float)timer41C * drivetrainClockScale - used;
    }
    return 0.0f - used;
}

// Original 0x5925b0: remaining fraction of the timer +0x414 capacity.
float DrivetrainAuxiliary::GetNitrousElapsed() {
    float none = 0.0f;
    volatile float remaining = GetNitrousRemaining();
    if (IsTickSet(timer414)) {
        float capacity = (float)timer414 * drivetrainClockScale;
        if (capacity > 0.0f) {
            if (remaining > capacity) {
                volatile float stored = capacity;
                return (remaining - capacity) / nitrousDivisor / capacity + stored / capacity;
            }
            return remaining / capacity + none;
        }
    }
    return none;
}

// Original 0x592500: bank the unused capacity and stop the active timer.
void DrivetrainAuxiliary::FinishNitrous() {
    if (!IsTickSet(timer420)) {
        return;
    }
    int *clock = replayRecorder.QueryState5F00F0() ? &realClock8651AC : &drivetrainClock;
    float remaining = (float)timer41C * drivetrainClockScale
                    - (float)(*clock - timer420) * drivetrainClockScale;
    if (!(0.0f <= remaining)) {
        remaining = 0.0f;
    }
    timer41C = (int)(remaining * nitrousTicksPerSecond + 0.5f);
    if (nitrousResetMode) {
        timer418 = 0;
    }
    timer420 = 0;
}

// Original 0x59fa30: active nitrous stage 1..3 from the remaining fraction.
int DrivetrainAuxiliary::GetNitrousIndex() {
    int index = 0;
    if (!IsTickSet(timer420)) {
        return index;
    }
    volatile float fraction = GetNitrousElapsed();
    for (int i = 2; i >= 0; --i) {
        if (fraction > nitrousThresholds[i]) {
            index = i + 1;
            if (index != 0) {
                return index;
            }
            FinishNitrous();
            return index;
        }
    }
    FinishNitrous();
    return index;
}
