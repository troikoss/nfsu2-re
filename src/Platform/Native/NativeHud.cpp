#include "NativeHud.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace {

void OpsHide(void *object) { FengPackage::Hide(static_cast<FengObject *>(object)); }
void OpsShow(void *object) { FengPackage::Show(static_cast<FengObject *>(object)); }
void OpsRotation(void *object, float degrees) { FengPackage::SetRotation(static_cast<FengObject *>(object), degrees); }
void OpsColour(void *object, unsigned int argb) { FengPackage::SetColour(static_cast<FengObject *>(object), argb); }
void OpsTexture(void *object, unsigned int hash) { FengPackage::SetTexture(static_cast<FengObject *>(object), hash); }
void OpsText(void *object, const char *text) { FengPackage::SetText(static_cast<FengObject *>(object), text); }

const HudObjectOps kOps = {OpsHide, OpsShow, OpsRotation, OpsColour, OpsTexture, OpsText};

unsigned int Rd32(const unsigned char *p) { return p[0] | p[1] << 8 | p[2] << 16 | (unsigned int)p[3] << 24; }

void ShowWithAncestors(FengObject *object) {
    for (; object; object = object->parent) FengPackage::Show(object);
}

}  // namespace

NativeHud::NativeHud() : textures_(0), loaded_(false) { memset(&gauge_, 0, sizeof(gauge_)); }

bool NativeHud::Load(const char *gameDirectory, NativeTextureCache *textures, const char *fontPath, char *error,
                     int errorSize) {
    textures_ = textures;
    std::string base = gameDirectory;
    std::vector<unsigned char> bundle;
    if (!ReadNativeFile(base + "/GLOBAL/InGameFreeRoam.bun", &bundle)) {
        snprintf(error, errorSize, "cannot read InGameFreeRoam.bun");
        return false;
    }
    size_t size = bundle.size();
    NativeImageMap images;
    LoadNativeTextures(&bundle[0], size, &images);
    textures->Add(images);
    std::vector<unsigned char> skins;
    if (ReadNativeFile(base + "/GLOBAL/HUD_CustomTextures_ALL.bin", &skins)) {
        NativeImageMap skinImages;
        LoadNativeTextures(&skins[0], skins.size(), &skinImages);
        textures->Add(skinImages);
        if (getenv("UG2_DEBUG_HUD")) fprintf(stderr, "HUD: %zu skin textures, %zu hud textures\n", skinImages.size(), images.size());
    }
    std::string reason;
    if (!package_.Load(&bundle[0], size, 0, &reason)) {
        snprintf(error, errorSize, "HUD_SingleRace.fng: %s", reason.c_str());
        return false;
    }
    ReadNativeFile(base + "/LANGUAGES/English.bin", &language_);
    if (!font_.Load(fontPath)) fprintf(stderr, "ug2demo: no HUD font found; HUD text is not drawn\n");
    loaded_ = true;
    return true;
}

// English.bin: chunk 0x39000; header dwords at +8: count at +4, hash table offset at +8 and
// string base at +0xc (relative to +8). Entries are (hash, offset) sorted by hash.
std::string NativeHud::Localised(unsigned int hash) const {
    if (language_.size() < 24) return "";
    const unsigned char *d = &language_[0];
    size_t size = language_.size();
    unsigned int count = Rd32(d + 12);
    size_t table = 8 + Rd32(d + 16);
    size_t strings = 8 + Rd32(d + 20);
    if (table + count * 8 > size) return "";
    int lo = 0, hi = (int)count - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        unsigned int h = Rd32(d + table + mid * 8);
        if (h == hash) {
            size_t at = strings + Rd32(d + table + mid * 8 + 4);
            return at < size ? std::string((const char *)d + at) : "";
        }
        if (h < hash) lo = mid + 1;
        else hi = mid - 1;
    }
    return "";
}

void NativeHud::Configure(NativeVehicle *vehicle, int skinIndex) {
    if (!loaded_) return;
    // The free drive feature set shows the gauge cluster; the other features' objects (race
    // timers, leaderboard, minimap...) stay hidden until their features are recovered.
    for (size_t i = 0; i < package_.Objects().size(); ++i) FengPackage::Hide(package_.Objects()[i]);
    HudGaugeObjects &o = gauge_.objects;
    o.needle30 = package_.Find("3rdPersonNeedle");
    o.rpmNumbers50 = package_.Find("3rdPersonTachRPMNumbers");
    o.backing54 = package_.Find("Drift_Angle_Backing2");
    o.speed58 = package_.Find("3rdPersonSpeed");
    o.speedUnits5C = package_.Find("3rdPersonSpeedUnits");
    o.gear64 = package_.Find("3rdPersonGear");
    o.shiftLight78 = package_.Find("Shift_light");
    o.redline7C = package_.Find("Redline");
    o.tachFill80 = package_.Find("tach_fill");
    o.turboLines84 = package_.Find("3rdperson_Turbolines");
    o.turboFill88 = package_.Find("3rdperson_Turbofill");
    o.turboDial8C = package_.Find("3rdperson_TurboDial");
    gauge_.ops = &kOps;
    const EngineSpec *engine = reinterpret_cast<const EngineSpec *>(vehicle->info->record30.engine1F0);
    gauge_.Configure(engine, skinIndex);
    void *shown[] = {o.needle30, o.rpmNumbers50, o.speed58, o.speedUnits5C, o.gear64, o.redline7C, o.tachFill80,
                     package_.Find("GearBkg")};
    for (size_t i = 0; i < sizeof(shown) / sizeof(shown[0]); ++i) {
        if (shown[i]) ShowWithAncestors(static_cast<FengObject *>(shown[i]));
    }
}

void NativeHud::Update(NativeVehicle *vehicle, bool metric) {
    if (!loaded_) return;
    const EngineSpec *engine = reinterpret_cast<const EngineSpec *>(vehicle->info->record30.engine1F0);
    std::string units = Localised(metric ? 0x8569a25f : 0x8569ab44);
    gauge_.Update(vehicle->owner, engine, metric, units.c_str());
}

void NativeHud::Draw(int width, int height) {
    if (!loaded_) return;
    package_.Draw(textures_, &font_, width, height);
}
