#ifndef UG2_HUD_CONTROLLER_H
#define UG2_HUD_CONTROLLER_H
#include "HudVisibility.h"

// Original 0x84-byte UI controller, built by 0x4f8bf0. Keep unidentified
// feature slots offset-based; absent features are explicitly null.
struct HudController {
    void *vtable0;
    unsigned int value4;
    unsigned int current8;
    unsigned int currentC;
    void *package10;
    int player14;
    HudFeatureVisibility *feature18, *feature1C, *feature20, *feature24;
    HudFeatureVisibility *feature28, *feature2C, *feature30, *feature34;
    HudFeatureVisibility *feature38, *feature3C, *feature40, *feature44;
    HudFeatureVisibility *feature48, *feature4C, *feature50, *feature54;
    HudFeatureVisibility *feature58, *feature5C, *feature60, *feature64;
    void *object68;
    HudFeatureVisibility *feature6C, *feature70, *feature74, *feature78;
    void *value7C;
    HudFeatureVisibility *feature80;
    void ApplyMasks(void *player, unsigned int low, unsigned int high); // 0x4ca090
};
UG2_CHECK_SIZE(HudController, 0x84);
UG2_CHECK_OFFSET(HudController, feature5C, 0x5c);

void *HudLookupAnyObject(void *package, unsigned int hash); // 0x5379c0

bool HudPackageScriptActive(void *package, unsigned int object, const char *script); // 0x537cc0
void HudPackageScript(void *package, unsigned int object, const char *script, int restart); // 0x537c00
void HudPlayerVisibility(void *player, unsigned int mask); // 0x5f1e10
#endif
