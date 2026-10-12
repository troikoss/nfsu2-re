#include "TrackPositionMarkers.h"
#include "WCollision.h"
#include "../Hud/HudMinimap.h"
#include "../Physics/ForceFeedback.h"

extern TrackPositionMarkerList trackPositionMarkers88F740;
extern TrackInfo *currentTrack883DAC;
extern GameSession89E7A0 gameSession89E7A0;
extern float gridStartDistance7A55AC; // 100
extern float gridSearchStep784560;    // 10
extern float gridLateral7A55A8;       // 3
extern float gridRowSpacing7A55A4;    // 17

// 0x502fd0 (shared list helper): the list object is its own sentinel.
TrackPositionMarker *TrackPositionMarkerList::End() {
    return reinterpret_cast<TrackPositionMarker *>(this);
}

// Static initializer 0x77e700 (constructor 0x598750 -> 0x57bcd0): an empty list.
void InitialiseTrackPositionMarkers() {
    trackPositionMarkers88F740.head0 = reinterpret_cast<TrackPositionMarker *>(&trackPositionMarkers88F740);
    trackPositionMarkers88F740.tail4 = reinterpret_cast<TrackPositionMarker *>(&trackPositionMarkers88F740);
}

// Original 0x5d8470: append every marker of chunk 0x34146 (16-byte aligned data) to the list.
int LoadTrackPositionMarkers(unsigned int *chunk) {
    if (chunk[0] != 0x34146) return 0;
    unsigned int start = ((unsigned int)chunk + 0x17) & ~0xfu;
    int count = (chunk[1] - start + (unsigned int)chunk + 8) / 0x30;
    TrackPositionMarker *marker = (TrackPositionMarker *)start;
    for (; count > 0; --count) {
        TrackPositionMarker *tail = trackPositionMarkers88F740.tail4;
        tail->next0 = marker;
        trackPositionMarkers88F740.tail4 = marker;
        marker->previous4 = tail;
        marker->next0 = reinterpret_cast<TrackPositionMarker *>(&trackPositionMarkers88F740);
        ++marker;
    }
    return 1;
}

// Original 0x5d84d0: unlink every marker of the chunk.
int UnloadTrackPositionMarkers(unsigned int *chunk) {
    if (chunk[0] != 0x34146) return 0;
    unsigned int start = ((unsigned int)chunk + 0x17) & ~0xfu;
    int count = (chunk[1] - start + (unsigned int)chunk + 8) / 0x30;
    TrackPositionMarker *marker = (TrackPositionMarker *)start;
    for (; count > 0; --count) {
        TrackPositionMarker *next = marker->next0;
        TrackPositionMarker *previous = marker->previous4;
        ++marker;
        previous->next0 = next;
        next->previous4 = previous;
    }
    return 1;
}

// Original 0x5d8520.
int CountTrackPositionMarkers(int track, unsigned int hash) {
    TrackPositionMarker *marker = trackPositionMarkers88F740.head0;
    int count = 0;
    for (; marker != trackPositionMarkers88F740.End(); marker = marker->next0) {
        if (marker->hash8 == hash && (track == 0 || marker->track24 == track)) ++count;
    }
    return count;
}

// Original 0x5d85d0.
TrackPositionMarker *GetTrackPositionMarker(int track, unsigned int hash, int index) {
    TrackPositionMarker *marker = trackPositionMarkers88F740.head0;
    int count = 0;
    for (; marker != trackPositionMarkers88F740.End(); marker = marker->next0) {
        if (marker->hash8 == hash && (track == 0 || marker->track24 == track)) {
            if (count == index) return marker;
            ++count;
        }
    }
    return 0;
}

// Original 0x5d8560.
int CountTrackPositionMarkers(unsigned int hash) {
    if (currentTrack883DAC) return CountTrackPositionMarkers(currentTrack883DAC->number8A, hash);
    return CountTrackPositionMarkers(0, hash);
}

