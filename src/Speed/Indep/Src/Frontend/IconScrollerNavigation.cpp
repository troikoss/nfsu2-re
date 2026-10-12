#include "PauseMenu.h"

// List links address the node at icon +4, including the scroller's sentinel at +4.
static IconScrollerIcon *IconFromLink(void *link) {
    return link ? reinterpret_cast<IconScrollerIcon *>(static_cast<char *>(link) - 4) : 0;
}

// Original 0x5276d0 / 0x527700. Original callers consume AL only.
bool IconScroller4C::AtFirst() { return currentC == IconFromLink(next4); }
bool IconScroller4C::AtLast() { return currentC == IconFromLink(previous8); }

// Original 0x527730 / 0x527750.
void IconScroller4C::Next() {
    if (vertical2C) StepWrap(1);
    else StepNoWrap(1);
}
void IconScroller4C::Previous() {
    if (vertical2C) StepWrap(-1);
    else StepNoWrap(-1);
}

// Original 0x51f9f0: stop at the list boundary; skip disabled items unless allowed.
void IconScroller4C::StepNoWrap(int direction) {
    IconScrollerIcon *old = currentC;
    IconScrollerIcon *icon = old;
    if (direction == -1) {
        void *first = next4;
        if (old != IconFromLink(first)) {
            do {
                icon = IconFromLink(icon->previous8);
                if (!icon->value20) goto select;
                if (allowDisabled30) break;
            } while (icon != IconFromLink(first));
        }
    } else if (direction == 1) {
        void *last = previous8;
        if (old != IconFromLink(last)) {
            do {
                icon = IconFromLink(icon->next4);
                if (!icon->value20) goto select;
                if (allowDisabled30) break;
            } while (icon != IconFromLink(last));
        }
    }
    if (!icon->value20 || allowDisabled30) {
select:
        if (icon != old) {
            UnknownSlot14(icon);
            relayout2E = 1;
            direction34 = direction;
        }
    }
}

// Original 0x51fac0: wrap at either boundary. The original does not terminate when
// every item is disabled and allowDisabled30 is zero; callers must avoid that state.
void IconScroller4C::StepWrap(int direction) {
    IconScrollerIcon *old = currentC;
    IconScrollerIcon *icon = old;
    if (direction == -1) {
        do {
            icon = IconFromLink(icon == IconFromLink(next4) ? previous8 : icon->previous8);
            if (!icon->value20) goto select;
        } while (!allowDisabled30);
    } else if (direction == 1) {
        do {
            icon = IconFromLink(icon == IconFromLink(previous8) ? next4 : icon->next4);
            if (!icon->value20) goto select;
        } while (!allowDisabled30);
    }
    if (!icon->value20 || allowDisabled30) {
select:
        if (icon != old) {
            UnknownSlot14(icon);
            relayout2E = 1;
            direction34 = direction;
        }
    }
}

// Original 0x51f820: 1-based index or -1, including null input.
int IconScroller4C::IndexOf(IconScrollerIcon *icon) {
    if (!icon) return -1;
    void *link = next4;
    int index = 1;
    for (;;) {
        IconScrollerIcon *entry = IconFromLink(link);
        IconScrollerIcon *sentinel = reinterpret_cast<IconScrollerIcon *>(this);
        if (entry == sentinel) break;
        if (entry == icon) return index;
        link = entry->next4;
        ++index;
    }
    return -1;
}

// Original 0x50d980: dispatch only when the sender is the selected icon's FEObject.
void IconScroller4C::Dispatch(unsigned int message, int value8, int valueC, int value10) {
    IconScrollerIcon *icon = currentC;
    if (icon && value8 == (int)icon->object10) {
        icon->React(package1C, message, value8, valueC, value10);
    }
}
