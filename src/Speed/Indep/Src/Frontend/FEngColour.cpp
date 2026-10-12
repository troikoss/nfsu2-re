#include "FEngColour.h"
#include "FEngCoordinates.h"
extern int fengStateKeys7992F4[];

static unsigned int ClampChannel(int value) {
    if (value < 0) return 0;
    return value > 255 ? 255 : (unsigned int)value;
}

// Original 0x5054d0: R/G/B/A order and signed channel clamps.
unsigned int FEngColour::Pack() {
    unsigned int red = ClampChannel(red8);
    unsigned int green = ClampChannel(green4);
    unsigned int blue = ClampChannel(blue0);
    unsigned int alpha = ClampChannel(alphaC);
    return ((alpha << 8 | red) << 8 | green) << 8 | blue;
}

// Original 0x50ccc0.
void FEngCopyColour(FEngColour *colour, HudVisibilityObject *object) {
    if (object) {
        const int *state = reinterpret_cast<const int *>(object->data2C);
        colour->blue0 = state[0]; colour->green4 = state[1];
        colour->red8 = state[2]; colour->alphaC = state[3];
    } else {
        colour->alphaC = 0; colour->red8 = 0; colour->green4 = 0; colour->blue0 = 0;
    }
}

// Original 0x495570.
unsigned int FEngGetColour(HudVisibilityObject *object) {
    FEngColour colour;
    FEngCopyColour(&colour, object);
    return colour.Pack();
}

// Original 0x504640: signed comparisons of wrapping +/-1 bounds, R/G/B/A order.
bool FEngColourNear(const FEngColour *value, const FEngColour *other) {
    const int *a = reinterpret_cast<const int *>(value), *b = reinterpret_cast<const int *>(other);
    const int order[4] = {2, 1, 0, 3};
    for (int i = 0; i < 4; ++i) {
        int c = order[i];
        if (b[c] > (int)((unsigned int)a[c] + 1u)) return false;
        if ((int)((unsigned int)a[c] - 1u) > b[c]) return false;
    }
    return true;
}

// Original 0x517ad0. Keep channel load/store order and modulo-32-bit additions.
void HudVisibilityObject::SetColourState(int key, const unsigned int *value, unsigned char add) {
    for (FEScript *script = scripts4C; script; script = script->next4) {
        FEScriptTrack *tracks = static_cast<FEScriptTrack *>(script->tracks20);
        for (unsigned int i = 0; i < (unsigned int)script->trackCount1C; ++i) {
            FEScriptTrack &track = tracks[i];
            if ((int)track.state7 == fengStateKeys7992F4[key]) {
                unsigned int *base = reinterpret_cast<unsigned int *>(track.base18);
                if (add) {
                    unsigned int green = base[1], blue = base[0];
                    base[2] = base[2] + value[2];
                    base[1] = green + value[1];
                    base[0] = blue + value[0];
                    base[3] = base[3] + value[3];
                } else {
                    base[3] = value[3]; base[2] = value[2]; base[1] = value[1]; base[0] = value[0];
                }
                track.flags3 &= 0x7f;
                break;
            }
        }
    }
    unsigned int *state = reinterpret_cast<unsigned int *>(reinterpret_cast<char *>(data2C) + FEngStateOffset(key));
    if (add) {
        unsigned int green = state[1];
        state[2] = state[2] + value[2];
        state[1] = green + value[1];
        state[0] = state[0] + value[0];
        state[3] = state[3] + value[3];
    } else {
        state[3] = value[3]; state[2] = value[2]; state[1] = value[1]; state[0] = value[0];
    }
}

// Original 0x51d7f0: dirty only outside +/-1 tolerance, but always perform the state write.
void FEngSetColour(HudVisibilityObject *object, unsigned int colour) {
    if (!object) return;
    FEngColour value;
    value.alphaC = colour >> 24;
    value.green4 = (colour >> 8) & 255;
    value.blue0 = colour & 255;
    value.red8 = (colour >> 16) & 255;
    if (object->type18 < 0x100) {
        if (!FEngColourNear(&value, reinterpret_cast<FEngColour *>(object->data2C))) object->flags1C |= 0x400000;
        object->SetColourState(0, reinterpret_cast<unsigned int *>(&value), 0);
    }
}
