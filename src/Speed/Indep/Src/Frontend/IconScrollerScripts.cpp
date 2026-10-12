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
