#ifndef UG2_NATIVE_WORLD_H
#define UG2_NATIVE_WORLD_H

#include "../../Speed/Indep/Src/World/WCollision.h"

struct NativeTrack {
    int packCount;
    bool spawnFound;
    float spawn[3];
    float heading;
};

bool LoadNativeTrack(const char *gameDirectory, const char *region, NativeTrack *track, char *error,
                     int errorSize);

#endif
