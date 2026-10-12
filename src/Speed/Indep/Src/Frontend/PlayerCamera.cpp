#include "PauseOptions.h"
#include "../Hud/HudPlayerMasks.h"
#include "../Camera/DriveCamera.h"

extern PlayerSettings83AA64 playerSettings83AA64[2];
extern unsigned char hudDrift89E7D9;
extern int hudDriftOverride890104;
extern int trackNumber89E7A0;
extern int viewMode8707DC; // returned by 0x5b98a0

// Original 0x502540: step the drive camera option, wrapping over four choices (five in drift
// sessions and on tracks 0x104e..0x1053).
void PlayerSettings83AA64::CycleCamera(int step) {
    int camera = value10;
    int last = 3;
    if (hudDrift89E7D9 || hudDriftOverride890104 || (trackNumber89E7A0 >= 0x104e && trackNumber89E7A0 <= 0x1053))
        last = 4;
    if (step == 1) {
        if (++camera > last) camera = 0;
    } else if (step == -1) {
        if (--camera < 0) {
            value10 = last;
            return;
        }
    }
    value10 = camera;
}

// Original 0x5f2fa0 (Player8900AC is the pause menu's view of the player record): step the
// player's camera option and, when the car's CarTypeInfo has a setting for the new camera type
// (lookup 0x441140, which reads the view mode through 0x5b98a0), tell the camera mover.
int Player8900AC::Value5F2FA0(int step) {
    PlayerRecord *player = reinterpret_cast<PlayerRecord *>(this);
    const CameraSettingsTable *table =
        reinterpret_cast<const CameraSettingsTable *>(player->car4->valueC + 0x720);
    playerSettings83AA64[player->index10].CycleCamera(step);
    int type = DriveCameraTypeForOption(playerSettings83AA64[player->index10].value10);
    if (table->Find(type, viewMode8707DC) && player->camera238) player->camera238->SetType20(type);
    return type;
}
