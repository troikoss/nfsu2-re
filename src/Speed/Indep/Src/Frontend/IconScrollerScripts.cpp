#include "PauseMenu.h"
#include "PauseOptions.h"
#include "../Hud/HudVisibility.h"

// Original 0x51d040: formatting/hash occurs even for a null object.
bool FngObjectScriptActive(void *object, const char *name) {
    unsigned int hash = FngFormatHash(name);
    HudVisibilityObject *o = static_cast<HudVisibilityObject *>(object);
    return o && o->currentScript54 && o->currentScript54->hash30 == hash &&
           o->currentScript54->time10 <= o->currentScript54->lengthC;
}

// Original 0x51df00: show lock at +0x22, otherwise hide; +0x23 enables the button's FadeIn.
void UpdateIconLockScripts(IconScrollerIcon *icon, void *lock, void *button) {
    if (icon->unidentified21[1]) {
        FngSetObjectScript(lock, "SHOW", true);
        FngSetObjectScript(button, "HIDE", true);
        return;
    }
    if (!icon->unidentified21[2]) {
        FngSetObjectScript(lock, "HIDE", true);
        FngSetObjectScript(button, "HIDE", true);
    } else {
        FngSetObjectScript(lock, "HIDE", true);
        unsigned int hash = FngFormatHash("FadeIn");
        HudVisibilityObject *object = static_cast<HudVisibilityObject *>(button);
        if (!object || !object->currentScript54 || object->currentScript54->hash30 != hash ||
            object->currentScript54->lengthC < object->currentScript54->time10) {
            if (!FngObjectScriptActive(button, "Animate")) FngSetObjectScript(button, "FadeIn", true);
        }
    }
}

// Original 0x527660. This helper hashes directly rather than calling 0x51d040.
void ScrollerScrollBar::SetArrow2Static() {
    HudVisibilityObject *object = arrow4C;
    if (object) {
        unsigned int hash = FngFormatHash("Static");
        if (!object->currentScript54 || object->currentScript54->hash30 != hash ||
            object->currentScript54->lengthC < object->currentScript54->time10)
            FngSetObjectScript(arrow4C, "Static", true);
    }
}

// Original 0x495810.
void ScrollerScrollBar::SetArrow1Blink() {
    if (arrow48 && !FngObjectScriptActive(arrow48, "Blink"))
        FngSetObjectScript(arrow48, "Blink", true);
}

// Original 0x52fa70: script requests precede endpoint visibility, including early returns.
void IconScroller4C::UpdateArrows(int direction) {
    if (direction == 1 || direction == -10) {
        HudVisibilityObject *object = scrollBar38.arrow48;
        if (object) {
            unsigned int hash = FngFormatHash("Static");
            if (!object->currentScript54 || object->currentScript54->hash30 != hash ||
                object->currentScript54->lengthC < object->currentScript54->time10)
                FngSetObjectScript(scrollBar38.arrow48, "Static", true);
        }
        object = scrollBar38.arrow4C;
        if (object) {
            unsigned int hash = FngFormatHash("Blink");
            if (!object->currentScript54 || object->currentScript54->hash30 != hash ||
                object->currentScript54->lengthC < object->currentScript54->time10)
                FngSetObjectScript(scrollBar38.arrow4C, "Blink", true);
        }
    } else if (direction == -1 || direction == 10) {
        scrollBar38.SetArrow2Static();
        scrollBar38.SetArrow1Blink();
    }
    IconScrollerIcon *first = next4 ? reinterpret_cast<IconScrollerIcon *>(static_cast<char *>(next4) - 4) : 0;
    if (currentC == first) {
        HudHideObject(scrollBar38.arrow48);
        return;
    }
    IconScrollerIcon *last = previous8 ? reinterpret_cast<IconScrollerIcon *>(static_cast<char *>(previous8) - 4) : 0;
    if (currentC == last) {
        HudHideObject(scrollBar38.arrow4C);
        return;
    }
    HudShowObject(scrollBar38.arrow48);
    HudShowObject(scrollBar38.arrow4C);
}
