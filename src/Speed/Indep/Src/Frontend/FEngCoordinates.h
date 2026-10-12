#ifndef UG2_FENG_COORDINATES_H
#define UG2_FENG_COORDINATES_H
#include "../Hud/HudVisibility.h"

// String-specific extension; +0x64 is text64 rather than the group child pointer.
struct FEStringObject : HudVisibilityObject {
    char unidentified68[4];
    int justification6C;
    int leading70;
    unsigned int maxWidth74;
};
UG2_CHECK_OFFSET(FEStringObject, justification6C, 0x6c);
UG2_CHECK_OFFSET(FEStringObject, leading70, 0x70);
UG2_CHECK_SIZE(FEStringObject, 0x78);

struct FEngFont {
    char unidentified0[0xc];
    unsigned char *fntfC;
    char unidentified10[0x14];
    float lineHeight24;
    float offset28;
    float leadingScale2C;
    float LineWidth(const unsigned short *text, unsigned int flags, unsigned int maxWidth, unsigned char *wrapped); // 0x50bce0, remaining interface
    float TextWidth(const unsigned short *text, unsigned int flags); // 0x50bec0
    float TextHeight(const unsigned short *text, int leading); // 0x50bf40
};
UG2_CHECK_OFFSET(FEngFont, fntfC, 0xc);
UG2_CHECK_OFFSET(FEngFont, lineHeight24, 0x24);
FEngFont *FindFEngFont(unsigned int hash); // 0x51bd70, remaining resource lookup
void *FindFEngGlyph(unsigned short character, const void *glyphs, unsigned int count, int stride); // 0x6f45b0
float __stdcall FEngHorizontalOffset(unsigned char flags, float width); // 0x50c020
float __stdcall FEngVerticalOffset(unsigned char flags, float height); // 0x50c050
bool FEngVectorNear(const float *value, const float *other); // 0x504500, AL
int __stdcall FEngStateOffset(int key); // 0x5046f0
void FEngGetCentre(HudVisibilityObject *object, float *x, float *y); // 0x51d620
void FEngGetSize(HudVisibilityObject *object, float *width, float *height); // 0x51d730
void FEngSetWidth(HudVisibilityObject *object, float width); // 0x495590
void FEngSetHeight(HudVisibilityObject *object, float height); // 0x525db0
void FEngGetTopLeft(HudVisibilityObject *object, float *x, float *y); // 0x51d070
void FEngSetTopLeft(HudVisibilityObject *object, float x, float y); // 0x51d380
void FEngSetCentre(HudVisibilityObject *object, float x, float y); // 0x51d4d0
void FEngSetSize(HudVisibilityObject *object, float width, float height); // 0x50cb00
#endif
