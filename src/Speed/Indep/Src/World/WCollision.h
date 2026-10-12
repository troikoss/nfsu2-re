#ifndef UG2_WCOLLISION_H
#define UG2_WCOLLISION_H

#include "../Physics/WheelMath.h"
#include "../Physics/Ug2Layout.h"

// Partial PC x86 layouts of UG2's track collision ("topology") structures.

struct WCollisionLink {
    WCollisionLink *next;
    WCollisionLink *previous;
};

// Compressed polygon in chunk 0x34134, 0x20 bytes.
struct WCollisionPolygon {
    unsigned char group0;
    unsigned char extents1;
    unsigned char surface2;
    unsigned char flags3;
    short heightBase4;
    short cacheIndex6;
    short x8[4];
    short y10[4];
    short z18[4];
    void Decode(float *vertices);
};

struct WCollisionRef {
    unsigned short polygon;
    unsigned short mask;
};

struct WCollisionCell {
    unsigned short start;
    unsigned short count;
};

struct WCollisionFace;
struct WCollider;

// Per-section collision pack (chunk 0x80034130), pool 0x883d98 entries of 0x70 bytes.
struct WCollisionPack {
    WCollisionLink link0;
    WCollisionPolygon *polygons8;
    int polygonCountC;
    short *values10;
    int valueCount14;
    WCollisionRef *refs18;
    int refCount1C;
    WCollisionCell *cells20;
    int cellCount24;
    char unidentified28[8];
    short section30;
    unsigned char value32;
    unsigned char value33;
    short columns34;
    short rows36;
    float originX38;
    float originY3C;
    float minimum40[3];
    char unidentified4C[4];
    float maximum50[3];
    char unidentified5C[4];
    WCollisionLink faces60;
    WCollisionLink colliders68;
    void Fixup(int *chunk);
    WCollisionFace *GetFace(WCollisionPolygon *polygon);
    int ReleaseUnusedFaces(int count);
    void ShrinkColliderAreas();
    int FillArea(WCollider *collider, float *minimum, float *maximum);
};

// Expanded polygon in the 0x400-entry cache at 0x883d90, 0x80 bytes.
struct WCollisionFace {
    WCollisionLink link0;
    signed char references8;
    unsigned char vertexCount9;
    unsigned char flagsA;
    signed char surfaceB;
    WCollisionPolygon *source0C;
    int value10;
    float minimumX14;
    float minimumY18;
    float maximumX1C;
    float maximumY20;
    char unidentified24[0xc];
    float vertices30[4][4];
    WheelVector normal70;
    float distance7C;
    WCollisionFace *Expand(WCollisionPolygon *polygon);
    float EdgeDistance(float *point);
    float PlanarDistance(float *point, float limit);
};

struct WBarrierGroup {
    char unidentified0[8];
    signed char enabled8;
    char unidentified9[0x17];
};

// Global collision manager at 0x88f768 (constructor 0x5e81d0).
struct WCollisionManager {
    WCollisionLink packs0;
    WCollisionPack *sections8[0x208];
    int barrierCount828;
    WBarrierGroup *barriers82C;
    int LoadChunk(int *chunk);
    WCollisionPack *GetPack(short section);
};

struct TrackSection {
    WCollisionLink link0;
    short id8;
    short pointCountA;
    float minimumC[2];
    float maximum14[2];
    char unidentified1C[8];
    float points24[1][2];
    bool Contains(float *point);
    float InteriorDistance(float *point);
    float DistanceTo(float *point, float limit);
};

// Track section topology at 0x883e70 (constructor 0x5e6360).
struct TrackTopology {
    WCollisionLink sections0;
    WCollisionLink otherSections8;
    WCollisionLink records10;
    WCollisionLink unidentified18;
    WCollisionLink records20;
    WCollisionLink records28;
    char unidentified30[0x1c];
    int *header4C;
    int LoadChunk(int *chunk);
    TrackSection *FindSection(float *point);
    TrackSection *FindNearest(float *point, float *distance);
};

struct WSectionTracker {
    float x0;
    float y4;
    TrackSection *section8;
    float radiusC;
    bool Update(float *point, TrackTopology *topology, float radius);
};

// Per-car collider, constructed at car +0x5c4 by 0x5e0b40.
struct WCollider {
    WCollisionLink link0;
    WCollisionPack *pack8;
    WSectionTracker trackerC;
    float boundsMinimum1C[2];
    float boundsMaximum24[2];
    float areaMinimum2C[2];
    float areaMaximum34[2];
    WCollisionFace *areaFaces3C[160];
    int cachedCount2BC;
    WCollisionFace *cached2C0[4];
    char unidentified2D0[0x10];
    int areaCount2E0;
    int areaValue2E4;
    int areaValue2E8;
    int areaValue2EC;
    int groupCount2F0;
    WBarrierGroup *groups2F4[4];
    int value304;
    int value308;
    int value30C;
    int value310;
    void Construct();
    void Destruct(); // 0x5e0bf0
    void ReleaseArea();
    void Release();
    void SetPack(WCollisionPack *pack);
    void RefreshArea();
    void UpdatePack(float *point, float radius);
    bool UpdateArea(float *minimum, float *maximum, WheelVector *velocity);
    float QueryGround(WheelVector *point, int *surface, WheelVector *normal, char *found);
};

UG2_CHECK_OFFSET(WCollisionPack, section30, 0x30);
UG2_CHECK_OFFSET(WCollisionPack, faces60, 0x60);
UG2_CHECK_SIZE(WCollisionPack, 0x70);
UG2_CHECK_OFFSET(WCollisionFace, vertices30, 0x30);
UG2_CHECK_OFFSET(WCollisionFace, normal70, 0x70);
UG2_CHECK_SIZE(WCollisionFace, 0x80);
UG2_CHECK_OFFSET(WCollisionManager, barrierCount828, 0x828);
UG2_CHECK_OFFSET(TrackSection, points24, 0x24);
UG2_CHECK_OFFSET(TrackTopology, header4C, 0x4c);
UG2_CHECK_OFFSET(WCollider, areaFaces3C, 0x3c);
UG2_CHECK_OFFSET(WCollider, cachedCount2BC, 0x2bc);
UG2_CHECK_OFFSET(WCollider, areaCount2E0, 0x2e0);
UG2_CHECK_OFFSET(WCollider, groupCount2F0, 0x2f0);
UG2_CHECK_OFFSET(WCollider, value310, 0x310);

extern WCollisionManager collisionManager;
extern TrackTopology trackTopology;
extern WCollisionLink unpackedColliders;
extern WCollisionLink freeFaces;
extern WCollisionFace *faceCache;
extern void *collisionPackPool;
extern int sectionNumberOffset;

void InitialiseCollisionCache(WCollisionManager *manager, char large);
int CollisionSectionIndex(short section);
void RegisterCollisionPack(WCollisionPack *pack);
void RunWorldStaticInitializers();

#endif
