#include "HudGauge.h"


// Original globals (bound to the mapped retail image by OriginalSymbols.ld).
extern float hudDialSweep798510;
extern float hudDialMaximum798514;   // 7500
extern float hudDialMaximum798518;   // 8500
extern float hudDialMaximum79851C;   // 9500
extern float hudDialMaximum798520;   // 10500
extern float hudDialLimit78C194;     // 9000
extern float hudDialLimit79C434;     // 8000
extern float hudDialLimit78ABF8;     // 7000
extern float hudDialLimit7A845C;     // 6500
extern float hudDialLimit789FE0;     // 6000
extern float hudSpeedScaleMetric7904FC;
extern float hudSpeedScaleImperial78426C;
extern float hudHalfTurn78450C;      // 360
extern signed char hudGearCharacters7F66A4[20];


// Minimal printf replacements: verification compiles run without system include paths.
static char *AppendText(char *out, const char *text) {
    while (*text) *out++ = *text++;
    return out;
}

// "%02d" of a value below 100.
static char *AppendTwoDigits(char *out, int value) {
    *out++ = (char)('0' + (value / 10) % 10);
    *out++ = (char)('0' + value % 10);
    return out;
}

// Name `base` followed by "_%02d" (the skin formats all end in CUSTOM_%02d).
static void BuildSkinName(char *name, const char *base, int skin) {
    char *out = AppendText(name, base);
    out = AppendTwoDigits(out, skin);
    *out = 0;
}

// "%3d": right aligned in three columns.
static void FormatSpeed(char *out, int value) {
    if (value < 0) value = 0;
    char digits[12];
    int count = 0;
    do {
        digits[count++] = (char)('0' + value % 10);
        value /= 10;
    } while (value);
    for (int pad = count; pad < 3; ++pad) *out++ = ' ';
    while (count) *out++ = digits[--count];
    *out = 0;
}

unsigned int HudNameHash(const char *text, unsigned int seed) {
    unsigned int hash = seed;
    for (; *text; ++text) {
        hash = hash * 33 + (unsigned char)*text;
    }
    return hash;
}

// Original 0x60fe10.
int HudDialType(const EngineSpec *engine) {
    float redline = engine->value4;
    float maximum = engine->value8;
    if (hudDialLimit78C194 <= maximum && hudDialMaximum798518 <= redline) {
        if (hudDialLimit78C194 <= redline) {
            return 0;
        }
        return 1;
    }
    if (maximum < hudDialLimit79C434 || redline < hudDialMaximum798514) {
        if (maximum < hudDialLimit78ABF8 || redline < hudDialLimit7A845C) {
            int type = 6;
            if (redline < hudDialLimit789FE0) {
                type = 7;
            }
            return type;
        }
        int type = 4;
        if (redline < hudDialLimit78ABF8) {
            return 5;
        }
        return type;
    }
    int type = 2;
    if (redline < hudDialLimit79C434) {
        return 3;
    }
    return type;
}

// Original 0x4aa0e0.
float HudNeedleAngle(float rpm, float sweep, const EngineSpec *engine) {
    float fraction = rpm / engine->value8;
    if (0.0f <= fraction) {
        if (1.0f < fraction) {
            fraction = 1.0f;
        }
    } else {
        fraction = 0.0f;
    }
    if (sweep <= 0.0f) {
        sweep = sweep + hudHalfTurn78450C;
    }
    float angle = sweep * fraction;
    if (hudHalfTurn78450C < angle) {
        return angle - hudHalfTurn78450C;
    }
    if (angle < 0.0f) {
        angle = hudHalfTurn78450C - angle;
    }
    return angle;
}

