#include "HudMinimap.h"
#include "../Physics/VehicleMover.h"
#include "../Physics/Engine.h"

extern "C" double __cdecl sqrt(double);
#pragma intrinsic(sqrt)

struct HudArcSineEntry {
    unsigned short angle0;
    unsigned short unused2;
    float slope4;
};

// Original globals (bound to the mapped retail image by OriginalSymbols.ld).
extern HudArcSineEntry hudArcSineTable7EA628[];
extern float hudZero7D7D3C;
extern float hudOne784250;
extern float hudHalf784260;
extern float hudFixedScale784238;      // 65536
extern float hudFixedUnit784278;       // 1 / 65536
extern float hudNegativeFixed786DB4;   // -65536
extern float hudMinimumLength7843EC;   // 1e-4
extern float hudUnitsToDegrees784248;  // 360 / 65536
extern float hudDegreesToRadians784244;
extern float hudCarHeadingScale784240; // 2 pi / 65536
extern float hudMapWindow78758C;       // 0.3
extern float hudMapZoomOverride78E098; // 1.0
extern float hudIndicatorLimit78E090;  // 1.0
extern float hudIndicatorRim78E08C;    // 0.55
extern float hudNorthOffset78E094;     // 270

extern TrackInfo *trackInfoTable883DA0;
extern int trackInfoCount883DA4;
extern int trackNumber89E7A0;
extern int raceActive89E7B0;
extern unsigned char gameFlag89E7D9;
extern int gameFlag890104;
extern unsigned char gameFlag89E7E3;
extern int gameFlag89010C;
extern unsigned char gameFlag89E7E1;
extern unsigned char ambientForced89E7E6;
extern unsigned char exploreMapMode83AA1C;  // 0 off, 1 static, 2 rotate
extern unsigned char raceMapMode83AA1B;
extern int hudMinimapRefresh8364C0;
extern float hudMapLeft8364C4;
extern float hudMapTop8364C8;
extern float hudMapZoom8364CC;
extern float hudMapZoom8364D0;
extern HudPlayerSlot *hudPlayerSlots8900AC[];
extern int hudFlag862838;

// The GPS state object 0x838590 read by 0x501100, and the object 0x85b940 read by 0x501690.
struct HudGpsState {
    char unidentified0[0x28];
    unsigned char latched28;
    unsigned char active29;
};
extern HudGpsState hudGps838590;
extern int hudGpsMode81CBC4;
extern int hudGpsMode81CBC8;
struct HudFlagHolder {
    char unidentified0[8];
    int value8;
};
extern HudFlagHolder hudHolder85B940;

void HudMinimapSeam(const char *what);

namespace {

// Original 0x501100 on the GPS state object.
int GetGpsState(HudGpsState *gps) {
    if (!gps->active29) {
        return 4;
    }
    int state;
    if (hudGpsMode81CBC4 == 1) {
        state = 0;
    } else if (hudGpsMode81CBC4 == 2) {
        gps->latched28 = 1;
        state = 3;
    } else {
        state = 4;
    }
    if (hudGpsMode81CBC8 == 3) {
        state = 2;
    } else if (hudGpsMode81CBC8 == 4) {
        return 1;
    }
    return state;
}

// Original 0x501690.
bool HolderSet(HudFlagHolder *holder) {
    return holder->value8 != 0;
}

}  // namespace

// Original 0x5d3eb0.
int LoadTrackInfoChunk(unsigned int *chunk) {
    if (chunk[0] != 0x34201) {
        return 0;
    }
    trackInfoTable883DA0 = reinterpret_cast<TrackInfo *>(chunk + 2);
    trackInfoCount883DA4 = chunk[1] / 0x128;
    return 1;
}

// Original 0x5d3e40.
TrackInfo *FindTrackInfo(int number) {
    TrackInfo *info = trackInfoTable883DA0;
    for (int i = 0; i < trackInfoCount883DA4; ++i) {
        if (info->number8A == number) {
            return info;
        }
        ++info;
    }
    return 0;
}

