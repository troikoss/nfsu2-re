#include "FEngCoordinates.h"
extern float fengEpsilon7992EC;
extern int fengStateKeys7992F4[];

// Original 0x6f45b0: variable-stride sorted glyph lookup.
void *FindFEngGlyph(unsigned short character, const void *glyphs, unsigned int count, int stride) {
    const char *base = static_cast<const char *>(glyphs);
    while (count) {
        const char *entry = base + (count >> 1) * stride;
        unsigned int found = *reinterpret_cast<const unsigned short *>(entry);
        if (character == found) return const_cast<char *>(entry);
        if (character > found) { base = entry + stride; --count; }
        count >>= 1;
    }
    return 0;
}

// Original 0x50bec0: retain the maximum line width found while walking the text.
float FEngFont::TextWidth(const unsigned short *text, unsigned int flags) {
    float width = LineWidth(text, 0, 0, 0);
    unsigned short character = *text;
    if (character) {
        const unsigned short *p = text;
        do {
            ++p;
            if ((flags & 0x20) || (character != 10 && character != '^')) {
                float candidate = LineWidth(p, 0, 0, 0);
                if (width < candidate) width = candidate;
            }
            character = *p;
        } while (character);
    }
    return width;
}

// Original 0x50bf40, including the low-byte glyph lookup and trailing-break policy.
float FEngFont::TextHeight(const unsigned short *text, int leading) {
    if (!text) return 0.0f;
    float height = 0.0f;
    bool glyphs = true;
    unsigned short character = *text;
    if (character) {
        do {
            ++text;
            if (character == 10 || character == '^') {
                glyphs = false;
                height = (float)leading + height + lineHeight24;
            } else if (character != 13) {
                const unsigned char *data = fntfC;
                unsigned int code = character & 0xff;
                unsigned int count = *reinterpret_cast<const unsigned short *>(data + 10);
                const unsigned char *table = data + *reinterpret_cast<const unsigned int *>(data + 0x14);
                int stride = (*reinterpret_cast<const unsigned int *>(data + 0xc) & 0x40000) ? 16 : 12;
                const void *entry = 0;
                if ((int)(code - 0x20) < (int)count) {
                    entry = table + (code - 0x20) * stride;
                    if (*static_cast<const unsigned short *>(entry) != code) entry = 0;
                }
                if (!entry) entry = FindFEngGlyph((unsigned short)code, table, count, stride);
                if (entry) glyphs = true;
            }
            character = *text;
        } while (character);
        if (!glyphs) return height;
    }
    height = height + lineHeight24;
    return height;
}

// Original 0x50c020 / 0x50c050: priority of the first flag is deliberate.
float __stdcall FEngHorizontalOffset(unsigned char flags, float width) {
    if (flags & 1) return width * -0.5f;
    if (flags & 2) return -width;
    return 0.0f;
}
float __stdcall FEngVerticalOffset(unsigned char flags, float height) {
    if (flags & 4) return height * -0.5f;
    if (flags & 8) return -height;
    return 0.0f;
}

// Original 0x504500: ordered inclusive epsilon bounds, not a rounded difference.
bool FEngVectorNear(const float *value, const float *other) {
    for (int i = 0; i < 3; ++i) {
        if (!(fengEpsilon7992EC + value[i] >= other[i])) return false;
        if (!(value[i] - fengEpsilon7992EC <= other[i])) return false;
    }
    return true;
}

// Original 0x5046f0.
int __stdcall FEngStateOffset(int key) {
    switch (key) {
    case 1: return 0x10;
    case 2: return 0x1c;
    case 3: return 0x28;
    case 4: return 0x38;
    case 5: return 0x44;
    case 6: return 0x4c;
    case 7: return 0x54;
    case 8: return 0x64;
    case 9: return 0x74;
    case 10: return 0x84;
    default: return 0;
    }
}

// Original 0x5178d0: first matching track per script, then always update the live state.
void HudVisibilityObject::SetVectorState(int key, const float *value, unsigned char add) {
    for (FEScript *script = scripts4C; script; script = script->next4) {
        FEScriptTrack *tracks = static_cast<FEScriptTrack *>(script->tracks20);
        for (unsigned int i = 0; i < (unsigned int)script->trackCount1C; ++i) {
            if ((int)tracks[i].state7 == fengStateKeys7992F4[key]) {
                if (add) {
                    tracks[i].base18[0] = value[0] + tracks[i].base18[0];
                    tracks[i].base18[1] = value[1] + tracks[i].base18[1];
                    tracks[i].base18[2] = value[2] + tracks[i].base18[2];
                } else {
                    tracks[i].base18[0] = value[0];
                    tracks[i].base18[1] = value[1];
                    tracks[i].base18[2] = value[2];
                }
                tracks[i].flags3 &= 0x7f;
                break;
            }
        }
    }
    float *state = reinterpret_cast<float *>(reinterpret_cast<char *>(data2C) + FEngStateOffset(key));
    if (add) {
        state[0] = state[0] + value[0];
        state[1] = value[1] + state[1];
        state[2] = value[2] + state[2];
    } else {
        state[0] = value[0]; state[1] = value[1]; state[2] = value[2];
    }
}

