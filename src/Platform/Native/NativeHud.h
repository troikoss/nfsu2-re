#ifndef UG2_NATIVE_HUD_H
#define UG2_NATIVE_HUD_H

// Platform scaffolding: the player's HUD built from UG2's own HUD_SingleRace.fng (free drive
// layout, GLOBAL/InGameFreeRoam.bun) and drawn over the 3D view. The gauge logic is the
// recovered feature in Speed/Indep/Src/Hud/HudGauge.cpp.
#include "NativeFng.h"
#include "NativeVehicle.h"
#include "../../Speed/Indep/Src/Hud/HudGauge.h"

class NativeHud {
public:
    NativeHud();
    // Loads the FNG, its textures and the localisation table.
    bool Load(const char *gameDirectory, NativeTextureCache *textures, const char *fontPath, char *error,
              int errorSize);
    void Configure(NativeVehicle *vehicle, int skinIndex);
    void Update(NativeVehicle *vehicle, bool metric);
    void Draw(int width, int height);
    void Describe() { package_.Describe(); }

private:
    std::string Localised(unsigned int hash) const;
    FengPackage package_;
    NativeFont font_;
    NativeTextureCache *textures_;
    HudGauge gauge_;
    std::vector<unsigned char> language_;
    bool loaded_;
};

#endif
