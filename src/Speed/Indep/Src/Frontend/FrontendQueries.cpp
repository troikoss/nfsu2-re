#include "PauseMenu.h"

extern const char uiMainFng78F530[];        // "UI_Main.fng"
extern const char uiQRModeSelectFng78F398[]; // "UI_QRModeSelect.fng"
extern const char pcOLLobbyFng78F320[];      // "PC_OL_Lobby.fng"

// Original 0x50d2d0: the FEng control mask of a player (one bit per pad, none past four).
unsigned int FngControlMask(int player) {
    if (player == 0) return 1;
    if (player == 1) return 2;
    if (player == 2) return 4;
    return player == 3 ? 8 : 0;
}

// Original 0x502ff0: the frontend package to return to for the profile's game mode.
const char *ProfileData83A9D0::Value502FF0(int value) {
    const char *package = uiMainFng78F530;
    if (value == 0) {
        int mode = value156A8;
        if (mode == 1 || mode == 2) return uiMainFng78F530;
        if (mode == 4) return uiQRModeSelectFng78F398;
        if (mode == 8 || mode == 0x200) return pcOLLobbyFng78F320;
    }
    return package;
}
