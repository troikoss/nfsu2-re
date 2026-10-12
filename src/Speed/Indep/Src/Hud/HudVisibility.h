#ifndef UG2_HUD_VISIBILITY_H
#define UG2_HUD_VISIBILITY_H

#include "../Physics/Ug2Layout.h"

// Original FEng script (FEScript) prefix: list links, length and current time in milliseconds,
// track table and name hash (script reader 0x5515e0, script set 0x517720).
struct FEScript {
    void *vtable0;
    FEScript *next4;
    FEScript *previous8;
    int lengthC;
    int time10;
    unsigned int flags14;      // 'Sf'
    FEScript *chain18;         // 'Sc'
    int trackCount1C;
    void *tracks20;            // 0x38-byte tracks
    int eventCount24;
    void *events28;            // 12-byte events
    char unidentified2C[4];
    unsigned int hash30;
};
UG2_CHECK_OFFSET(FEScript, time10, 0x10);
UG2_CHECK_OFFSET(FEScript, hash30, 0x30);

// Original 0x38-byte script track prefix, used by state writers.
struct FEScriptTrack {
    unsigned char value0;
    unsigned char value1;
    unsigned char value2;
    unsigned char flags3;
    unsigned char unidentified4[3];
    signed char state7;
    char unidentified8[0x10];
    float base18[4];
    char unidentified28[0x10];
};
UG2_CHECK_OFFSET(FEScriptTrack, base18, 0x18);
UG2_CHECK_SIZE(FEScriptTrack, 0x38);

// Partial original FEng object (FEObject) layout, from the object reader 0x555140 and its users.
// Native FNG objects retain this prefix so recovered routines operate on it directly.
struct HudVisibilityObject {
    void SetColourState(int key, const unsigned int *value, unsigned char add); // 0x517ad0
    void SetVectorState(int key, const float *value, unsigned char add); // 0x5178d0
    char unidentified0[4];
    HudVisibilityObject *next4;
    HudVisibilityObject *previous8;
    unsigned int guidC;        // 'OP' +0
    unsigned int hash10;       // 'Oh'
    char unidentified14[4];
    int type18;                // 1 image, 2 string, 5 group, ...
    unsigned int flags1C;      // bit 0 hidden, 0x400000 dirty
    char unidentified20[2];
    short resource22;          // 'OP' +0xc
    unsigned int texture24;    // image resource hash
    unsigned int value28;
    float *data2C;             // state block: colour, pivot, position, rotation, size (+0x38), ...
    char unidentified30[0x1c];
    FEScript *scripts4C;
    char unidentified50[4];
    FEScript *currentScript54;
    char unidentified58[8];
    int childCount60;
    union {
        HudVisibilityObject *firstChild64;
        unsigned short *text64;
    };
};
UG2_CHECK_OFFSET(HudVisibilityObject, hash10, 0x10);
UG2_CHECK_OFFSET(HudVisibilityObject, type18, 0x18);
UG2_CHECK_OFFSET(HudVisibilityObject, flags1C, 0x1c);
UG2_CHECK_OFFSET(HudVisibilityObject, texture24, 0x24);
UG2_CHECK_OFFSET(HudVisibilityObject, data2C, 0x2c);
UG2_CHECK_OFFSET(HudVisibilityObject, scripts4C, 0x4c);
UG2_CHECK_OFFSET(HudVisibilityObject, currentScript54, 0x54);
UG2_CHECK_OFFSET(HudVisibilityObject, childCount60, 0x60);
UG2_CHECK_OFFSET(HudVisibilityObject, firstChild64, 0x64);
UG2_CHECK_SIZE(HudVisibilityObject, 0x68);

// Original cdecl FEng operations, including dirty-bit conditions and group recursion.
void HudHideObject(HudVisibilityObject *object); // 0x50ca00
void HudShowObject(HudVisibilityObject *object); // 0x50ca50

struct HudFeatureObjectNode {
    HudFeatureObjectNode *next0;
    HudFeatureObjectNode *previous4;
    HudVisibilityObject *object8;
};

// Shared HUD feature prefix. The minimap constructor 0x4f73e0 sets mask10=1,
// mask14=0, and registers TrackMapTargetRing alongside the other map objects.
struct HudFeatureVisibility {
    HudFeatureObjectNode *next0;
    HudFeatureObjectNode *previous4;
    void *package8;
    unsigned int valueC;
    unsigned int mask10;
    unsigned int mask14;
    unsigned int current18;
    unsigned int current1C;
    void ApplyMasks(unsigned int low, unsigned int high); // 0x4c6690
};
UG2_CHECK_OFFSET(HudFeatureObjectNode, object8, 8);
UG2_CHECK_SIZE(HudFeatureObjectNode, 0xc);
UG2_CHECK_OFFSET(HudFeatureVisibility, mask10, 0x10);
UG2_CHECK_OFFSET(HudFeatureVisibility, current1C, 0x1c);
UG2_CHECK_SIZE(HudFeatureVisibility, 0x20);

#endif
