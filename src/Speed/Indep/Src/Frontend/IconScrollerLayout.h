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
int FngFormatBuffer(char *buffer, unsigned int size, const char *format, ...); // 0x50c960, formatting boundary
HudVisibilityObject *FngFindImageByHash(const char *package, unsigned int hash); // 0x537a60, remaining lookup boundary
HudVisibilityObject *FngFindNamedImage(const char *package, const char *name); // 0x4953e0
struct ScrollerScrollBar {
    ScrollerScrollBar() {} // Temporary native storage construction; no initialization.
    ScrollerScrollBar(const char *package, const char *prefix, unsigned char vertical,
                      unsigned char scaled, unsigned char value); // 0x542ac0
    unsigned char vertical0, scaled1, deferred2, value3;
    char unidentified4[8];
    float positionC, position10;
    char unidentified14[8];
    float origin1C, origin20, extent24, extent28, handle2C, handle30, step34;
    char unidentified38[8];
    HudVisibilityObject *backing40;
    HudVisibilityObject *handle44;
    HudVisibilityObject *arrow48;
    HudVisibilityObject *arrow4C;
    HudVisibilityObject *end50;
    HudVisibilityObject *end54;
    void SetArrow2Static(); // 0x527660
    void SetArrow1Blink(); // 0x495810
    void UpdateScaled(int page, int count, int selected); // 0x52e200
    void UpdateFixed(int page, int count, int selected); // 0x52e320
    void Update(int page, int count, int selected); // 0x538080
};
UG2_CHECK_OFFSET(ScrollerScrollBar, positionC, 0xc);
UG2_CHECK_OFFSET(ScrollerScrollBar, origin1C, 0x1c);
UG2_CHECK_OFFSET(ScrollerScrollBar, step34, 0x34);
UG2_CHECK_OFFSET(ScrollerScrollBar, backing40, 0x40);
UG2_CHECK_OFFSET(ScrollerScrollBar, handle44, 0x44);
UG2_CHECK_OFFSET(ScrollerScrollBar, arrow48, 0x48);
UG2_CHECK_OFFSET(ScrollerScrollBar, arrow4C, 0x4c);
UG2_CHECK_SIZE(ScrollerScrollBar, 0x58);
#endif
