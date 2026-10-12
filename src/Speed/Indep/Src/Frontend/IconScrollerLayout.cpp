#include "PauseMenu.h"
#include "FEngCoordinates.h"
extern "C" double __cdecl fabs(double);
#ifdef _MSC_VER
#pragma intrinsic(fabs)
#endif
extern float tweenSixth786D94;
extern float frontendDelta865198;
extern int realClock8651AC;
extern int scrollerClock8641A0;

static IconScrollerIcon *IconFromLink(void *link) {
    return link ? reinterpret_cast<IconScrollerIcon *>(static_cast<char *>(link) - 4) : 0;
}

// Original 0x5748c0. Do not simplify inv * duration to one.
void ScrollerTween::LimitEndSlope(float limit) {
    double inv = 1.0f / duration24;
    double t = inv * duration24;
    double slope = ((coefficient14 + coefficient14 + t * coefficient10 * 3.0f) * t + coefficient18) * inv;
    double magnitude = fabs(slope);
    if (limit < magnitude) targetSlopeC = (magnitude / slope) * limit * duration24;
}

// Original 0x574910. The start acceleration is stored as float before clamping.
void ScrollerTween::LimitAcceleration(float limit) {
    double inv = 1.0f / duration24;
    volatile float twice = coefficient14 + coefficient14; // store at original 0x574925
    volatile float start = (0.0f * inv * coefficient10 * 6.0f + twice) * inv * inv;
    double magnitude = fabs(start);
    double end = (inv * duration24 * coefficient10 * 6.0f + twice) * inv * inv;
    if (limit < magnitude) start = (magnitude / start) * limit;
    if (!(fabs(end) > limit)) {
        if (!(magnitude > limit)) return;
    } else {
        end = (fabs(end) / end) * limit;
    }
    double squared = duration24 * duration24;
    double scaledStart = start * squared;
    coefficient14 = 0.5f * scaledStart;
    coefficient10 = (squared * end - scaledStart) * tweenSixth786D94;
}

// Original 0x574e30: states 0/1/2, coefficient generation and cubic evaluation.
void UpdateScrollerTween(ScrollerTween *tween, float delta, float slopeLimit, float accelerationLimit) {
    if (tween->state2C == 0) return;
    if (tween->state2C != 1) {
        if (tween->state2C != 2) return;
        tween->time20 = 0.0f;
        if (tween->mode2E == 0) tween->state2C = 1;
        if (slopeLimit > 0.0f) tween->LimitEndSlope(slopeLimit);
        double difference = tween->target8 - tween->value0;
        tween->coefficient1C = tween->value0;
        tween->coefficient18 = tween->slope4;
        tween->coefficient10 = (tween->targetSlopeC + tween->slope4) - (difference + difference);
        tween->coefficient14 = (difference * 3.0f - tween->targetSlopeC) - (tween->slope4 + tween->slope4);
        if (accelerationLimit > 0.0f) tween->LimitAcceleration(accelerationLimit);
    }
    double time = delta / tween->duration24 + tween->time20;
    tween->time20 = time;
    if (!(time >= tween->limit28)) { // x87 C0 also takes this branch for unordered values
        tween->value0 = ((time * tween->coefficient10 + tween->coefficient14) * time + tween->coefficient18) * time + tween->coefficient1C;
        tween->slope4 = (tween->coefficient14 + tween->coefficient14 + time * tween->coefficient10 * 3.0f) * time + tween->coefficient18;
    } else {
        tween->state2C = 0;
        if (1.0f <= tween->limit28) {
            tween->value0 = tween->target8;
            tween->slope4 = tween->targetSlopeC;
        }
    }
}

// Original 0x50d8d0.
float ScrollerFadeFactor(float position, float centre, float window, float edge) {
    float upper = centre + window * 0.5f;
    float lower = centre - window * 0.5f;
    if (position < lower || upper < position) return 0.0f;
    if (lower <= position && position < lower + edge) return (position - lower) / edge;
    if ((position < upper) != (position == upper)) {
        if (upper - edge < position) return (upper - position) / edge;
    }
    return 1.0f;
}

