#ifndef UG2_FENG_COLOUR_H
#define UG2_FENG_COLOUR_H
#include "../Hud/HudVisibility.h"
struct FEngColour {
    int blue0, green4, red8, alphaC;
    unsigned int Pack(); // 0x5054d0
};
UG2_CHECK_SIZE(FEngColour, 0x10);
void FEngCopyColour(FEngColour *colour, HudVisibilityObject *object); // 0x50ccc0
bool FEngColourNear(const FEngColour *value, const FEngColour *other); // 0x504640, AL
unsigned int FEngGetColour(HudVisibilityObject *object); // 0x495570
void FEngSetColour(HudVisibilityObject *object, unsigned int colour); // 0x51d7f0
#endif