// Original 0x43c490.
unsigned short HudArcSineUnits(float ratio) {
    int negative = 0;
    if (ratio < hudZero7D7D3C) {
        ratio = -ratio;
        negative = 1;
    }
    if (!(ratio < hudOne784250)) {
        return (unsigned short)((negative ? 0x8000 : 0) + 0x4000);
    }
    int value = (int)(ratio * hudFixedScale784238);
    int step = 0x8000;
    int bound = 0x8000;
    int level = 0;
    if (value >= bound) {
        do {
            if (level >= 11) {
                break;
            }
            step >>= 1;
            bound += step;
            ++level;
        } while (value >= bound);
    }
    int base = bound - step;
    int index = (value - base) >> (11 - level);
    int sample = (step >> 4) * index + base;
    const HudArcSineEntry *entry = &hudArcSineTable7EA628[level * 16 + index];
    unsigned short angle = (unsigned short)(entry->angle0 -
        (unsigned short)(int)((ratio - (float)sample * hudFixedUnit784278) * entry->slope4 * hudNegativeFixed786DB4));
    if (negative) {
        return (unsigned short)(0x10000 - angle);
    }
    return angle;
}

// Original 0x43e6d0.
unsigned short HudAngleUnits(float x, float y) {
    int quadrant = 0;
    if (x < hudZero7D7D3C) {
        quadrant = 1;
    }
    if (y < hudZero7D7D3C) {
        quadrant ^= 3;
    }
    float ratio = 0.0f;
    float absoluteY = y < 0.0f ? -y : y;
    float absoluteX = x < 0.0f ? -x : x;
    float lengthSquared = x * x + y * y;
    int angle;
    if (absoluteY < absoluteX) {
        if (hudMinimumLength7843EC < lengthSquared) {
            if (y == hudZero7D7D3C) {
                ratio = 0.0f;
            } else {
                ratio = absoluteY / (float)sqrt(lengthSquared);
            }
        }
        angle = HudArcSineUnits(ratio);
    } else {
        if (hudMinimumLength7843EC < lengthSquared) {
            if (x == hudZero7D7D3C) {
                ratio = 0.0f;
            } else {
                ratio = absoluteX / (float)sqrt(lengthSquared);
            }
        }
        angle = 0x4000 - HudArcSineUnits(ratio);
    }
    if (quadrant == 0) {
        return (unsigned short)angle;
    }
    if (quadrant == 3) {
        return (unsigned short)(0x10000 - angle);
    }
    if (quadrant == 1) {
        return (unsigned short)(0x8000 - angle);
    }
    return (unsigned short)(angle + 0x8000);
}

// Original 0x401100: retain the cosine wrapper's float argument boundary.
float HudCosineRadians(float angle) {
    return EngineSine(angle + engineHalfPi);
}

// Original 0x4ac2b0.
void HudMinimapIndicator::GetPose(float *position, float *direction) {
    int type = slot1C ? slot1C->type4 : 0;
    switch (type) {
    case 1:
    case 2: {
        position[0] = x0C;
        position[1] = y10;
        float heading = angle14 * hudDegreesToRadians784244;
        direction[0] = HudCosineRadians(heading);
        direction[1] = EngineSine(angle14 * hudDegreesToRadians784244);
        break;
    }
    case 3:
        HudMinimapSeam("0x4ac2b0 AI racer indicator (0x5f0e30)");
        break;
    case 4: {
        HudPlayerSlot *player = hudPlayerSlots8900AC[id8];
        if (player && player->car4) {
            DrivetrainAuxiliary *car = player->car4->State();
            position[0] = car->origin10.x;
            position[1] = car->origin10.y;
            float heading = (float)car->heading3CC * hudCarHeadingScale784240;
            direction[0] = HudCosineRadians(heading);
            direction[1] = EngineSine(heading);
        }
        break;
    }
    case 5: case 6: case 7: case 8: case 9: case 10: case 11:
    case 12: case 13: case 14: case 15: case 16: case 17: case 18:
        position[0] = x0C;
        position[1] = y10;
        direction[0] = EngineSine(angle14 * hudDegreesToRadians784244 + engineHalfPi);
        direction[1] = EngineSine(angle14 * hudDegreesToRadians784244);
        break;
    }
}

