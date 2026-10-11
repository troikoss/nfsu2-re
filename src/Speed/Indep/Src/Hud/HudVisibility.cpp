#include "HudVisibility.h"

extern int fengVisibilityDirty8384D4;

// Original 0x50ca00.
void HudHideObject(HudVisibilityObject *object) {
    if (!object) return;
    if (fengVisibilityDirty8384D4 || !(object->flags1C & 1)) {
        object->flags1C |= 0x400001;
    }
    if (object->type18 == 5) {
        HudVisibilityObject *child = object->firstChild64;
        int count = object->childCount60;
        if (count > 0) {
            do {
                HudHideObject(child);
                child = child->next4;
                --count;
            } while (count != 0);
        }
    }
}

// Original 0x50ca50.
void HudShowObject(HudVisibilityObject *object) {
    if (!object) return;
    if (fengVisibilityDirty8384D4 || (object->flags1C & 1)) {
        object->flags1C = (object->flags1C & ~1u) | 0x400000;
    }
    if (object->type18 == 5) {
        HudVisibilityObject *child = object->firstChild64;
        int count = object->childCount60;
        if (count > 0) {
            do {
                HudShowObject(child);
                child = child->next4;
                --count;
            } while (count != 0);
        }
    }
}

// Original 0x4c6690. Masks are saved even when the object list is empty.
void HudFeatureVisibility::ApplyMasks(unsigned int low, unsigned int high) {
    unsigned int requiredLow = mask10;
    current18 = low;
    unsigned int requiredHigh = mask14;
    current1C = high;
    HudFeatureObjectNode *node = next0;
    HudFeatureObjectNode *end = reinterpret_cast<HudFeatureObjectNode *>(this);
    for (; node != end; node = node->next0) {
        if (!(requiredLow & low) && !(requiredHigh & high)) {
            HudHideObject(node->object8);
        } else {
            HudShowObject(node->object8);
        }
    }
}
