#include "PauseMenu.h"
#include "FEngPackage.h"

static IconScrollerIcon *IconFromLink(void *link) {
    return link ? reinterpret_cast<IconScrollerIcon *>(static_cast<char *>(link) - 4) : 0;
}

// Original 0x43be60: stop at the sentinel or the requested zero-based list index.
void *ScrollerIconList::NodeAt(int index) {
    void *node = first0;
    for (int i = 0; node != this && i != index; ++i) node = *static_cast<void **>(node);
    return node;
}

// Original 0x52f680: mark every icon before testing whether selection is permitted.
bool IconScroller4C::SelectIcon(IconScrollerIcon *icon) {
    IconScrollerIcon *entry = IconFromLink(next4);
    while (entry != reinterpret_cast<IconScrollerIcon *>(this)) {
        unsigned int hash = static_cast<HudVisibilityObject *>(entry->object10)->hash10;
        if (fengManager8384C4) FngSetFocusEnabled(package1C, hash, 0);
        entry = IconFromLink(entry->next4);
    }
    if (icon->value20 && !allowDisabled30) return false;
    currentC = icon;
    unsigned int hash = static_cast<HudVisibilityObject *>(icon->object10)->hash10;
    if (fengManager8384C4) FngSetFocusEnabled(package1C, hash, 1);
    HudVisibilityObject *object = static_cast<HudVisibilityObject *>(currentC->object10);
    if (fengManager8384C4 && object) fengManager8384C4->SetFocusByHash(package1C, object->hash10);
    selectedC8 = IndexOf(icon);
    return true;
}

// Original 0x52f7c0: input is zero-based; stored index remains the unmodified input.
bool IconScroller4C::SelectIndex(int index) {
    IconScrollerIcon *entry = IconFromLink(next4);
    while (entry != reinterpret_cast<IconScrollerIcon *>(this)) {
        unsigned int hash = static_cast<HudVisibilityObject *>(entry->object10)->hash10;
        if (fengManager8384C4) FngSetFocusEnabled(package1C, hash, 0);
        entry = IconFromLink(entry->next4);
    }
    void *node = reinterpret_cast<ScrollerIconList *>(&next4)->NodeAt(index);
    if (node) {
        IconScrollerIcon *icon = IconFromLink(node);
        if (icon && icon != currentC && (!icon->value20 || allowDisabled30)) {
            currentC = icon;
            if (fengManager8384C4) {
                FngSetFocusEnabled(package1C, static_cast<HudVisibilityObject *>(icon->object10)->hash10, 1);
            }
            HudVisibilityObject *object = static_cast<HudVisibilityObject *>(currentC->object10);
            if (fengManager8384C4 && object) fengManager8384C4->SetFocusByHash(package1C, object->hash10);
            selectedC8 = index;
            return true;
        }
    }
    return false;
}
