#include "NativeHud.h"
#ifdef UG2_ENABLE_MODDING
#include "NativeModBridge.h"
#endif

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

extern int hudPlayerCount89E7C0;
extern int trackNumber89E7A0;
extern unsigned char exploreMapMode83AA1C;
extern HudPlayerSlot *hudPlayerSlots8900AC[];
void NativeTrap(const char *what);
void HudMinimapSeam(const char *what) { NativeTrap(what); }

namespace {
NativeHud *activeHud = 0;
NativeTextureCache *hudTextures = 0;
int nativeBankBalance = 0;

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


// Fixed native node storage replaces constructor list allocation (0x440ec0).
// Names, order and masks below come from the respective original constructors.
void RegisterFeature(FengPackage *package, HudFeatureVisibility *feature, HudFeatureObjectNode *nodes,
                     const char *const *names, int count, unsigned int low, unsigned int high) {
    HudFeatureObjectNode *end = reinterpret_cast<HudFeatureObjectNode *>(feature);
    feature->next0 = feature->previous4 = end;
    feature->package8 = package;
    feature->mask10 = low;
    feature->mask14 = high;
    for (int i = 0; i < count; ++i) {
        FengObject *object = package->Find(names[i]);
        if (!object) continue;
        HudFeatureObjectNode *node = &nodes[i];
        node->object8 = object;
        // Native hierarchy seam: reveal parent containers without recursively
        // showing unrelated siblings. Child visibility remains recovered.
        for (FengObject *parent = object->parent; parent; parent = parent->parent)
            parent->flags1C &= ~1u;
        node->next0 = end;
        node->previous4 = feature->previous4;
        feature->previous4->next0 = node;
        feature->previous4 = node;
    }
    feature->ApplyMasks(0, 0);
}

}  // namespace

// Native FEng/career boundaries for recovered HudFreeroam.cpp.
void HudObjectRotation(void *o, float value) { FengPackage::SetRotation(static_cast<FengObject *>(o), value); }
int HudGpsMode() { NativeTrap("HUD GPS route manager 0x501100"); return 0; }
void HudGpsPosition(float *, float *) { NativeTrap("HUD GPS route position 0x5010c0"); }
void HudGpsEvent(void *, int) { NativeTrap("HUD GPS event receiver 0x458520"); }
void HudNotificationPrepare(HudNotificationEntry *) { NativeTrap("HUD notification prepare 0x4c8ac0"); }
void HudNotificationNormal(HudNotificationEntry *, HudPlayerSlot *) { NativeTrap("HUD notification render 0x4c8b80"); }
void HudNotificationType11(HudNotificationEntry *, HudPlayerSlot *) { NativeTrap("HUD notification render 0x4c8dd0"); }
void HudNotificationType13(HudNotificationEntry *, HudPlayerSlot *) { NativeTrap("HUD notification render 0x4abaf0"); }
void HudNotificationType14(HudNotificationEntry *, HudPlayerSlot *) { NativeTrap("HUD notification render 0x4aba30"); }
void HudNotificationSound(int, int) { NativeTrap("HUD notification sound 0x51de30"); }
void HudNotificationTexture(void *o, unsigned int hash) { FengPackage::SetTexture(static_cast<FengObject *>(o), hash); }
void HudObjectTexture(void *o, unsigned int hash) { FengPackage::SetTexture(static_cast<FengObject *>(o), hash); }
void HudObjectPosition(void *o, float x, float y) { FengPackage::SetPosition(static_cast<FengObject *>(o), x, y); }
void HudObjectColour(void *o, unsigned int value) { FengPackage::SetColour(static_cast<FengObject *>(o), value); }
void *HudLookupObject(void *package, unsigned int hash) { return static_cast<FengPackage *>(package)->Find(hash); }
unsigned int HudObjectHash(const char *name) { return FengHash(name); }
const char *HudLocalisedText(unsigned int hash) {
    static std::string text;
    text = activeHud ? activeHud->Localised(hash) : "";
    return text.c_str();
}
void HudObjectScript(void *o, const char *name, int restart) {
    FengPackage::PlayScript(static_cast<FengObject *>(o), name, restart != 0);
}
bool HudCashSuppressed() { return false; } // Native context has no modal career screen.
int HudBankBalance() { return nativeBankBalance; } // Set only through SetCareerHudState.
int HudLanguage() { return 0; } // The native loader currently reads English.bin.
void HudTextPrint(void *o, const char *format, ...) {
    // FEng's %$d inserts digit grouping; libc does not implement that extension.
    std::string text;
    va_list args;
    va_start(args, format);
    for (const char *p = format; *p; ++p) {
        if (*p != '%') { text += *p; continue; }
        ++p;
        if (*p == 's') text += va_arg(args, const char *);
        else if (*p == '$' && p[1] == 'd') {
            ++p;
            char digits[32];
            snprintf(digits, sizeof(digits), "%d", va_arg(args, int));
            std::string number = digits;
            for (int at = (int)number.size() - 3; at > (number[0] == '-' ? 1 : 0); at -= 3)
                number.insert(at, ",");
            text += number;
        } else NativeTrap("FEng text format");
    }
    va_end(args);
    FengPackage::SetText(static_cast<FengObject *>(o), text.c_str());
}

