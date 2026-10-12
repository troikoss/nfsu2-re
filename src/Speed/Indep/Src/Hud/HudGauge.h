#ifndef UG2_HUD_GAUGE_H
#define UG2_HUD_GAUGE_H

// Recovered UG2 PC v1.2 HUD tachometer/speedometer feature (HUD_SingleRace.fng objects):
// skin selection 0x5ed9b0, dial type 0x60fe10 and needle angle 0x4aa0e0 for the feature built by
// 0x4f5100. Its per-frame update 0x4c68d0 is HudGaugeFeature::Update (HudFreeroam.cpp). The FEng object operations are reached through a small
// operations table because the FEng object model itself is not recovered.
//
// The gauge update, shift light and drivetrain readers live in HudFreeroam.cpp.
// The HUDINDEX skin option remains a seam.

#include "../Physics/Engine.h"
#include "../Physics/VehicleMover.h"

struct HudObjectOps {
    void (*hide)(void *object);                     // 0x50ca00
    void (*show)(void *object);                     // 0x50ca50
    void (*setRotation)(void *object, float degrees);  // 0x51ced0
    void (*setColour)(void *object, unsigned int argb);  // 0x51d7f0
    void (*setTexture)(void *object, unsigned int hash); // 0x50c990
    void (*setText)(void *object, const char *text);     // 0x537be0 with a printf-style result
    void (*setPosition)(void *object, float x, float y);   // 0x51d4d0
    void (*getPosition)(void *object, float *x, float *y); // 0x51d620
    void (*getSize)(void *object, float *x, float *y);     // 0x51d730
    // 0x5179d0 with no relative flag: state key 5 is the top-left UV pair, 6 the bottom-right.
    void (*setState)(void *object, int key, const float *value);
};

// The texture hashes 0x5ed9b0 produces for a dial skin (output +0 .. +0x1c).
struct HudDialSkin {
    unsigned int lines0;     // 3RDPERSON_<n>LINES_CUSTOM_<skin>
    unsigned int redline4;   // 3RDPERSON_REDLINE_<rpm>
    unsigned int fill8;      // TACH_FILL_CUSTOM_<skin>
    unsigned int unused0C;
    unsigned int needle10;   // 3RDPERSON_RPMDIAL_CUSTOM_<skin>
    unsigned int turboLines14;
    unsigned int turboFill18;
    unsigned int turboDial1C;
};

// FEng objects of the gauge feature (offsets in the 0xc8-byte feature, by constructor 0x4f5100).
struct HudGaugeObjects {
    void *needle30;
    void *rpmNumbers50;      // the lines image
    void *backing54;
    void *speed58;
    void *speedUnits5C;
    void *gear64;
    void *shiftLight78;
    void *redline7C;
    void *tachFill80;
    void *turboLines84;
    void *turboFill88;
    void *turboDial8C;
};

struct HudGauge {
    HudGaugeObjects objects;
    const HudObjectOps *ops;
    float sweepC4;           // needle sweep in degrees for the engine's maximum RPM
    unsigned char noSkinC0;  // set when the skin lookup produced no lines texture
    // Constructor tail 0x4f5100: dial type, sweep and skin textures for the car's engine spec.
    void Configure(const EngineSpec *engine, int skinIndex);
};

// Original 0x60fe10: dial type 0..7 from the engine's redline (+4) and maximum (+8).
int HudDialType(const EngineSpec *engine);
// Original 0x4aa0e0. The gauge passes car +0xc's +0x7f0 as a fourth argument it never reads.
float HudNeedleAngle(float rpm, float sweep, const EngineSpec *engine, const char *unused);
// Original 0x5ed9b0 for the non-drag layout.
void HudSelectDialSkin(HudDialSkin *skin, const EngineSpec *engine, int skinIndex);
// Original 0x43db50 / 0x43db80: h = h * 33 + c, from 0xffffffff or a given seed.
unsigned int HudNameHash(const char *text, unsigned int seed = 0xffffffffu);

#endif