// Original 0x4ca6d0.
bool HudMinimapIndicator::IsShown() {
    int type = 0;
    if (slot1C) {
        type = slot1C->type4;
        if (type > 0x12) {
            return true;
        }
    }
    switch (type) {
    case 3:
        HudMinimapSeam("0x4ca6d0 AI racer indicator");
        return false;
    case 4:
        return true;
    case 5: case 6: case 7: case 8: case 9: case 10: case 11:
    case 12: case 13: case 14: case 15: case 16: case 17: case 18:
        HudMinimapSeam("0x4ca6d0 career map icon");
        return false;
    default:
        return false;
    }
}

// Original 0x4ac460: only AI racers (type 3) and career icons (0xc..0x12) change.
void HudMinimapIndicator::Refresh() {
    if (!slot1C) {
        return;
    }
    int type = slot1C->type4;
    if (type == 3 || (type > 0xb && type < 0x13)) {
        HudMinimapSeam("0x4ac460 indicator refresh");
    }
}

// Original 0x4ca800.
HudMinimapIndicator *HudMinimap::FindIndicator(int type, int id) {
    HudMinimapIndicator *end = reinterpret_cast<HudMinimapIndicator *>(this);
    for (HudMinimapIndicator *node = next0; node != end; node = node->next0) {
        int nodeType = node->slot1C ? node->slot1C->type4 : 0;
        if (nodeType == type && node->id8 == id) {
            return node;
        }
    }
    return 0;
}

