#ifndef UG2_NATIVE_PAUSE_H
#define UG2_NATIVE_PAUSE_H

// Platform scaffolding: runs the recovered pause menu (Speed/Indep/Src/Frontend/PauseMenu.cpp,
// PauseFNGObject 0x4eb410) and in-game options (Frontend/PauseOptions.cpp) over UG2's own
// UI_Pause.fng, UI_PauseOptionsMain.fng, UI_PauseOptions.fng and UI_InGameDialog.fng from
// GLOBAL/InGameCommon.bun. FEng, the IconScroller and options list bases, the option item bases,
// the in-game dialog and the game flow are not recovered; NativePause.cpp and
// NativePauseOptions.cpp supply them as seams at their original call boundaries.
#include "NativeFng.h"

#include <string>
#include <vector>

enum NativePauseKey { kPauseKeyLeft, kPauseKeyRight, kPauseKeyAccept, kPauseKeyBack, kPauseKeyUp, kPauseKeyDown, kPauseKeyStart, kPauseKeyDevice };

// What the demo does for the game-flow messages the menu queues (0x5f4240 / 0x5f41e0).
enum NativePauseRequest { kPauseRequestNone, kPauseRequestResume, kPauseRequestRestart, kPauseRequestQuit };

class NativePause {
public:
    NativePause();
    ~NativePause();
    bool Load(const char *gameDirectory, NativeTextureCache *textures, const char *fontPath, char *error,
              int errorSize);
    // Native view/input context: settings shared with the driving demo for this session.
    void SetViewSettings(bool metric, int camera);
    bool Metric() const;
    int CameraOption() const;
    bool Loaded() const;
    bool Active() const;
    // JOY_EVENT_PAUSE seam: constructs the screen through FNGINIT_UI_Pause.fng (0x4f4790).
    void Open();
    void OpenControls();
    bool CaptureKey(int scancode);
    void Key(NativePauseKey key);
    // Coordinates in the centred 640x480 FEng space. Hover selects; click edits.
    void Pointer(float x, float y, bool click, bool drag = false);
    // Advances the real-time clock 0x8651ac, script events and queued FEng messages.
    void Update(float seconds);
    void Draw(int width, int height);
    // Returns and clears the last game-flow request; closes the screen when one is pending.
    NativePauseRequest TakeRequest();
};

#endif
