#ifndef UG2_NATIVE_PAUSE_H
#define UG2_NATIVE_PAUSE_H

// Platform scaffolding: runs the recovered pause menu (Speed/Indep/Src/Frontend/PauseMenu.cpp,
// PauseFNGObject 0x4eb410) over UG2's own UI_Pause.fng and UI_InGameDialog.fng. The packages are
// the JDLZ-compressed 0x30210 chunks of GLOBAL/InGameCommon.bun, unpacked by the recovered bWare
// decompressor. FEng, the IconScroller base, the in-game dialog and the game flow are not
// recovered; NativePause.cpp supplies them as seams at their original call boundaries.
#include "NativeFng.h"

#include <string>
#include <vector>

enum NativePauseKey { kPauseKeyLeft, kPauseKeyRight, kPauseKeyAccept, kPauseKeyBack };

// What the demo does for the game-flow messages the menu queues (0x5f4240 / 0x5f41e0).
enum NativePauseRequest { kPauseRequestNone, kPauseRequestResume, kPauseRequestRestart, kPauseRequestQuit };

class NativePause {
public:
    NativePause();
    ~NativePause();
    bool Load(const char *gameDirectory, NativeTextureCache *textures, const char *fontPath, char *error,
              int errorSize);
    bool Loaded() const;
    bool Active() const;
    // JOY_EVENT_PAUSE seam: constructs the screen through FNGINIT_UI_Pause.fng (0x4f4790).
    void Open();
    void Key(NativePauseKey key);
    // Advances the real-time clock 0x8651ac, script events and queued FEng messages.
    void Update(float seconds);
    void Draw(int width, int height);
    // Returns and clears the last game-flow request; closes the screen when one is pending.
    NativePauseRequest TakeRequest();
};

#endif