// Original 0x51d070.
void FEngGetTopLeft(HudVisibilityObject *object, float *x, float *y) {
    if (!object) return;
    float *state = object->data2C;
    switch (object->type18) {
    case 1: case 7: case 9:
        *x = state[7] - state[14] * 0.5f;
        *y = state[8] - state[15] * 0.5f;
        return;
    case 2: {
        FEngFont *font = FindFEngFont(object->texture24);
        if (font) {
            FEStringObject *string = static_cast<FEStringObject *>(object);
            float width = font->TextWidth(object->text64, 0) * state[14];
            float height = font->TextHeight(object->text64, string->leading70) * state[15];
            *x = FEngHorizontalOffset((unsigned char)string->justification6C, width) + state[7];
            *y = FEngVerticalOffset((unsigned char)string->justification6C, height) + state[8];
        }
        return;
    }
    case 5: return;
    default: *x = state[7]; *y = state[8];
    }
}

// Original 0x51d380.
void FEngSetTopLeft(HudVisibilityObject *object, float x, float y) {
    if (!object) return;
    float *state = object->data2C;
    float position[3] = {state[7], state[8], state[9]};
    float width = state[14], height = state[15];
    switch (object->type18) {
    case 1: case 7: case 9:
        position[0] = width * 0.5f + x;
        position[1] = height * 0.5f + y;
        break;
    case 2: {
        FEngFont *font = FindFEngFont(object->texture24);
        if (font) {
            FEStringObject *string = static_cast<FEStringObject *>(object);
            float textWidth = font->TextWidth(object->text64, 0) * width;
            float textHeight = font->TextHeight(object->text64, string->leading70) * height;
            position[0] = x - FEngHorizontalOffset((unsigned char)string->justification6C, textWidth);
            position[1] = y - FEngVerticalOffset((unsigned char)string->justification6C, textHeight);
        }
        break;
    }
    default: position[0] = x; position[1] = y;
    }
    if (object->type18 < 0x100) {
        if (!FEngVectorNear(position, state + 7)) object->flags1C |= 0x400000;
        object->SetVectorState(2, position, 0);
    }
}

// Original 0x51d4d0.
void FEngSetCentre(HudVisibilityObject *object, float x, float y) {
    if (!object) return;
    float *state = object->data2C;
    float position[3] = {state[7], state[8], state[9]};
    switch (object->type18) {
    case 2: {
        FEngFont *font = FindFEngFont(object->texture24);
        if (font) {
            FEStringObject *string = static_cast<FEStringObject *>(object);
            float width = font->TextWidth(object->text64, 0) * state[14];
            float height = font->TextHeight(object->text64, string->leading70) * state[15];
            position[0] = x - (width * 0.5f + FEngHorizontalOffset((unsigned char)string->justification6C, width));
            position[1] = y - (height * 0.5f + FEngVerticalOffset((unsigned char)string->justification6C, height));
        }
        break;
    }
    default: position[0] = x; position[1] = y;
    }
    if (object->type18 < 0x100) {
        if (!FEngVectorNear(position, state + 7)) object->flags1C |= 0x400000;
        object->SetVectorState(2, position, 0);
    }
}

// Original 0x50cb00: state size and dirty flag even for non-image object types.
void FEngSetSize(HudVisibilityObject *object, float width, float height) {
    if (!object) return;
    object->data2C[14] = width;
    object->data2C[15] = height;
    object->flags1C |= 0x400000;
}

// Original 0x51d620.
void FEngGetCentre(HudVisibilityObject *object, float *x, float *y) {
    if (!object) return;
    float *state = object->data2C;
    if (object->type18 == 2) {
        FEngFont *font = FindFEngFont(object->texture24);
        if (font) {
            FEStringObject *string = static_cast<FEStringObject *>(object);
            float width = font->TextWidth(object->text64, 0) * state[14];
            float height = font->TextHeight(object->text64, string->leading70) * state[15];
            *x = width * 0.5f + FEngHorizontalOffset((unsigned char)string->justification6C, width) + state[7];
            *y = height * 0.5f + FEngVerticalOffset((unsigned char)string->justification6C, height) + state[8];
        }
    } else { *x = state[7]; *y = state[8]; }
}

// Original 0x51d730: unsupported types return zero size; null/missing font leave outputs untouched.
void FEngGetSize(HudVisibilityObject *object, float *width, float *height) {
    if (!object) return;
    float *state = object->data2C;
    switch (object->type18) {
    case 1: case 7: case 9: *width = state[14]; *height = state[15]; return;
    case 2: {
        FEngFont *font = FindFEngFont(object->texture24);
        if (font) {
            FEStringObject *string = static_cast<FEStringObject *>(object);
            *width = font->TextWidth(object->text64, 0) * state[14];
            *height = font->TextHeight(object->text64, string->leading70) * state[15];
        }
        return;
    }
    default: *width = 0.0f; *height = 0.0f;
    }
}

// Original 0x495590.
void FEngSetWidth(HudVisibilityObject *object, float width) {
    float oldWidth, height;
    FEngGetSize(object, &oldWidth, &height);
    FEngSetSize(object, width, height);
}

// Original 0x525db0: direct stores, including duplicate image-type stores.
void FEngSetHeight(HudVisibilityObject *object, float height) {
    float width, oldHeight;
    FEngGetSize(object, &width, &oldHeight);
    if (object) {
        float *state = object->data2C;
        if (object->type18 == 1 || object->type18 == 7 || object->type18 == 9) {
            state[14] = width; state[15] = height;
        }
        state[15] = height; state[14] = width;
        object->flags1C |= 0x400000;
    }
}
