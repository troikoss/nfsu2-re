#include "IconScrollerLayout.h"
#include "FEngCoordinates.h"
#include "PauseMenu.h"

// Original 0x538080. The hidden branch intentionally leaves backing/end visibility unchanged.
void ScrollerScrollBar::Update(int page, int count, int selected) {
    if (count <= page || count == 0) {
        HudHideObject(handle44); HudHideObject(arrow48); HudHideObject(arrow4C);
        return;
    }
    HudShowObject(backing40);
    if (selected == 0) HudHideObject(handle44); else HudShowObject(handle44);
    HudShowObject(arrow48); HudShowObject(arrow4C);
    HudShowObject(end54); HudShowObject(end50);
    if (scaled1) UpdateScaled(page, count, selected); else UpdateFixed(page, count, selected);
}

// Original 0x52e200. Rounded page/count/size, then extended step reused for position.
void ScrollerScrollBar::UpdateScaled(int page, int count, int selected) {
    volatile float pageFloat = (float)page, countFloat = (float)count;
    float extent = vertical0 ? extent28 : extent24;
    float size = (float)(((double)pageFloat / countFloat) * extent);
    if (vertical0) FEngSetHeight(handle44, size); else FEngSetWidth(handle44, size);
    double difference = (double)countFloat - pageFloat;
    if (difference < 1.0) difference = 1.0;
    double step = ((double)extent - size) / difference;
    step34 = (float)step;
    float position = (float)(step * ((double)selected - 1.0) + (vertical0 ? origin20 : origin1C));
    if (vertical0) position10 = position; else positionC = position;
    if (!deferred2) {
        float x, y;
        FEngGetTopLeft(handle44, &x, &y);
        if (vertical0) FEngSetTopLeft(handle44, x, position);
        else FEngSetTopLeft(handle44, position, y);
    }
}

// Original 0x52e320. Here page stays an integer in the denominator and step is rounded before use.
void ScrollerScrollBar::UpdateFixed(int page, int count, int selected) {
    float origin = vertical0 ? origin20 : origin1C;
    float handle = vertical0 ? handle30 : handle2C;
    float extent = vertical0 ? extent28 : extent24;
    volatile float selectedFloat = (float)((double)selected - 1.0), countFloat = (float)count;
    double difference = (double)countFloat - page;
    if (difference < 1.0) difference = 1.0;
    float step = (float)(((double)extent - handle) / difference);
    step34 = step;
    float position;
    if (selectedFloat == 0.0f) position = (float)((double)handle * 0.5 + origin);
    else if ((double)countFloat - 1.0 == selectedFloat)
        position = (float)(((double)extent + origin) - (double)handle * 0.5);
    else position = (float)((double)handle * 0.5 + (double)step * selectedFloat + origin);
    if (!deferred2) {
        float x, y;
        FEngGetCentre(handle44, &x, &y);
        if (vertical0) FEngSetCentre(handle44, x, position);
        else FEngSetCentre(handle44, position, y);
    }
    if (vertical0) position10 = position; else positionC = position;
}

// Original 0x4953e0: hash before typed image lookup, including null package names.
HudVisibilityObject *FngFindNamedImage(const char *package, const char *name) {
    return FngFindImageByHash(package, FngHash(name));
}

// Original 0x542ac0. Step +0x34 and bytes +0x3c..+0x3f are intentionally untouched.
ScrollerScrollBar::ScrollerScrollBar(const char *package, const char *prefix,
                                     unsigned char vertical, unsigned char scaled, unsigned char value) {
    vertical0 = vertical; scaled1 = scaled; deferred2 = 0; value3 = value;
    reinterpret_cast<unsigned int *>(unidentified4)[0] = 0;
    reinterpret_cast<unsigned int *>(unidentified4)[1] = 0;
    positionC = 0.0f; position10 = 0.0f;
    reinterpret_cast<unsigned int *>(unidentified14)[0] = 0;
    reinterpret_cast<unsigned int *>(unidentified14)[1] = 0;
    origin1C = 0.0f; origin20 = 0.0f; extent24 = 0.0f; extent28 = 0.0f;
    handle2C = 0.0f; handle30 = 0.0f;
    reinterpret_cast<unsigned int *>(unidentified38)[0] = 0;
    backing40 = 0; handle44 = 0; arrow48 = 0; arrow4C = 0; end50 = 0; end54 = 0;
    if (prefix) {
        char name[64];
        FngFormatBuffer(name, 64, "%s%s", prefix, "_Backing");
        backing40 = FngFindNamedImage(package, name);
        FngFormatBuffer(name, 64, "%s%s", prefix, "_Handle");
        handle44 = FngFindNamedImage(package, name);
        FngFormatBuffer(name, 64, "%s%s", prefix, "_Arrow_1");
        arrow48 = FngFindNamedImage(package, name);
        FngFormatBuffer(name, 64, "%s%s", prefix, "_Arrow_2");
        arrow4C = FngFindNamedImage(package, name);
        FngFormatBuffer(name, 64, "%s%s", prefix, "_Backing_End_1");
        end50 = FngFindNamedImage(package, name);
        FngFormatBuffer(name, 64, "%s%s", prefix, "_Backing_End_2");
        end54 = FngFindNamedImage(package, name);
        FEngGetTopLeft(backing40, &origin1C, &origin20);
        FEngGetSize(backing40, &extent24, &extent28);
        if (scaled1) {
            // Retail constructor uses the opposite extent for this initial scaled handle size.
            float extent = vertical0 ? extent24 : extent28;
            handle30 = extent;
            reinterpret_cast<unsigned int *>(unidentified38)[0] = 0;
            handle2C = extent;
        } else {
            FEngGetSize(handle44, &handle2C, &handle30);
            reinterpret_cast<unsigned int *>(unidentified38)[0] = 0;
        }
    }
}
