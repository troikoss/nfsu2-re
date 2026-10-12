#ifndef UG2_TRACK_POSITION_MARKERS_H
#define UG2_TRACK_POSITION_MARKERS_H

// Track position markers: chunk 0x34146 of Tracks\Routes<region>\TrackPosMarkers*.bin, loaded by
// 0x5d8470 into the list at 0x88f740 (loader registered by 0x77e720), and the race-start grid
// 0x5eb5d0 built from the ForwardStartingPosition/BackwardStartingPosition markers.
#include "../Physics/Ug2Layout.h"

// One 0x30-byte marker. The file's first two dwords are replaced by the list links.
struct TrackPositionMarker {
    TrackPositionMarker *next0;
    TrackPositionMarker *previous4;
    unsigned int hash8;      // bStringHash of the marker name
    int valueC;              // grid slot for starting positions
    float position10[3];
    float value1C;
    unsigned short angle20;  // 16-bit heading units
    unsigned short value22;
    int track24;             // track number; 0 queries match any track
    char name28[8];
};
UG2_CHECK_OFFSET(TrackPositionMarker, position10, 0x10);
UG2_CHECK_OFFSET(TrackPositionMarker, angle20, 0x20);
UG2_CHECK_OFFSET(TrackPositionMarker, track24, 0x24);
UG2_CHECK_SIZE(TrackPositionMarker, 0x30);

// List head at 0x88f740; the list object is its own sentinel.
struct TrackPositionMarkerList {
    TrackPositionMarker *head0;
    TrackPositionMarker *tail4;
    TrackPositionMarker *End(); // 0x502fd0
};

void InitialiseTrackPositionMarkers();               // static initializer 0x77e700
int LoadTrackPositionMarkers(unsigned int *chunk);   // 0x5d8470
int UnloadTrackPositionMarkers(unsigned int *chunk); // 0x5d84d0
int CountTrackPositionMarkers(int track, unsigned int hash);                          // 0x5d8520
TrackPositionMarker *GetTrackPositionMarker(int track, unsigned int hash, int index); // 0x5d85d0
int CountTrackPositionMarkers(unsigned int hash);                          // 0x5d8560, current track
int CountTrackPositionMarkers(const char *name);                           // 0x5d8590
TrackPositionMarker *GetTrackPositionMarker(unsigned int hash, int index); // 0x5d8620
TrackPositionMarker *GetTrackPositionMarker(const char *name, int index);  // 0x5d8660

// Road navigator used by the grid fallback (constructor 0x5da2f0). Advancing it needs the road
// network, which is not recovered.
struct WRoadNav {
    char unidentified0[8];
    float position8[2];
    float value10;
    float value14;
    float value18;
    float value1C;
    float value20;
    float value24;
    float direction28[2];
    float value30;
    float value34;
    float value38;
    WRoadNav *Construct(); // 0x5da2f0
    void Advance5DA370(float distance, float lateral, int flag);
    void Destroy();        // 0x4022c0 (shared empty routine)
};
UG2_CHECK_OFFSET(WRoadNav, direction28, 0x28);
UG2_CHECK_SIZE(WRoadNav, 0x3c);

// One 0x20-byte grid entry written by 0x5eb5d0.
struct StartingPosition {
    float position0[3];
    char unidentifiedC[4];
    unsigned short angle10;
    char unidentified12[0xe];
};
UG2_CHECK_OFFSET(StartingPosition, angle10, 0x10);
UG2_CHECK_SIZE(StartingPosition, 0x20);

void GetStartingPositions(int count, StartingPosition *positions); // 0x5eb5d0

#endif
