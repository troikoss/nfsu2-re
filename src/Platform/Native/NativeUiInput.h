#ifndef UG2_NATIVE_UI_INPUT_H
#define UG2_NATIVE_UI_INPUT_H

// SDL platform seam. Devices feed the same FEng messages as keyboard input.
#include "NativePause.h"
#include <SDL2/SDL.h>

class NativeUiInput {
public:
    NativeUiInput() : controller(0), direction(-1), repeatAt(0), focused(true) { Open(); }
    ~NativeUiInput() { Close(); }
    void Close() { if (controller) SDL_GameControllerClose(controller); controller = 0; direction = -1; }
    void Open(int requested = -1) {
        if (controller) return;
        for (int i = 0; i < SDL_NumJoysticks(); ++i) {
            if (requested >= 0 && i != requested) continue;
            if (SDL_IsGameController(i)) { controller = SDL_GameControllerOpen(i); if (controller) break; }
        }
    }
    bool Event(const SDL_Event &event, SDL_Window *window, NativePause &pause) {
        if (event.type == SDL_CONTROLLERDEVICEADDED) { Open(); return true; }
        if (event.type == SDL_CONTROLLERDEVICEREMOVED) {
            if (controller && event.cdevice.which == SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(controller))) {
                Close(); Open();
            }
            return true;
        }
        if (event.type == SDL_WINDOWEVENT) {
            if (event.window.event == SDL_WINDOWEVENT_FOCUS_LOST) { focused = false; direction = -1; }
            if (event.window.event == SDL_WINDOWEVENT_FOCUS_GAINED) focused = true;
        }
        if (!focused) return false;
        if (event.type == SDL_CONTROLLERBUTTONDOWN && controller &&
            event.cbutton.which == SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(controller))) {
            if (event.cbutton.button == SDL_CONTROLLER_BUTTON_START) {
                if (pause.Active()) pause.Key(kPauseKeyStart); else pause.Open();
            } else if (pause.Active()) {
                if (event.cbutton.button == SDL_CONTROLLER_BUTTON_A) pause.Key(kPauseKeyAccept);
                else if (event.cbutton.button == SDL_CONTROLLER_BUTTON_Y) pause.OpenControls();
                else if (event.cbutton.button == SDL_CONTROLLER_BUTTON_B) pause.Key(kPauseKeyBack);
            }
            return true;
        }
        if (!pause.Active()) return false;
        if (event.type == SDL_MOUSEMOTION || event.type == SDL_MOUSEBUTTONDOWN) {
            if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_RIGHT) {
                pause.Key(kPauseKeyBack); return true;
            }
            if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button != SDL_BUTTON_LEFT) return true;
            int w, h; SDL_GetWindowSize(window, &w, &h);
            int x = event.type == SDL_MOUSEMOTION ? event.motion.x : event.button.x;
            int y = event.type == SDL_MOUSEMOTION ? event.motion.y : event.button.y;
            if (h > 0) pause.Pointer((x - w * .5f) * 480.0f / h, (y - h * .5f) * 480.0f / h,
                                     event.type == SDL_MOUSEBUTTONDOWN);
            return true;
        }
        if (event.type == SDL_MOUSEWHEEL) {
            int amount = event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED ? -event.wheel.y : event.wheel.y;
            if (amount) pause.Key(amount > 0 ? kPauseKeyUp : kPauseKeyDown);
            return true;
        }
        return false;
    }
    void Update(NativePause &pause) {
        if (!focused || !pause.Active() || !controller) { direction = -1; return; }
        int x = SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_LEFTX);
        int y = SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_LEFTY);
        int next = -1;
        if (SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_DPAD_LEFT)) next = kPauseKeyLeft;
        else if (SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_DPAD_RIGHT)) next = kPauseKeyRight;
        else if (SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_DPAD_UP)) next = kPauseKeyUp;
        else if (SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_DPAD_DOWN)) next = kPauseKeyDown;
        else if (x < -16000 && abs(x) > abs(y)) next = kPauseKeyLeft;
        else if (x > 16000 && abs(x) > abs(y)) next = kPauseKeyRight;
        else if (y < -16000) next = kPauseKeyUp;
        else if (y > 16000) next = kPauseKeyDown;
        Uint32 now = SDL_GetTicks();
        if (next >= 0 && (next != direction || (Sint32)(now - repeatAt) >= 0)) {
            pause.Key((NativePauseKey)next);
            repeatAt = now + (next != direction ? 350 : 110);
        }
        direction = next;
    }
private:
    SDL_GameController *controller;
    int direction;
    Uint32 repeatAt;
    bool focused;
};
#endif
