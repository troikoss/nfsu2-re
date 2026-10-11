// SDL integration check with a real virtual controller and an isolated pause receiver.
#include "../src/Platform/Native/NativeUiInput.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <vector>
static bool active = false;
static int controls = 0, clicks = 0;
static float pointerX = 0, pointerY = 0;
static std::vector<NativePauseKey> keys;
NativePause::NativePause() {}
NativePause::~NativePause() {}
bool NativePause::Active() const { return active; }
void NativePause::Open() { active = true; }
void NativePause::OpenControls() { ++controls; }
void NativePause::Key(NativePauseKey key) { keys.push_back(key); }
void NativePause::Pointer(float x, float y, bool click) { pointerX=x;pointerY=y;if(click) ++clicks; }
static void Pump(NativeUiInput &input, SDL_Window *window, NativePause &pause) {
    SDL_JoystickUpdate();SDL_PumpEvents();SDL_Event event;
    while(SDL_PollEvent(&event)) input.Event(event,window,pause);
    input.Update(pause);
}
int main() {
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS,"1");
    assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER)==0);
    SDL_Window *window=SDL_CreateWindow("UI input check",0,0,1280,720,SDL_WINDOW_HIDDEN);assert(window);
    NativePause pause;NativeUiInput input;
    int device=SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER,6,15,0);assert(device>=0);
    SDL_Joystick *joystick=SDL_JoystickOpen(device);assert(joystick);
    assert(SDL_IsGameController(device));
    Pump(input,window,pause);
    input.Close(); input.Open(device); // isolate the fixture from the user's attached pads
    assert(SDL_JoystickSetVirtualButton(joystick,SDL_CONTROLLER_BUTTON_START,1)==0);
    Pump(input,window,pause);assert(active);
    SDL_JoystickSetVirtualButton(joystick,SDL_CONTROLLER_BUTTON_START,0);Pump(input,window,pause);
    SDL_JoystickSetVirtualButton(joystick,SDL_CONTROLLER_BUTTON_DPAD_DOWN,1);Pump(input,window,pause);
    assert(keys.size()==1 && keys.back()==kPauseKeyDown);
    Pump(input,window,pause);assert(keys.size()==1); // no immediate repeat
    SDL_Delay(370);Pump(input,window,pause);assert(keys.size()==2 && keys.back()==kPauseKeyDown);
    SDL_JoystickSetVirtualButton(joystick,SDL_CONTROLLER_BUTTON_DPAD_DOWN,0);Pump(input,window,pause);
    SDL_JoystickSetVirtualAxis(joystick,SDL_CONTROLLER_AXIS_LEFTX,22000);Pump(input,window,pause);
    assert(keys.back()==kPauseKeyRight);
    SDL_JoystickSetVirtualAxis(joystick,SDL_CONTROLLER_AXIS_LEFTX,0);Pump(input,window,pause);
    SDL_JoystickSetVirtualButton(joystick,SDL_CONTROLLER_BUTTON_A,1);Pump(input,window,pause);assert(keys.back()==kPauseKeyAccept);
    SDL_JoystickSetVirtualButton(joystick,SDL_CONTROLLER_BUTTON_A,0);Pump(input,window,pause);
    SDL_JoystickSetVirtualButton(joystick,SDL_CONTROLLER_BUTTON_B,1);Pump(input,window,pause);assert(keys.back()==kPauseKeyBack);
    SDL_JoystickSetVirtualButton(joystick,SDL_CONTROLLER_BUTTON_Y,1);Pump(input,window,pause);assert(controls==1);
    SDL_Event e={};e.type=SDL_MOUSEBUTTONDOWN;e.button.button=SDL_BUTTON_LEFT;e.button.x=790;e.button.y=450;
    input.Event(e,window,pause);assert(clicks==1 && fabsf(pointerX-100)<.01f && fabsf(pointerY-60)<.01f);
    e.button.button=SDL_BUTTON_RIGHT;input.Event(e,window,pause);assert(keys.back()==kPauseKeyBack);
    e.type=SDL_MOUSEWHEEL;e.wheel.y=-1;input.Event(e,window,pause);assert(keys.back()==kPauseKeyDown);
    e.type=SDL_WINDOWEVENT;e.window.event=SDL_WINDOWEVENT_FOCUS_LOST;input.Event(e,window,pause);
    size_t count=keys.size();SDL_JoystickSetVirtualAxis(joystick,SDL_CONTROLLER_AXIS_LEFTX,-22000);Pump(input,window,pause);assert(keys.size()==count);
    SDL_JoystickClose(joystick);assert(SDL_JoystickDetachVirtual(device)==0);Pump(input,window,pause);
    input.Close();SDL_DestroyWindow(window);SDL_Quit();
    puts("PASS SDL virtual gamepad hotplug, Start/A/B/Y, D-pad repeat, stick, focus loss and mouse conversion/click/wheel");
}
