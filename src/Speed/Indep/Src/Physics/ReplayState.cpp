#include "VehicleMover.h"

extern int replayState8669F4;
extern int replayState866A14;

// Original 0x5f00f0. This recovers the predicate, not recording/playback.
bool ReplayRecorder::QueryState5F00F0() {
    return flag21 != 0 || replayState8669F4 > 0 || replayState866A14 > 0;
}