// Original 0x50d9c0: both endpoints are included.
void IconScroller4C::SetWindowVisibility(IconScrollerIcon *icon, float position) {
    float half = windowCC * 0.5f;
    if (masterD4 - half <= position && position <= half + masterD4)
        HudShowObject(static_cast<HudVisibilityObject *>(icon->object10));
    else HudHideObject(static_cast<HudVisibilityObject *>(icon->object10));
}

// Original 0x51fd60: current RGB, icon alpha (150 for a disallowed disabled icon), no clamp.
void IconScroller4C::SetIconAlpha(IconScrollerIcon *icon, float factor) {
    if (icon && icon->object10) {
        HudVisibilityObject *object = static_cast<HudVisibilityObject *>(icon->object10);
        if (object->data2C) {
            unsigned int colour = FEngGetColour(object);
            double alpha = (unsigned char)(icon->colour1C >> 24);
            if (icon->value20 && !allowDisabled30) alpha = 150.0f;
            int value = (int)(alpha * factor);
            FEngSetColour(object, ((unsigned int)value << 24) | (colour & 0xffffff));
        }
    }
}

// Original 0x52f8c0: centre position, then scale/alignment through top-left coordinates.
void IconScroller4C::LayoutIcon(IconScrollerIcon *icon) {
    if (!icon) return;
    float position = tween90.value0 + icon->offset14 + masterD4;
    SetWindowVisibility(icon, position);
    HudVisibilityObject *object = static_cast<HudVisibilityObject *>(icon->object10);
    FEngSetSize(object, icon->width24, icon->height28);
    FEngSetCentre(object, position, masterD8);
    float y = 0.0f;
    float temporary, discarded;
    if (alignmentC0 == 0) FEngGetTopLeft(object, &temporary, &y);
    else if (alignmentC0 == 1) {
        FEngGetTopLeft(object, &temporary, &y);
        y = (icon->height28 - icon->height28 * 0.8f) * 0.5f + y;
    } else if (alignmentC0 == 2) {
        FEngGetTopLeft(object, &temporary, &y);
        y = (icon->height28 - icon->height28 * 0.8f) + y;
    }
    if (icon != currentC) {
        FEngSetSize(object, icon->width24 * 0.8f, icon->height28 * 0.8f);
        FEngGetTopLeft(object, &temporary, &discarded); // x survives, y is discarded
        FEngSetTopLeft(object, temporary, y);
    }
    if (fadeE4) SetIconAlpha(icon, ScrollerFadeFactor(position,
                                                    masterD4, windowCC, icon->width24 * 0.8f));
}

// Original 0x538a30: snap tween, lay out using prior selection, then select the requested icon.
void IconScroller4C::ResetLayout(int index) {
    IconScrollerIcon *icon = IconFromLink(next4);
    if (index > 0) icon = IconFromLink(reinterpret_cast<ScrollerIconList *>(&next4)->NodeAt(index - 1));
    float position = value2D ? icon->offset14 : icon->value18;
    tween90.target8 = -position;
    if (-position != tween90.value0) tween90.state2C = 2;
    tween90.value0 = tween90.target8;
    tween90.slope4 = tween90.targetSlopeC;
    tween90.state2C = 0;
    for (IconScrollerIcon *entry = IconFromLink(next4); entry != reinterpret_cast<IconScrollerIcon *>(this);
         entry = IconFromLink(entry->next4)) LayoutIcon(entry);
    if (index == 0) icon = IconFromLink(next4);
    UnknownSlot14(icon);
    scrollBar38.Update(1, option28 - 1, selectedC8);
}

// Original 0x538940: placement precedes the frame's tween advancement.
void IconScroller4C::UpdateLayout() {
    if (next4 == &next4 || !currentC) return;
    if (relayout2E) {
        relayout2E = 0;
        scrollerClock8641A0 = realClock8651AC;
        scrollBar38.Update(1, option28 - 1, selectedC8);
        float position = -currentC->offset14;
        tween90.target8 = position;
        if (position != tween90.value0) tween90.state2C = 2;
        UpdateArrows(direction34);
        direction34 = -9;
    }
    UpdateIconLockScripts(currentC, object14, object18);
    for (IconScrollerIcon *entry = IconFromLink(next4); entry != reinterpret_cast<IconScrollerIcon *>(this);
         entry = IconFromLink(entry->next4)) LayoutIcon(entry);
    UpdateScrollerTween(&tween90, frontendDelta865198, 0.0f, 0.0f);
}
