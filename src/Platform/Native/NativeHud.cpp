#include "NativeHud.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int trackNumber89E7A0;
extern unsigned char exploreMapMode83AA1C;
extern HudPlayerSlot *hudPlayerSlots8900AC[];
void NativeTrap(const char *what);
void HudMinimapSeam(const char *what) { NativeTrap(what); }

namespace {

void OpsHide(void *object) { FengPackage::Hide(static_cast<FengObject *>(object)); }
void OpsShow(void *object) { FengPackage::Show(static_cast<FengObject *>(object)); }
void OpsRotation(void *object, float degrees) { FengPackage::SetRotation(static_cast<FengObject *>(object), degrees); }
void OpsColour(void *object, unsigned int argb) { FengPackage::SetColour(static_cast<FengObject *>(object), argb); }
void OpsTexture(void *object, unsigned int hash) { FengPackage::SetTexture(static_cast<FengObject *>(object), hash); }
void OpsText(void *object, const char *text) { FengPackage::SetText(static_cast<FengObject *>(object), text); }

void OpsPosition(void *object, float x, float y) { FengPackage::SetPosition(static_cast<FengObject *>(object), x, y); }
void OpsGetPosition(void *object, float *x, float *y) { FengPackage::GetPosition(static_cast<FengObject *>(object), x, y); }
void OpsSize(void *object, float *x, float *y) { FengPackage::GetSize(static_cast<FengObject *>(object), x, y); }
void OpsState(void *object, int key, const float *value) { FengPackage::SetState(static_cast<FengObject *>(object), key, value); }

const HudObjectOps kOps = {OpsHide, OpsShow, OpsRotation, OpsColour, OpsTexture, OpsText,
                          OpsPosition, OpsGetPosition, OpsSize, OpsState};

unsigned int Rd32(const unsigned char *p) { return p[0] | p[1] << 8 | p[2] << 16 | (unsigned int)p[3] << 24; }

void ShowWithAncestors(FengObject *object) {
    for (; object; object = object->parent) FengPackage::Show(object);
}

}  // namespace

