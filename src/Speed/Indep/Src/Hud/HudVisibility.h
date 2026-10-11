#ifndef UG2_HUD_VISIBILITY_H
#define UG2_HUD_VISIBILITY_H

#include "../Physics/Ug2Layout.h"

// Partial original FEng object layout used by 0x50ca00/0x50ca50. Native FNG
// objects retain this prefix so the recovered routines operate on it directly.
struct HudVisibilityObject {
    char unidentified0[4];
    HudVisibilityObject *next4;
    char unidentified8[0x10];
    int type18;
    unsigned int flags1C;
    char unidentified20[0x40];
    int childCount60;
    HudVisibilityObject *firstChild64;
};
UG2_CHECK_OFFSET(HudVisibilityObject, type18, 0x18);
UG2_CHECK_OFFSET(HudVisibilityObject, flags1C, 0x1c);
UG2_CHECK_OFFSET(HudVisibilityObject, childCount60, 0x60);
UG2_CHECK_OFFSET(HudVisibilityObject, firstChild64, 0x64);
UG2_CHECK_SIZE(HudVisibilityObject, 0x68);

// Original cdecl FEng operations, including dirty-bit conditions and group recursion.
void HudHideObject(HudVisibilityObject *object); // 0x50ca00
void HudShowObject(HudVisibilityObject *object); // 0x50ca50

struct HudFeatureObjectNode {
    HudFeatureObjectNode *next0;
    HudFeatureObjectNode *previous4;
    HudVisibilityObject *object8;
};

// Shared HUD feature prefix. The minimap constructor 0x4f73e0 sets mask10=1,
// mask14=0, and registers TrackMapTargetRing alongside the other map objects.
struct HudFeatureVisibility {
    HudFeatureObjectNode *next0;
    HudFeatureObjectNode *previous4;
    void *package8;
    unsigned int valueC;
    unsigned int mask10;
    unsigned int mask14;
    unsigned int current18;
    unsigned int current1C;
    void ApplyMasks(unsigned int low, unsigned int high); // 0x4c6690
};
UG2_CHECK_OFFSET(HudFeatureObjectNode, object8, 8);
UG2_CHECK_SIZE(HudFeatureObjectNode, 0xc);
UG2_CHECK_OFFSET(HudFeatureVisibility, mask10, 0x10);
UG2_CHECK_OFFSET(HudFeatureVisibility, current1C, 0x1c);
UG2_CHECK_SIZE(HudFeatureVisibility, 0x20);

#endif