// Native adaptation of the state writes in 0x4c66f0/0x4c67e0. Texture
// lookup and object storage are platform seams; no synthetic bar geometry.
static float BarFraction(float value, float maximum, float start, float end) {
    // Original 0x4a9d60.
    if (0.0f < value && value < maximum) return (end - start) * (value / maximum) + start;
    if (0.0f < value && maximum <= value) return end;
    return start;
}
void HudVerticalBar(void *o, float value, float maximum, float start, float end, int) {
    FengObject *object = static_cast<FengObject *>(o);
    if (!object) return;
    const NativeImage *image = hudTextures->Find(object->textureHash);
    if (!image) NativeTrap("FEng vertical bar texture");
    float height = (float)image->height, inverse = 1.0f / height;
    float v = BarFraction(value, maximum, (height - start) * inverse, end * inverse);
    float size = (1.0f - v) * height;
    object->uv[1] = v;
    object->position[1] += (object->size[1] - size) * 0.5f;
    object->size[1] = size;
    object->flags1C |= 0x400000;
}
void HudHorizontalBar(void *o, float value, float maximum, float start, float end, int) {
    FengObject *object = static_cast<FengObject *>(o);
    if (!object) return;
    const NativeImage *image = hudTextures->Find(object->textureHash);
    if (!image) NativeTrap("FEng horizontal bar texture");
    float width = (float)image->width, inverse = 1.0f / width;
    float v = BarFraction(value, maximum, (width - start) * inverse, end * inverse);
    float size = (1.0f - v) * width;
    object->uv[2] = 1.0f - v;
    object->position[0] -= (object->size[0] - size) * 0.5f;
    object->size[0] = size;
    object->flags1C |= 0x400000;
}

#ifdef UG2_ENABLE_MODDING
namespace {
FengPackage *modHudPackage;
uint32_t modHudGeneration;
Ug2Handle HudHandle(size_t index) { return (uint64_t(modHudGeneration) << 32) | (index + 1); }
uint32_t ModHudCount() { return modHudPackage ? modHudPackage->Objects().size() : 0; }
int ModHudObject(uint32_t index, Ug2HudObject *out) {
    if (!modHudPackage || index >= modHudPackage->Objects().size()) return UG2_ERROR;
    FengObject *object = modHudPackage->Objects()[index];
    out->hash = object->hash; out->type = object->type18;
    out->handle = HudHandle(index); out->parent = 0;
    for (size_t i = 0; i < modHudPackage->Objects().size(); ++i)
        if (modHudPackage->Objects()[i] == object->parent) out->parent = HudHandle(i);
    return UG2_OK;
}
Ug2Handle ModHudFind(uint32_t hash) {
    if (!modHudPackage) return 0;
    for (size_t i = 0; i < modHudPackage->Objects().size(); ++i)
        if (modHudPackage->Objects()[i]->hash == hash) return HudHandle(i);
    return 0;
}
int ModHudTranslate(Ug2Handle handle, float x, float y) {
    uint32_t index = uint32_t(handle);
    if (!modHudPackage || uint32_t(handle >> 32) != modHudGeneration || !index || index > modHudPackage->Objects().size()) return UG2_ERROR;
    modHudPackage->SetModTranslation(modHudPackage->Objects()[index - 1], x, y);
    return UG2_OK;
}
struct HudReady {
    ~HudReady() { NativeModsEvent(UG2_HUD_READY); }
};
}
#endif
NativeHud::~NativeHud() {
#ifdef UG2_ENABLE_MODDING
    if (modHudPackage == &package_) {
        modHudPackage = 0; ++modHudGeneration;
        NativeModsHudAdapter(0, 0, 0, 0);
    }
#endif
}

