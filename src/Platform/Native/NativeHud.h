#ifndef UG2_NATIVE_HUD_H
#define UG2_NATIVE_HUD_H

// Platform scaffolding: the player's HUD built from UG2's own HUD_SingleRace.fng (free drive
// layout, GLOBAL/InGameFreeRoam.bun) and drawn over the 3D view. The gauge logic is the
// recovered feature in Speed/Indep/Src/Hud/HudGauge.cpp.
#include "NativeFng.h"
#include "NativeVehicle.h"
#include "../../Speed/Indep/Src/Hud/HudFreeroam.h"
#include "../../Speed/Indep/Src/Hud/HudController.h"

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
    HudFeatureVisibility gaugeVisibility_;
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
    unsigned int featureHigh_;
    HudMinimap minimap_;
    HudFeatureVisibility mapVisibility_;
    HudFeatureObjectNode mapVisibilityNodes_[7];
    HudMinimapIndicator playerIndicator_;
    HudIndicatorSlot playerSlot_;
    HudPlayerSlot player_;
    std::vector<unsigned char> trackInfo_;
    bool minimapReady_;
    std::vector<unsigned char> language_;
    bool loaded_;
};

#endif