NativeHud::NativeHud() : textures_(0), minimapReady_(false), loaded_(false) {
    memset(&gauge_, 0, sizeof(gauge_));
    memset(&minimap_, 0, sizeof(minimap_));
    memset(&mapVisibility_, 0, sizeof(mapVisibility_));
    memset(mapVisibilityNodes_, 0, sizeof(mapVisibilityNodes_));
    memset(&playerIndicator_, 0, sizeof(playerIndicator_));
    memset(&playerSlot_, 0, sizeof(playerSlot_));
    memset(&player_, 0, sizeof(player_));
}

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
    std::vector<unsigned char> mapPack;
    if (ReadNativeFile(base + "/TRACKS/TRACKMAP4000.BIN", &mapPack)) {
        NativeImageMap mapImages;
        LoadNativeTextures(&mapPack[0], mapPack.size(), &mapImages);
        textures->Add(mapImages);
    }
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
    // Platform asset seam: retain the TrackInfo chunk storage for the recovered loader.
    // The native demo uses the L4RA city world; TrackInfo 4000 describes its map.
    if (ReadNativeFile(base + "/GLOBAL/GLOBALB.BUN", &trackInfo_)) {
        for (size_t offset = 0; offset + 8 <= trackInfo_.size();) {
            unsigned int id = Rd32(&trackInfo_[offset]);
            unsigned int length = Rd32(&trackInfo_[offset + 4]);
            if (length > trackInfo_.size() - offset - 8) break;
            if (id == 0x34201 && length >= sizeof(TrackInfo)) {
                LoadTrackInfoChunk(reinterpret_cast<unsigned int *>(&trackInfo_[offset]));
                break;
            }
            offset += 8 + length;
        }
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
    // The native feature setup selects recovered gauges and the single-player city minimap.
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
    minimap_.map8 = package_.Find(0x90fe80f3u);
    minimap_.maskC = package_.Find(0x40cdc515u);
    minimap_.north10 = package_.Find(0xf2a54430u);
    minimap_.track14 = FindTrackInfo(4000);
    minimap_.package18 = &package_;
    minimap_.ops = &kOps;
    // 0x4db900 registers the objects that exist, then 0x4ec730 takes a type-4
    // slot from that pool. Object suffixes are pool slots, not player indices.
    for (int i = 0; i < 8; ++i) {
        char name[32];
        snprintf(name, sizeof(name), "PLAYERCARINDICATOR_%d", i);
        playerSlot_.object0 = package_.Find(name);
        if (playerSlot_.object0) break;
    }
    playerSlot_.type4 = 4;
    minimapReady_ = minimap_.track14 && minimap_.map8 && minimap_.maskC && minimap_.north10 && playerSlot_.object0 &&
                    textures_->Find(HudNameHash("TRACKMAP4000"));
    if (!minimapReady_) {
        fprintf(stderr, "ug2demo: city minimap TrackInfo or FEng objects missing\n");
        return;
    }
    static_cast<FengObject *>(minimap_.map8)->imageMask = static_cast<FengObject *>(minimap_.maskC);
    // Allocation/list seam: original 0x4ec950 adds both players and race/career icons.
    // This demo owns one player node and has no AI, race or career icon population.
    HudMinimapIndicator *end = reinterpret_cast<HudMinimapIndicator *>(&minimap_);
    minimap_.next0 = minimap_.previous4 = &playerIndicator_;
    playerIndicator_.next0 = playerIndicator_.previous4 = end;
    playerIndicator_.id8 = 0;
    playerIndicator_.slot1C = &playerSlot_;
    player_.car4 = vehicle->owner;
    hudPlayerSlots8900AC[0] = &player_;
    trackNumber89E7A0 = 4000;
    exploreMapMode83AA1C = 2;
    // Core-map membership and required masks from constructor 0x4f73e0.
    // Native allocation seam: fixed storage replaces 0x440ec0 list-node allocation.
    const char *mapNames[] = {"Track2000_map", "TrackMapTargetRing", "Minimap_Mask",
                             "StartLineIndicator", "PlayerCarIndicator_1",
                             "PlayerCarIndicator_2", "Minimap_North_indicator"};
    HudFeatureObjectNode *endVisibility = reinterpret_cast<HudFeatureObjectNode *>(&mapVisibility_);
    mapVisibility_.next0 = mapVisibility_.previous4 = endVisibility;
    mapVisibility_.package8 = &package_;
    mapVisibility_.mask10 = 1;
    mapVisibility_.mask14 = 0;
    for (size_t i = 0; i < sizeof(mapNames) / sizeof(mapNames[0]); ++i) {
        FengObject *object = package_.Find(mapNames[i]);
        if (!object) continue;
        HudFeatureObjectNode *node = &mapVisibilityNodes_[i];
        node->object8 = object;
        node->next0 = endVisibility;
        node->previous4 = mapVisibility_.previous4;
        mapVisibility_.previous4->next0 = node;
        mapVisibility_.previous4 = node;
    }
    mapVisibility_.ApplyMasks(0, 0); // Constructor 0x4f73e0 ends with this call.
    // The full player mask producer 0x600090 is not recovered here. Native free
    // drive supplies minimap bit 0; recovered 0x4c6690 owns object visibility.
    mapVisibility_.ApplyMasks(1, 0);

}

void NativeHud::Update(NativeVehicle *vehicle, bool metric) {
    if (!loaded_) return;
    const EngineSpec *engine = reinterpret_cast<const EngineSpec *>(vehicle->info->record30.engine1F0);
    std::string units = Localised(metric ? 0x8569a25f : 0x8569ab44);
    gauge_.Update(vehicle->owner, engine, metric, units.c_str());
    if (minimapReady_) minimap_.Update();
}

void NativeHud::Draw(int width, int height) {
    if (!loaded_) return;
    package_.Draw(textures_, &font_, width, height);
}
