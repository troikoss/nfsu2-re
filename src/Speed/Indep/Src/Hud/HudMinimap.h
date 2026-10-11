#ifndef UG2_HUD_MINIMAP_H
#define UG2_HUD_MINIMAP_H

// Recovered UG2 PC v1.2 HUD minimap (HUD_SingleRace.fng objects Track2000_map, Minimap_Mask,
// Minimap_North_indicator and the *INDICATOR_%d icons): the per-frame update 0x4ca840 of the
// minimap world object (global 0x8364d4, built by 0x4ec950), its indicator helpers 0x4ca800,
// 0x4ca6d0, 0x4ac2b0 and 0x4ac460, the angle helpers 0x43c490/0x43e6d0 and the TrackInfo table
// 0x5d3eb0/0x5d3e40.
//
// The FEng object operations go through HudObjectOps (HudGauge.h). Not recovered and left as seams:
// AI racer indicators (type 3: 0x5f0e30, 0x5eef60, 0x5ec060), the career map icons (types 5..0x12:
// 0x500b60, 0x502420, 0x4ae930, 0x529410, 0x495570), the indicator pools and list allocation
// (0x4db900, 0x4ec730, 0x4ec790, 0x4ec820, 0x4ec8c0, 0x43f8f0) and the GPS part of the feature
// update 0x4da640 (0x4c8410, 0x500400).

#include "HudGauge.h"
#include "../Physics/Ug2Layout.h"

// One 0x128-byte record of the TrackInfo chunk 0x34201 (GLOBALB.BUN).
struct TrackInfo {
    char unidentified0[0x8a];
    short number8A;               // the track number (TRACKMAP%d, 0x89e7a0)
    char unidentified8C[0x20];
    float mapOriginAC;            // world x at the left edge of TRACKMAP
    float mapOriginB0;            // world y at the top edge
    float mapScaleB4;             // world units across the map texture
    char unidentifiedB8[8];
    float mapZoomC0;              // the minimap shows 0.3 / zoom of the map texture
    char unidentifiedC4[0x64];
};
UG2_CHECK_OFFSET(TrackInfo, number8A, 0x8a);
UG2_CHECK_OFFSET(TrackInfo, mapOriginAC, 0xac);
UG2_CHECK_OFFSET(TrackInfo, mapZoomC0, 0xc0);
UG2_CHECK_SIZE(TrackInfo, 0x128);

// Original 0x5d3eb0: takes the TrackInfo chunk (header included); returns 1 for chunk 0x34201.
int LoadTrackInfoChunk(unsigned int *chunk);
// Original 0x5d3e40: the record with a track number, or 0.
TrackInfo *FindTrackInfo(int number);

// Original 0x43c490: arcsine of a ratio in 16-bit angle units (0x4000 is a quarter turn), through
// the table at 0x7ea628.
unsigned short HudArcSineUnits(float ratio);
// Original 0x43e6d0: direction of (x, y) in 16-bit angle units, counter-clockwise from +x.
unsigned short HudAngleUnits(float x, float y);

// The {FEng object, indicator type} pairs 0x4db900 registers per *INDICATOR_%d object (types from
// the table at 0x7f6828: 1 start line, 3 AI racer, 4 player car, 5..0x12 career map icons).
struct HudIndicatorSlot {
    void *object0;
    int type4;
};

// One indicator of the minimap list (0x4ec790, 0x20 bytes).
struct HudMinimapIndicator {
    HudMinimapIndicator *next0;
    HudMinimapIndicator *previous4;
    int id8;                      // player index for type 4, racer for type 3
    float x0C;                    // fixed world position (types 1, 2, 5..0x12)
    float y10;
    float angle14;                // heading in degrees
    unsigned int flags18;         // bit 0 hides beyond the rim, bit 1 hides
    HudIndicatorSlot *slot1C;
    // Original 0x4ac2b0: world position and heading direction (cos, sin of the heading for cars).
    void GetPose(float *position, float *direction);
    // Original 0x4ca6d0.
    bool IsShown();
    // Original 0x4ac460: per-type colour/texture refresh.
    void Refresh();
};

UG2_CHECK_SIZE(HudIndicatorSlot, 8);
UG2_CHECK_OFFSET(HudMinimapIndicator, id8, 8);
UG2_CHECK_OFFSET(HudMinimapIndicator, slot1C, 0x1c);
UG2_CHECK_SIZE(HudMinimapIndicator, 0x20);

// The minimap world object (0x1c bytes, global 0x8364d4).
struct HudMinimap {
    HudMinimapIndicator *next0;   // indicator list head
    HudMinimapIndicator *previous4;
    void *map8;                   // Track2000_map
    void *maskC;                  // Minimap_Mask
    void *north10;                // Minimap_North_indicator
    TrackInfo *track14;
    void *package18;
    const HudObjectOps *ops;      // scaffold: FEng operations (not part of the original object)

    // Original 0x4ca800: the indicator of a type and id, or 0.
    HudMinimapIndicator *FindIndicator(int type, int id);
    // Original 0x4ca840.
    void Update();
};

UG2_CHECK_OFFSET(HudMinimap, map8, 8);
UG2_CHECK_OFFSET(HudMinimap, track14, 0x14);
UG2_CHECK_OFFSET(HudMinimap, ops, 0x1c);

// A player slot of the table at 0x8900ac (+4 is the player's car).
struct HudPlayerSlot {
    char unidentified0[4];
    struct MoverModeOwner *car4;
};

#endif