NativeHud::NativeHud() : textures_(0), minimapReady_(false), loaded_(false) {
    activeHud = this;
    featureHigh_ = 0;
    memset(&controller_, 0, sizeof(controller_));
    memset(&gaugeVisibility_, 0, sizeof(gaugeVisibility_));
    memset(&turbo_, 0, sizeof(turbo_));
    memset(&nitrous_, 0, sizeof(nitrous_));
    memset(&region_, 0, sizeof(region_));
    memset(&regionPlayer_, 0, sizeof(regionPlayer_));
    memset(&cash_, 0, sizeof(cash_));
    memset(&reward_, 0, sizeof(reward_));
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
    hudTextures = textures;
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
    if (!font_.Load(gameDirectory, textures, fontPath)) fprintf(stderr, "ug2demo: no HUD font found; HUD text is not drawn\n");
    loaded_ = true;
#ifdef UG2_ENABLE_MODDING
    modHudPackage = &package_; ++modHudGeneration;
    NativeModsHudAdapter(ModHudCount, ModHudObject, ModHudFind, ModHudTranslate);
#endif
    return true;
}

// English.bin: chunk 0x39000; header dwords at +8: count at +4, hash table offset at +8 and
// string base at +0xc (relative to +8). Entries are (hash, offset) sorted by hash.
std::string LookupNativeString(const std::vector<unsigned char> &language, unsigned int hash) {
    if (language.size() < 24) return "";
    const unsigned char *d = &language[0];
    size_t size = language.size();
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

std::string NativeHud::Localised(unsigned int hash) const { return LookupNativeString(language_, hash); }

void NativeHud::Configure(NativeVehicle *vehicle, int skinIndex) {
    if (!loaded_) return;
#ifdef UG2_ENABLE_MODDING
    HudReady notifyReady;
#endif
    // Native session seam: this demo owns one local player.
    hudPlayerCount89E7C0 = 1;
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
    const EngineSpec *engine = reinterpret_cast<const EngineSpec *>(vehicle->info->parameters10.record30.engine1F0);
    gauge_.Configure(engine, skinIndex);
    const char *gaugeNames[] = {"3rdperson_Turbolines", "3rdperson_Turbofill", "3rdperson_TurboDial",
        "3rdPersonNeedle", "3rdPersonTachRPMNumbers", "3rdPersonSpeed", "3rdPersonSpeedUnits",
        "3rdPersonShiftUp", "3rdPersonGear", "OptimalRPM", "GearBkg", "PerfectShift", "MissedShift",
        "Redline", "tach_fill", "Drift_Angle_Backing2", "tach_fill", "Shift_light"};
    RegisterFeature(&package_, &gaugeVisibility_, gaugeVisibilityNodes_, gaugeNames, 18, 0x10000060, 0);
    gaugeVisibility_.ApplyMasks(0x60, 0);
    const char *turboNames[] = {"3rdperson_Turbofill", "3rdperson_Turbolines", "3rdperson_TurboDial",
                              "3rdperson_TurboRed_1", "3rdperson_TurboRed_2", "Drift_Angle_Backing2"};
    RegisterFeature(&package_, &turbo_, turboNodes_, turboNames, 6, 0x10000060, 0);
    turbo_.fill20 = package_.Find(turboNames[0]);
    turbo_.lines24 = package_.Find(turboNames[1]);
    turbo_.dial28 = package_.Find(turboNames[2]);
    turbo_.red2C = package_.Find(turboNames[3]);
    turbo_.red30 = package_.Find(turboNames[4]);
    turbo_.backing34 = package_.Find(turboNames[5]);
    turbo_.ApplyMasks(0x60, 0);
    const char *nitrousNames[] = {"NOS_Line", "NOS_GameBreaker", "NOS_Mask", "NOS_Fill"};
    RegisterFeature(&package_, &nitrous_, nitrousNodes_, nitrousNames, 4, 0, 0x40000);
    nitrous_.line20 = package_.Find(nitrousNames[0]);
    nitrous_.gamebreaker24 = package_.Find(nitrousNames[1]);
    nitrous_.mask28 = package_.Find(nitrousNames[2]);
    nitrous_.colour2C = 0x96005a7d;
    nitrous_.colour30 = 0x9600b4fa;
    const char *regionNames[] = {"Region_Message"};
    RegisterFeature(&package_, &region_, regionNodes_, regionNames, 1, 1, 0);
    region_.message20 = package_.Find(regionNames[0]);
    region_.region24 = 0; // Constructor 0x4f4e70.
    // Native asset seam: authoring sample strings are not live career events.
    FengPackage::SetText(static_cast<FengObject *>(region_.message20), "");
    region_.ApplyMasks(0, 0);
    const char *cashNames[] = {"Cash_Bank", "Cash_data", "Cash_header", "Show_case_backing"};
    RegisterFeature(&package_, &cash_, cashNodes_, cashNames, 4, 0, 0x80000);
    cash_.bank20 = package_.Find(cashNames[0]);
    cash_.data24 = package_.Find(cashNames[1]);
    cash_.reward28 = 0;
    cash_.animated2C = cash_.hidden2D = 0; // Constructor 0x4f4f10.
    FengPackage::SetText(static_cast<FengObject *>(cash_.bank20), "");
    const char *rewardNames[] = {"Cash_Reward"};
    RegisterFeature(&package_, &reward_, rewardNodes_, rewardNames, 1, 0, 0x80);
    reward_.reward20 = package_.Find(rewardNames[0]);
    FengPackage::SetText(static_cast<FengObject *>(reward_.reward20), "");
    player_.slot.car4 = vehicle->owner;
    // Original caller decides nitrous visibility from the car's capacity tick.
    // Until the full player mask producer is recovered, native setup supplies
    // this bit only for a present, positive recovered capacity.
    int capacity = vehicle->owner->State()->timer414;
    featureHigh_ = capacity != 0 && capacity != 0x7fffffff && capacity > 0 ? 0x40000 : 0;
    nitrous_.ApplyMasks(0, featureHigh_);
    controller_.package10 = &package_;
    controller_.feature18 = &gaugeVisibility_;
    controller_.feature1C = &turbo_;
    controller_.feature28 = &nitrous_;
    controller_.feature74 = 0;
    controller_.feature78 = &reward_;
    controller_.feature80 = &cash_;
    // Set current masks consistently with setup; the dispatcher owns subsequent transitions.
    controller_.current8 = 0x60;
    controller_.currentC = featureHigh_;
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
    player_.slot.car4 = vehicle->owner;
    hudPlayerSlots8900AC[0] = &player_.slot;
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
    controller_.feature5C = &mapVisibility_;
    controller_.current8 |= 1;

}

void NativeHud::Update(NativeVehicle *vehicle, bool metric, float seconds) {
    if (!loaded_) return;
    // Native player/context seam supplies active chase gauges, map, equipped NOS,
    // and career bits only after their state arrives. 0x4ca090 handles transitions.
    controller_.ApplyMasks(0, 0x61, featureHigh_);
    const EngineSpec *engine = reinterpret_cast<const EngineSpec *>(vehicle->info->parameters10.record30.engine1F0);
    std::string units = Localised(metric ? 0x8569a25f : 0x8569ab44);
    gauge_.Update(vehicle->owner, engine, metric, units.c_str());
    HudUpdateShiftLight(vehicle->owner, vehicle->mover->drivetrain4C->indexF0,
                        vehicle->owner->engineValue440, static_cast<FengObject *>(gauge_.objects.shiftLight78));
    turbo_.Update(&player_.slot);
    nitrous_.Update(&player_.slot);
    region_.Update(&regionPlayer_);
    cash_.Update(&player_.slot);
    reward_.Update(&player_.slot);
    package_.AdvanceScripts(seconds);
    if (minimapReady_) minimap_.Update();
}

void NativeHud::SetCareerHudState(int balance, unsigned int regionHash) {
    nativeBankBalance = balance;
    regionPlayer_.region9B8 = regionHash;
    region_.ApplyMasks(1, 0);
    controller_.feature74 = &region_;
    featureHigh_ |= 0x80000 | 0x80;
    cash_.ApplyMasks(0, featureHigh_);
    reward_.ApplyMasks(0, featureHigh_);
}

void NativeHud::Draw(int width, int height) {
    if (!loaded_) return;
    package_.ClearModTranslations();
#ifdef UG2_ENABLE_MODDING
    NativeModsEvent(UG2_PRE_HUD_LAYOUT, 0, width, height);
#endif
    package_.Draw(textures_, &font_, width, height);
}

bool HudPackageScriptActive(void *package, unsigned int hash, const char *name) {
    FengObject *object = static_cast<FengPackage *>(package)->Find(hash);
    if (!object || object->activeScript < 0) return false;
    const FengScript &script = object->scripts[object->activeScript];
    return script.hash == FengHash(name) && object->scriptMilliseconds <= script.duration;
}
void HudPackageScript(void *package, unsigned int hash, const char *name, int restart) {
    FengPackage::PlayScript(static_cast<FengPackage *>(package)->Find(hash), name, restart != 0);
}
void HudPlayerVisibility(void *, unsigned int) { NativeTrap("HUD player feature allocation 0x5f1e10"); }

void *HudLookupAnyObject(void *package, unsigned int hash) { return static_cast<FengPackage *>(package)->Find(hash); }
