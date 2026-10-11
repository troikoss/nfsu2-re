#ifndef UG2_NATIVE_GAMEPAD_H
#define UG2_NATIVE_GAMEPAD_H
#include <SDL2/SDL.h>
#include <math.h>
// Native SDL input/profile seam, not a recovered DirectInput implementation.
// Positive axes use 100 + axis*2; negative axes use the following code.
struct NativeGamepadBindings {
    int binding[23][2];
    NativeGamepadBindings() : binding() {
        binding[1][0]=100+SDL_CONTROLLER_AXIS_TRIGGERRIGHT*2;
        binding[2][0]=100+SDL_CONTROLLER_AXIS_TRIGGERLEFT*2;
        binding[3][0]=101+SDL_CONTROLLER_AXIS_LEFTX*2;
        binding[4][0]=100+SDL_CONTROLLER_AXIS_LEFTX*2;
        binding[5][0]=SDL_CONTROLLER_BUTTON_A+1;
        binding[9][0]=SDL_CONTROLLER_BUTTON_RIGHTSTICK+1;
        binding[10][0]=SDL_CONTROLLER_BUTTON_Y+1;
        binding[12][0]=SDL_CONTROLLER_BUTTON_START+1;
        binding[14][0]=SDL_CONTROLLER_BUTTON_BACK+1;
    }
    static NativeGamepadBindings &Get() { static NativeGamepadBindings value; return value; }
    bool Button(int action, int button) const {
        return action>0 && action<23 &&
            (binding[action][0]==button+1 || binding[action][1]==button+1);
    }
    static float Value(SDL_GameController *pad, int code) {
        if (!pad || !SDL_GameControllerGetAttached(pad) || code<=0) return 0;
        if(code<100) return SDL_GameControllerGetButton(pad,(SDL_GameControllerButton)(code-1)) ? 1.0f : 0.0f;
        int axis=(code-100)/2;
        float value=SDL_GameControllerGetAxis(pad,(SDL_GameControllerAxis)axis)/32767.0f;
        if((code-100)%2) value=-value;
        float deadzone=axis>=SDL_CONTROLLER_AXIS_TRIGGERLEFT ? .05f : .15f;
        if(value<=deadzone) return 0;
        value=(value-deadzone)/(1-deadzone);
        return value>1 ? 1 : value;
    }
    float Action(SDL_GameController *pad,int action) const {
        if(action<1 || action>22) return 0;
        float a=Value(pad,binding[action][0]),b=Value(pad,binding[action][1]);
        return a>b ? a : b;
    }
    static const char *Name(int code) {
        if(!code) return "---";
        if(code<100) return SDL_GameControllerGetStringForButton((SDL_GameControllerButton)(code-1));
        static const char *names[]={"Left X +","Left X -","Left Y +","Left Y -","Right X +","Right X -","Right Y +","Right Y -","Left trigger","Left trigger -","Right trigger","Right trigger -"};
        return code>=100 && code<112 ? names[code-100] : "---";
    }
};
#endif
