#include "VehicleMover.h"
#include "GameModeFlags.h"

// Original 0x5b4fc0. The first two arguments are forwarded as complete dwords
// to 0x5b4dc0, which interprets their low bytes. Do not truncate them here.
void PhysicsParameters::Refresh(int refresh, int flag, char select) {
    if (value0 == -1) return;
    int mode = 0;
    if (select) {
        if (IsModeGroup3()) mode = 3;
        else if (gameFlag89E7E3 || gameFlag89010C) mode = 10;
        else if (IsModeGroup2()) mode = 2;
    }
    Compose(mode, refresh, flag);
}

// Original 0x5b5330: the class index, composed on first use.
signed char PhysicsParameters::GetClassIndex() {
    if (index4 == -1) Refresh(1, 1, 1);
    return index4;
}
