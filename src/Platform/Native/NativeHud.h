#ifndef UG2_NATIVE_HUD_H
#define UG2_NATIVE_HUD_H

// Platform scaffolding: the player's HUD built from UG2's own HUD_SingleRace.fng (free drive
// layout, GLOBAL/InGameFreeRoam.bun) and drawn over the 3D view. The gauge logic is the
// recovered feature in Speed/Indep/Src/Hud/HudGauge.cpp.
#include "NativeFng.h"
#include "NativeVehicle.h"
#include "../../Speed/Indep/Src/Hud/HudFreeroam.h"
#include "../../Speed/Indep/Src/Hud/HudController.h"
#include "../../Speed/Indep/Src/Hud/HudPlayerMasks.h"

// Native camera seam: the demo's camera is native, so the player's camera mover (+0x238)
// is this stand-in reporting the attached chase mover the original installs (kind 1 from
// 0x451660 after 0x444040). Slot +0x28 is the kind-1 method 0x404800 (returns false).
class NativeChaseCameraMover : public CameraMover {
public:
    NativeChaseCameraMover();
    void Virtual00();
    void Virtual04();
    void Virtual08();
    void Virtual0C();
    void Virtual10();
    void Virtual14();
    void Virtual18();
    void Virtual1C();
    void Virtual20();
    void Virtual24();
    bool Query28();
};

// English.bin lookup by string hash (chunk 0x39000, sorted (hash, offset) table).
std::string LookupNativeString(const std::vector<unsigned char> &language, unsigned int hash);

class NativeHud {
public:
    NativeHud();
    ~NativeHud();
    // Loads the FNG, its textures and the localisation table.
    bool Load(const char *gameDirectory, NativeTextureCache *textures, const char *fontPath, char *error,
              int errorSize);
    void Configure(NativeVehicle *vehicle, int skinIndex);
    void Update(NativeVehicle *vehicle, bool metric, float seconds);
    // Boundary for a recovered career/world producer; no synthetic wallet or region.
    void SetCareerHudState(int balance, unsigned int regionHash);
    std::string Localised(unsigned int hash) const;
    void Draw(int width, int height);
    void Describe() { package_.Describe(); }

private:
    FengPackage package_;
    NativeFont font_;
    NativeTextureCache *textures_;
    HudGauge gauge_;
    HudController controller_;
    HudGaugeFeature gaugeVisibility_; // feature 0x4f5100; objects from gauge_ setup
    HudFeatureObjectNode gaugeVisibilityNodes_[18];
    HudTurboFeature turbo_;
    HudFeatureObjectNode turboNodes_[6];
    HudNitrousFeature nitrous_;
    HudFeatureObjectNode nitrousNodes_[4];
    HudRegionFeature region_;
    HudFeatureObjectNode regionNodes_[1];
    HudRegionPlayer regionPlayer_;
    HudCashFeature cash_;
    HudFeatureObjectNode cashNodes_[4];
    HudRewardFeature reward_;
    HudFeatureObjectNode rewardNodes_[1];
    HudMinimap minimap_;
    HudFeatureVisibility mapVisibility_;
    HudFeatureObjectNode mapVisibilityNodes_[7];
    HudMinimapIndicator playerIndicator_;
    HudIndicatorSlot playerSlot_;
    // Native storage for the player record published at 0x8900ac/0x8900b4.
    PlayerRecord player_;
    NativeChaseCameraMover camera_;
    std::vector<unsigned char> trackInfo_;
    bool minimapReady_;
    std::vector<unsigned char> language_;
    bool loaded_;
};

#endif
