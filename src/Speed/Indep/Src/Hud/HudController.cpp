#include "HudController.h"
#include "HudFreeroam.h"

extern unsigned char hudDrift89E7D9;
extern int hudDriftOverride890104;
extern int hudRaceType89E7B0;

static void ApplyChanged(HudFeatureVisibility *feature, unsigned int changes, unsigned int bit,
                         unsigned int low, unsigned int high) {
    if (feature && (changes & bit)) feature->ApplyMasks(low, high);
}

static void SelectScript(void *package, const char *object, const char *script) {
    if (!HudPackageScriptActive(package, HudObjectHash(object), script))
        HudPackageScript(package, HudObjectHash(object), script, 1);
}

// Original 0x4ca090. Transition tests use the stripped low mask, while the
// player callback and script choices use the incoming mask, as in the x86.
void HudController::ApplyMasks(void *player, unsigned int low, unsigned int high) {
    unsigned int stripped = low & 0xfdfbffffu;
    unsigned int changesLow = current8 ^ stripped;
    unsigned int changesHigh = currentC ^ high;
    if (player && (changesLow & 0x1000)) HudPlayerVisibility(player, low & 0x1000);
    ApplyChanged(feature18, changesLow, 0x40, stripped, high);
    ApplyChanged(feature1C, changesLow, 0x40, stripped, high);
    ApplyChanged(feature34, changesLow, 0x40, stripped, high);
    ApplyChanged(feature20, changesLow, 0x200, stripped, high);
    ApplyChanged(feature24, changesLow, 0x20000000, stripped, high);
    ApplyChanged(feature28, changesHigh, 0x40000, stripped, high);
    ApplyChanged(feature2C, changesLow, 0x80000000, stripped, high);
    ApplyChanged(feature30, changesLow, 4, stripped, high);
    ApplyChanged(feature5C, changesLow, 1, stripped, high);
    ApplyChanged(feature3C, changesLow, 0x400, stripped, high);
    ApplyChanged(feature40, changesLow, 0x800000, stripped, high);
    if (!hudDrift89E7D9 && !hudDriftOverride890104) {
        HudVisibilityObject *bottom = static_cast<HudVisibilityObject *>(HudLookupAnyObject(package10, HudObjectHash("Leader_bottom")));
        if (low & 0x400) HudHideObject(bottom);
        else HudShowObject(bottom);
    }
    const char *lap = (low & 0x400) ? "LapInfoOn" : "LapInfoOff";
    SelectScript(package10, "LeaderBoardGroup", lap);
    SelectScript(package10, "Leader_bottom", lap);
    if (!(low & 0x800000)) {
        SelectScript(package10, "BestTimeCounter", "LeaderOff");
        if (hudRaceType89E7B0 == 7)
            HudPackageScript(package10, HudObjectHash("BestTimeCounter"), "Getaway", 1);
        SelectScript(package10, "StylePointsGroup", (low & 0x400) ? "LapInfoOnLeaderOff" : "LapInfoOffLeaderOff");
    } else {
        SelectScript(package10, "BestTimeCounter", "LeaderOn");
        SelectScript(package10, "StylePointsGroup", (low & 0x400) ? "LapInfoOnLeaderOn" : "LapInfoOffLeaderOn");
    }
    ApplyChanged(feature48, changesLow, 0x80, stripped, high);
    ApplyChanged(feature4C, changesLow, 0x20000, stripped, high);
    ApplyChanged(feature44, changesLow, 0x4000, stripped, high);
    ApplyChanged(feature54, changesHigh, 0x20000, stripped, high);
    ApplyChanged(feature58, changesHigh, 2, stripped, high);
    ApplyChanged(feature64, changesHigh, 4, stripped, high);
    ApplyChanged(feature6C, changesHigh, 0x40, stripped, high);
    // This feature's visibility subobject starts at +8 (LEA ECX,[EDX+8]).
    if (feature70 && (changesLow & 0x800))
        reinterpret_cast<HudFeatureVisibility *>(reinterpret_cast<char *>(feature70) + 8)->ApplyMasks(stripped, high);
    ApplyChanged(feature78, changesHigh, 0x80, stripped, high);
    ApplyChanged(feature74, changesLow, 1, stripped, high);
    ApplyChanged(feature80, changesHigh, 0x80000, stripped, high);
    current8 = stripped;
    currentC = high;
}

static HudUnrecoveredFeature *Unrecovered(void *feature) {
    return static_cast<HudUnrecoveredFeature *>(feature);
}

// Original 0x4ec5a0: every present feature's per-frame update, in the original order.
// The car argument is not read.
void HudController::UpdateFeatures(HudPlayerSlot *player, MoverModeOwner *) {
    if (feature18) static_cast<HudGaugeFeature *>(feature18)->Update(player);
    if (feature1C) static_cast<HudTurboFeature *>(feature1C)->Update(player);
    if (feature34) Unrecovered(feature34)->Update4C7090(player);
    if (feature20) Unrecovered(feature20)->Update4EBB10(player);
    if (feature28) static_cast<HudNitrousFeature *>(feature28)->Update(player);
    if (feature24) static_cast<HudHeatFeature *>(feature24)->Update(player);
    if (feature2C) Unrecovered(feature2C)->Update4DA590(player);
    if (feature30) Unrecovered(feature30)->Update4C7F00(player);
    if (feature3C) Unrecovered(feature3C)->Update4AA1B0(player);
    if (feature40) Unrecovered(feature40)->Update4DA190(player);
    if (feature44) Unrecovered(feature44)->Update4AAFC0(player);
    if (feature48) Unrecovered(feature48)->Update4DA090(player);
    if (feature4C) Unrecovered(feature4C)->Update4DA900(player);
    if (feature50) Unrecovered(feature50)->Update4EBEC0(player);
    if (feature54) Unrecovered(feature54)->Update4AB5B0(player);
    if (feature58) static_cast<HudNotificationFeature *>(feature58)->Update(player);
    if (feature5C) static_cast<HudGpsFeature *>(feature5C)->UpdateMap(player);
    if (feature64) Unrecovered(feature64)->Update4C9210(player);
    if (feature6C) Unrecovered(feature6C)->Update4DABD0(player);
    if (feature70) Unrecovered(feature70)->Update4C9CE0(player);
    if (feature74) static_cast<HudRegionFeature *>(feature74)->Update(reinterpret_cast<HudRegionPlayer *>(player));
    if (feature78) static_cast<HudRewardFeature *>(feature78)->Update(player);
    if (value7C) static_cast<HudUnrecoveredFeature *>(value7C)->Update4DB8C0();
    if (feature80) static_cast<HudCashFeature *>(feature80)->Update(player);
}