// Original 0x4ca840.
void HudMinimap::Update() {
    HudMinimapIndicator *end = reinterpret_cast<HudMinimapIndicator *>(this);
    if (hudMinimapRefresh8364C0) {
        // The original walks the list without doing anything per node.
        hudMinimapRefresh8364C0 = 0;
    }
    if (!track14 || track14->number8A != trackNumber89E7A0) {
        track14 = FindTrackInfo(trackNumber89E7A0);
    }
    TrackInfo *info = track14;
    float zoomX = hudMapWindow78758C / info->mapZoomC0;
    float zoomY = zoomX;
    float scaleX = info->mapScaleB4;
    float scaleY = info->mapScaleB4;
    float originX = info->mapOriginAC;
    float originY = info->mapOriginB0;
    if (hudMapZoomOverride78E098 > hudOne784250) {
        zoomX = hudMapWindow78758C / hudMapZoomOverride78E098;
        zoomY = zoomX;
    }
    float position[2];
    float direction[2];
    FindIndicator(4, 0)->GetPose(position, direction);
    float inverseX = hudOne784250 / scaleX;
    hudMapZoom8364D0 = 5.0f;
    float centreU = (position[0] - originX) * inverseX;
    float inverseY = hudOne784250 / scaleY;
    float centreV = (originY - position[1]) * inverseY;
    float halfX = zoomX * hudHalf784260;
    float left = centreU - halfX;
    float halfY = zoomY * hudHalf784260;
    float top = centreV - halfY;
    hudMapTop8364C8 = top;
    float right = halfX + centreU;
    float bottom = halfY + centreV;
    hudMapLeft8364C4 = left;
    if (raceActive89E7B0 == 0) {
        hudMapZoom8364CC = 5.0f;
    } else if (!gameFlag89E7D9 && gameFlag890104 == 0 && !gameFlag89E7E3 && gameFlag89010C == 0) {
        if (!gameFlag89E7E1 && !ambientForced89E7E6) {
            hudMapZoom8364CC = 5.0f;
        } else {
            hudMapZoom8364CC = 6.6f;
        }
    } else if (trackNumber89E7A0 >= 0x104e && (hudMapZoom8364CC = 5.0f, trackNumber89E7A0 <= 0x1053)) {
    } else {
        hudMapZoom8364CC = 3.3f;
    }
    char name[16] = "TRACKMAP";
    {
        int number = trackNumber89E7A0;
        char digits[12];
        int count = 0;
        do {
            digits[count++] = (char)('0' + number % 10);
            number /= 10;
        } while (number);
        char *out = name + 8;
        while (count) *out++ = digits[--count];
        *out = 0;
    }
    ops->setTexture(map8, HudNameHash(name));
    float uv[2];
    uv[0] = left;
    uv[1] = top;
    ops->setState(map8, 5, uv);
    uv[0] = right;
    uv[1] = bottom;
    ops->setState(map8, 6, uv);
    float mapX, mapY, sizeX, sizeY, unused;
    ops->getPosition(map8, &mapX, &mapY);
    ops->getPosition(map8, &unused, &mapY);
    ops->getSize(map8, &sizeX, &sizeY);
    ops->getSize(map8, &sizeX, &sizeY);
    float rotation = 0.0f;
    if (raceActive89E7B0 == 0) {
        if (exploreMapMode83AA1C == 2) {
            rotation = (float)HudAngleUnits(direction[1], direction[0]) * hudUnitsToDegrees784248;
        }
    } else if (raceMapMode83AA1B == 2) {
        rotation = (float)HudAngleUnits(direction[1], direction[0]) * hudUnitsToDegrees784248;
    }
    ops->setRotation(map8, -rotation);
    ops->setRotation(maskC, -rotation);
    ops->setRotation(north10, ((float)HudAngleUnits(0.0f, 1.0f) * hudUnitsToDegrees784248 - rotation) +
                                  hudNorthOffset78E094);
    HudMinimapIndicator *node = next0;
    if (node == end) {
        return;
    }
    float inverseZoomX = hudOne784250 / zoomX;
    float inverseZoomY = hudOne784250 / zoomY;
    float radians = rotation * hudDegreesToRadians784244;
    float radiansCos = radians + engineHalfPi;
    do {
        float at[2];
        float heading[2];
        node->GetPose(at, heading);
        float relativeX = ((at[0] - originX) * inverseX - centreU) * inverseZoomX;
        float relativeY = ((originY - at[1]) * inverseY - centreV) * inverseZoomY;
        float sine = EngineSine(radians);
        float cosine = EngineSine(radiansCos);
        float limit = hudIndicatorLimit78E090;
        float localX = cosine * relativeX + sine * relativeY;
        float localY = cosine * relativeY - sine * relativeX;
        float distance = (float)sqrt(localY * localY + localX * localX);
        if (raceActive89E7B0 == 0) {
            if (GetGpsState(&hudGps838590) == 3 || GetGpsState(&hudGps838590) == 0 ||
                GetGpsState(&hudGps838590) == 1 || GetGpsState(&hudGps838590) == 2 || hudFlag862838 == 0 ||
                HolderSet(&hudHolder85B940)) {
                limit = 10.0f;
            }
        }
        void *object = node->slot1C ? node->slot1C->object0 : 0;
        if (!(distance < limit) || !node->IsShown() ||
            (hudIndicatorRim78E08C < distance && (node->flags18 & 1) != 0) || ((node->flags18 >> 1) & 1) != 0) {
            ops->setPosition(object, 0.0f, 512.0f);
        } else {
            if (hudIndicatorRim78E08C < distance) {
                float inverse = hudOne784250 / distance;
                localX = localX * inverse * hudIndicatorRim78E08C;
                localY = inverse * localY * hudIndicatorRim78E08C;
            }
            float screenX = sizeX * localX + mapX;
            float screenY = sizeY * localY + mapY;
            float angle = (float)HudAngleUnits(heading[1], heading[0]) * hudUnitsToDegrees784248 - rotation;
            ops->setPosition(object, screenX, screenY);
            ops->setRotation(object, angle);
        }
        node->Refresh();
        node = node->next0;
    } while (node != end);
}
