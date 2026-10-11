#ifndef UG2_NATIVE_CONTROLS_H
#define UG2_NATIVE_CONTROLS_H
#include "NativePause.h"
#include <SDL2/SDL.h>
void *CreateNativeControls(void *data);
void DestroyNativeControls(void *screen);
void NativeControlsKey(void *screen, NativePauseKey key);
void NativeControlsPointer(void *screen, float x, float y, bool click);
void NativeControlsUpdate(void *screen);
bool NativeControlsCapture(void *screen, SDL_Scancode key);
bool NativeControlKey(int action, SDL_Scancode key);
bool NativeControlPressed(int action, const Uint8 *keys);
#endif