// Original 0x5d8590.
int CountTrackPositionMarkers(const char *name) {
    unsigned int hash = HudNameHash(name);
    if (currentTrack883DAC) return CountTrackPositionMarkers(currentTrack883DAC->number8A, hash);
    return CountTrackPositionMarkers(0, hash);
}

// Original 0x5d8620.
TrackPositionMarker *GetTrackPositionMarker(unsigned int hash, int index) {
    if (currentTrack883DAC) return GetTrackPositionMarker(currentTrack883DAC->number8A, hash, index);
    return GetTrackPositionMarker(0, hash, index);
}

// Original 0x5d8660.
TrackPositionMarker *GetTrackPositionMarker(const char *name, int index) {
    unsigned int hash = HudNameHash(name);
    if (currentTrack883DAC) return GetTrackPositionMarker(currentTrack883DAC->number8A, hash, index);
    return GetTrackPositionMarker(0, hash, index);
}

// Original 0x5da2f0.
WRoadNav *WRoadNav::Construct() {
    position8[0] = 0.0f;
    position8[1] = 0.0f;
    value18 = 0.0f;
    value1C = 0.0f;
    value14 = position8[1];
    *reinterpret_cast<int *>(&value10) = 0;
    value20 = 0.0f;
    value24 = 0.0f;
    direction28[0] = 1.0f;
    direction28[1] = 0.0f;
    value30 = 0.0f;
    value34 = 0.0f;
    value38 = 0.0f;
    return this;
}

// 0x4022c0: the shared empty routine the navigator's destructor resolves to.
void WRoadNav::Destroy() {
}

// Original 0x5eb5d0: `count` race-start positions from the track's numbered starting markers, or
// (when any slot lacks one) laid out along the road ahead; heights come from the collision world.
void GetStartingPositions(int count, StartingPosition *positions) {
    unsigned int hash = HudNameHash(gameSession89E7A0.reversed4 ? "BackwardStartingPosition"
                                                                : "ForwardStartingPosition");
    int available = CountTrackPositionMarkers(hash);
    bool placed = false;
    if (available >= count) {
        int slot = 0;
        for (; slot < count; ++slot) {
            int i = 0;
            TrackPositionMarker *marker = 0;
            for (; i < available; ++i) {
                marker = GetTrackPositionMarker(hash, i);
                if (marker->valueC == slot) break;
            }
            if (i >= available) break;
            positions[slot].position0[0] = marker->position10[0];
            positions[slot].position0[1] = marker->position10[1];
            positions[slot].position0[2] = marker->position10[2];
            positions[slot].angle10 = marker->angle20;
        }
        placed = slot >= count;
    }
    if (!placed) {
        WRoadNav nav;
        nav.Construct();
        for (float distance = 0.0f; distance < gridStartDistance7A55AC;) {
            distance = distance + gridSearchStep784560;
            nav.Advance5DA370(distance, 0.0f, 0);
        }
        for (int i = 0; i < count; ++i) {
            float lateral = i % 2 == 0 ? gridLateral7A55A8 : -gridLateral7A55A8;
            nav.Advance5DA370((float)(i / 2) * gridRowSpacing7A55A4 + gridStartDistance7A55AC, lateral, 0);
            positions[i].position0[0] = nav.position8[0];
            positions[i].position0[1] = nav.position8[1];
            positions[i].position0[2] = 0.0f;
            positions[i].angle10 = HudAngleUnits(nav.direction28[0], nav.direction28[1]);
        }
        nav.Destroy();
    }
    WCollider collider;
    collider.Construct();
    for (StartingPosition *position = positions; count; --count, ++position) {
        collider.UpdatePack(position->position0, 0.0f);
        if (collider.pack8) {
            position->position0[2] =
                collider.QueryGround(reinterpret_cast<WheelVector *>(position->position0), 0, 0, 0);
        }
    }
    collider.Destruct();
}
