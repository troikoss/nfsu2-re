#ifndef UG2_ICON_SCROLLER_LAYOUT_H
#define UG2_ICON_SCROLLER_LAYOUT_H
#include "../Physics/Ug2Layout.h"
struct ScrollerTween {
    float value0;
    float slope4;
    float target8;
    float targetSlopeC;
    float coefficient10;
    float coefficient14;
    float coefficient18;
    float coefficient1C;
    float time20;
    float duration24;
    float limit28;
    short state2C;
    short mode2E;
    void LimitEndSlope(float limit); // 0x5748c0
    void LimitAcceleration(float limit); // 0x574910
};
UG2_CHECK_SIZE(ScrollerTween, 0x30);
void UpdateScrollerTween(ScrollerTween *tween, float delta, float slopeLimit, float accelerationLimit); // 0x574e30
float ScrollerFadeFactor(float position, float centre, float window, float edge); // 0x50d8d0
struct HudVisibilityObject;
unsigned int FEngGetColour(HudVisibilityObject *object); // 0x495570
void FEngSetColour(HudVisibilityObject *object, unsigned int colour); // 0x51d7f0
unsigned int FngFormatHash(const char *format, ...); // 0x50b9c0, formatting interface
bool FngObjectScriptActive(void *object, const char *name); // 0x51d040
struct IconScrollerIcon;
void UpdateIconLockScripts(IconScrollerIcon *icon, void *lock, void *button); // 0x51df00
struct ScrollerScrollBar { char unidentified0[0x58]; void Update(int page, int count, int selected); }; // 0x538080, remaining interface
#endif
