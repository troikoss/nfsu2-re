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
    void UpdateFeatures(struct HudPlayerSlot *player, struct MoverModeOwner *car); // 0x4ec5a0
};

// Feature updates dispatched by 0x4ec5a0 whose features are not recovered (thiscall, player).
struct HudUnrecoveredFeature : HudFeatureVisibility {
    void Update4C7090(struct HudPlayerSlot *player); // +0x34
    void Update4EBB10(struct HudPlayerSlot *player); // +0x20
    void Update4DA590(struct HudPlayerSlot *player); // +0x2c
    void Update4C7F00(struct HudPlayerSlot *player); // +0x30
    void Update4AA1B0(struct HudPlayerSlot *player); // +0x3c
    void Update4DA190(struct HudPlayerSlot *player); // +0x40
    void Update4AAFC0(struct HudPlayerSlot *player); // +0x44
    void Update4DA090(struct HudPlayerSlot *player); // +0x48
    void Update4DA900(struct HudPlayerSlot *player); // +0x4c
    void Update4EBEC0(struct HudPlayerSlot *player); // +0x50
    void Update4AB5B0(struct HudPlayerSlot *player); // +0x54
    void Update4C9210(struct HudPlayerSlot *player); // +0x64
    void Update4DABD0(struct HudPlayerSlot *player); // +0x6c
    void Update4C9CE0(struct HudPlayerSlot *player); // +0x70
    void Update4DB8C0();                             // +0x7c
};
UG2_CHECK_SIZE(HudController, 0x84);
UG2_CHECK_OFFSET(HudController, feature5C, 0x5c);

void *HudLookupAnyObject(void *package, unsigned int hash); // 0x5379c0

bool HudPackageScriptActive(void *package, unsigned int object, const char *script); // 0x537cc0
void HudPackageScript(void *package, unsigned int object, const char *script, int restart); // 0x537c00
void HudPlayerVisibility(void *player, unsigned int mask); // 0x5f1e10
#endif