// Original 0x5ed9b0, layout without the drag HUD.
void HudSelectDialSkin(HudDialSkin *skin, const EngineSpec *engine, int skinIndex) {
    char name[0x20];
    skin->lines0 = skin->redline4 = skin->fill8 = skin->unused0C = 0;
    skin->needle10 = skin->turboLines14 = skin->turboFill18 = skin->turboDial1C = 0;
    BuildSkinName(name, "TACH_FILL_CUSTOM_", skinIndex);
    skin->fill8 = HudNameHash(name);
    unsigned int prefix = HudNameHash("3RDPERSON_");
    BuildSkinName(name, "3RDPERSON_TURBODIAL_CUSTOM_", skinIndex);
    skin->turboDial1C = HudNameHash(name);
    BuildSkinName(name, "3RDPERSON_TURBOFILL_CUSTOM_", skinIndex);
    skin->turboFill18 = HudNameHash(name);
    BuildSkinName(name, "3RDPERSON_TURBOLINES_CUSTOM_", skinIndex);
    skin->turboLines14 = HudNameHash(name);
    BuildSkinName(name, "3RDPERSON_RPMDIAL_CUSTOM_", skinIndex);
    skin->needle10 = HudNameHash(name);
    int type = HudDialType(engine);
    static const char *const lines[4] = {"3RDPERSON_10500LINES_CUSTOM_", "3RDPERSON_9500LINES_CUSTOM_",
                                         "3RDPERSON_8500LINES_CUSTOM_", "3RDPERSON_7500LINES_CUSTOM_"};
    static const char *const redlines[8] = {"REDLINE_9000", "REDLINE_8500", "REDLINE_8000", "REDLINE_7500",
                                            "REDLINE_7000", "REDLINE_6500", "REDLINE_6000", "REDLINE_5500"};
    if (type < 0 || type > 7) {
        return;
    }
    BuildSkinName(name, lines[type / 2], skinIndex);
    skin->lines0 = HudNameHash(name);
    skin->redline4 = HudNameHash(redlines[type], prefix);
}

// Constructor tail of 0x4f5100: sweep = (maximum / dial scale) * 235 degrees.
void HudGauge::Configure(const EngineSpec *engine, int skinIndex) {
    HudDialSkin skin;
    HudSelectDialSkin(&skin, engine, skinIndex);
    noSkinC0 = skin.lines0 == 0;
    if (skin.lines0) ops->setTexture(objects.rpmNumbers50, skin.lines0);
    if (skin.fill8) ops->setTexture(objects.tachFill80, skin.fill8);
    if (skin.needle10) ops->setTexture(objects.needle30, skin.needle10);
    if (skin.turboLines14) ops->setTexture(objects.turboLines84, skin.turboLines14);
    if (skin.turboDial1C) ops->setTexture(objects.turboDial8C, skin.turboDial1C);
    if (skin.turboFill18) ops->setTexture(objects.turboFill88, skin.turboFill18);
    ops->setTexture(objects.redline7C, skin.redline4);
    float scale = 0.0f;
    switch (HudDialType(engine)) {
    case 0:
    case 1:
        scale = hudDialMaximum798520;
        break;
    case 2:
    case 3:
        scale = hudDialMaximum79851C;
        break;
    case 4:
    case 5:
        scale = hudDialMaximum798518;
        break;
    case 6:
    case 7:
        scale = hudDialMaximum798514;
        break;
    }
    sweepC4 = (engine->value8 / scale) * hudDialSweep798510;
}

// Original 0x4c68d0 for an active single-player gauge.
void HudGauge::Update(MoverModeOwner *car, const EngineSpec *engine, bool metric, const char *unitText) {
    float rpm = car->engineValue440;
    float speed = car->speed430;
    if (speed < 0.0f) {
        speed = -speed;
    }
    char text[16];
    ops->setText(objects.speedUnits5C, unitText);
    speed = speed * (metric ? hudSpeedScaleMetric7904FC : hudSpeedScaleImperial78426C);
    FormatSpeed(text, (int)speed);
    ops->setText(objects.speed58, text);
    ops->setRotation(objects.needle30, HudNeedleAngle(rpm, sweepC4, engine));
    if (objects.gear64) {
        char gear = 'X';
        for (int i = 0; i < 10; ++i) {
            if (car->gear434 == hudGearCharacters7F66A4[i * 2]) {
                gear = (char)hudGearCharacters7F66A4[i * 2 + 1];
                break;
            }
        }
        text[0] = gear;
        text[1] = 0;
        ops->setText(objects.gear64, text);
        // 0xffff8040 while the car is not shifting (+0x448), 0xffc3c3c3 during a shift.
        ops->setColour(objects.gear64, car->value448 == 0 ? 0xffff8040u : 0xffc3c3c3u);
    }
    if (noSkinC0) {
        ops->hide(objects.rpmNumbers50);
        ops->hide(objects.redline7C);
    }
}
