// Native platform seams for the unrecovered options list and widgets. Original category
// setup, message handlers and setting edits remain in Frontend/PauseOptions.cpp. These
// seams translate retail FEng calls to NativeFng; they are not verified recovered code.
#include "NativeFrontend.h"
#include "NativePause.h"
#include "../../Speed/Indep/Src/Frontend/PauseOptions.h"

#include <math.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <map>
#include <vector>

void NativeTrap(const char *what);
int DriveCameraTypeForOption(int option);
extern float volumes83AA28[7];
extern Audio82B884 *audio82B884;
extern Player8900AC *players8900AC[2];
extern unsigned char units83AA1A;
extern unsigned char mirror83AA18;
extern unsigned char prompt844AD8;
extern unsigned char autosave844AD9;
extern int levelOfDetail87099C;
extern int resolution870D1C;
extern int resolutions86E880[6];
extern unsigned char displayChanged8709D1;
extern PlayerSettings83AA64 playerSettings83AA64[2];
extern unsigned char hudDrift89E7D9;
extern int hudDriftOverride890104;
extern int trackNumber89E7A0;
extern int value83AA04;

namespace {
const unsigned int kLeft = 0x9120409e, kRight = 0xb5971bf1;
const unsigned int kUp = 0x72619778, kDown = 0x911c0a4b;
const unsigned int kAccept = 0x0c407210, kBack = 0x911ab364;

struct Widget {
    Widget() : backing(0), x(0), y(0), dataX(0), dataY(0), width(0), dataWidth(0) {}
    FengObject *backing;
    float x, y, dataX, dataY, width, dataWidth;
};
struct Bar {
    Bar() : base(0), knob(0), value(0), handle(0), begin(0), end(0) {}
    FengObject *base, *knob, *value, *handle, *begin, *end;
};
struct List {
    List() : selected(0), top(0), startY(0), stepY(24) {}
    std::vector<OptionItem *> items;
    int selected, top;
    float startY, stepY;
};
std::map<OptionItem *, Widget> widgets;
std::map<UIProgressBar *, Bar> bars;
std::map<OptionsScrollerFNGObject *, List> lists;
std::map<unsigned int, unsigned int> frontendTrackColours;
Audio82B884 nativeAudio;
Player8900AC nativePlayer;

FengObject *Object(void *p) { return static_cast<FengObject *>(p); }
FengObject *Named(const char *package, const char *prefix, int index) {
    char name[64];
    snprintf(name, sizeof(name), "%s%d", prefix, index);
    return Object(FngFindObject(package, FengHash(name)));
}
void Script(void *p, const char *name) { FngSetObjectScript(p, name, true); }
// 0x51d380 takes a top-left point for images; NativeFng stores their centres.
void Position(void *p, float x, float y) {
    FengObject *object = Object(p);
    if (object && object->type18 == 1) { x += object->size[0] / 2; y += object->size[1] / 2; }
    FengPackage::SetPosition(object, x, y);
}
void Visible(void *p, bool show) { if (show) FngShowObject(p); else FngHideObject(p); }

// 0x4ff530 dispatches to the slider's draw update; native ownership replaces FEng pointers.
void UpdateBar(UIProgressBar *bar) {
    std::map<UIProgressBar *, Bar>::iterator found = bars.find(bar);
    if (found == bars.end()) return;
    Bar &b = found->second;
    float fraction = bar->max24 == bar->min28 ? 0 : (bar->value30 - bar->min28) / (bar->max24 - bar->min28);
    if (b.base && b.knob) {
        float travel = b.base->size[0] - 92.0f; // padding 46 per side, 0x51e840 / 0x5275b0
        float left = b.base->position[0] - b.base->size[0] / 2;
        float top = b.base->position[1] - b.base->size[1] / 2;
        float handleTop = (b.base->size[1] * 0.5f + top) - b.knob->size[1] * 0.5f + 1.0f;
        Position(b.knob, (fraction * travel - b.knob->size[0] * 0.5f) + 46.0f + left,
            handleTop - 2.0f); // 0x7844c0 is 2.0, read from the retail executable
        if (b.handle) {
            b.handle->size[0] = fraction * travel;
            Position(b.handle, left + 46.0f, handleTop);
        }
    }
    if (b.value) {
        char text[32];
        snprintf(text, sizeof(text), "%d", (int)(fraction * 100.0f));
        FengPackage::SetText(b.value, text);
    }
}
void BarVisible(UIProgressBar *bar, bool show) {
    Bar &b = bars[bar];
    Visible(b.base, show); Visible(b.knob, show); Visible(b.value, show);
    Visible(b.handle, show); Visible(b.begin, show); Visible(b.end, show);
}
void HighlightWidget(OptionWidget *item, const char *package, const char *script) {
    if (package) NativeFrontendSetFocus(package, Object(item->name2C));
    Script(item->name2C, script); Script(item->data30, script);
    Script(widgets[item].backing, script);
}
void HighlightArrows(void *left, void *right, const char *script) {
    Script(left, script); Script(right, script);
}
void MoveWidget(OptionItem *item, float y) {
    Widget &w = widgets[item];
    float dy = y - w.y;
    w.y = y; w.dataY += dy;
}
void Layout(OptionsScrollerFNGObject *owner) {
    List &list = lists[owner];
    int count = (int)list.items.size();
    int visible = owner->value134 > 0 ? owner->value134 : 7;
    if (list.selected <= list.top) list.top = list.selected - 1;
    if (list.selected > list.top + visible) list.top = list.selected - visible;
    if (list.top < 0) list.top = 0;
    if (list.top > count - visible) list.top = count > visible ? count - visible : 0;
    for (int i = 0; i < count; ++i) {
        OptionItem *item = list.items[i];
        if (i < list.top || i >= list.top + visible) item->Hide();
        else {
            MoveWidget(item, list.startY + (i - list.top) * list.stepY);
            item->Show(); item->UnknownSlot10();
            // Native presentation: match the supplied frontend reference, with arrows on the selected slider.
            OptionSlider *slider = dynamic_cast<OptionSlider *>(item);
            if (slider) { Visible(slider->leftArrow4C, list.selected == i + 1); Visible(slider->rightArrow50, list.selected == i + 1); }
        }
    }
    // The full scrollbar (0x538080) is not modelled. Retail page arrows are enough here.
    FengPackage *p = NativeFrontendPackage(owner->package4);
    if (p) {
        Visible(p->Find(0x444969fd), list.top > 0);
        Visible(p->Find(0x444969fe), list.top + visible < count);
    }
}
void ClearList(OptionsScrollerFNGObject *owner) {
    List &list = lists[owner];
    for (size_t i = 0; i < list.items.size(); ++i) { list.items[i]->Hide(); delete list.items[i]; }
    list.items.clear(); list.selected = 0; list.top = 0;
    owner->value0FC[1] = list.startY;
    owner->value11C[1] = list.startY;
}

// 0x54ee40 / 0x54ef30 / 0x54f410: numbered retail objects, original call order.
int Add(OptionsScrollerFNGObject *owner, OptionItem *item, int arrows, int kind) {
    List &list = lists[owner];
    int index = (int)list.items.size() + 1;
    Widget &w = widgets[item];
    item->name2C = Named(owner->package4, "OPTION_NAME_", index);
    w.backing = Named(owner->package4, "OPTION_BACKING_", index);
    OptionWidget *widget = kind ? static_cast<OptionWidget *>(item) : 0;
    OptionSlider *slider = kind == 2 ? static_cast<OptionSlider *>(item) : 0;
    if (widget) {
        widget->data30 = kind == 1 ? Named(owner->package4, "OPTION_DATA_", index) : 0;
        void **left = kind == 2 ? &slider->leftArrow4C : &static_cast<OptionToggle *>(item)->leftArrow4C;
        void **right = kind == 2 ? &slider->rightArrow50 : &static_cast<OptionToggle *>(item)->rightArrow50;
        *left = Named(owner->package4, "LEFT_ARROW_", index);
        *right = Named(owner->package4, "RIGHT_ARROW_", index);
        FengPackage::SetTexture(Object(*left), arrows == 1 ? 0x9943d584 : 0x367b3e04);
        FengPackage::SetTexture(Object(*right), arrows == 1 ? 0x9943d584 : 0x367b3e04);
    }
    if (slider) {
        Bar &b = bars[&slider->bar54];
        b.base = Named(owner->package4, "SLIDER_BASE_", index);
        // Native theme seam: the reference uses UI_Options, whose retail Init track is dark.
        if (b.base && frontendTrackColours.count(b.base->hash))
            FengPackage::SetColour(b.base, frontendTrackColours[b.base->hash]);
        b.knob = Named(owner->package4, "SLIDER_KNOB_", index);
        b.value = Named(owner->package4, "SLIDER_VALUE_", index);
        b.handle = Named(owner->package4, "SLIDER_HANDLE_", index);
        b.begin = Named(owner->package4, "SLIDER_BEG_", index);
        b.end = Named(owner->package4, "SLIDER_END_", index);
        slider->InitSlider();
    }
    list.items.push_back(item);
    owner->value0FC[1] += list.stepY; owner->value11C[1] += list.stepY;
    if (!item->enabled28) item->Disable();
    item->Show(); item->UnknownSlot10(); item->SetLabels();
    if (getenv("UG2_DEBUG_PAUSE")) fprintf(stderr, "options: row %d %s enabled=%d\n", index,
        item->name2C ? Object(item->name2C)->text.c_str() : "<missing>", item->enabled28);
    return index;
}
void InitItem(OptionItem *item, float *position, float *size, unsigned char enabled) {
    memset(item->unidentified4, 0, sizeof(item->unidentified4));
    memset(item->unidentified29, 0, sizeof(item->unidentified29));
    item->enabled28 = enabled; item->name2C = 0;
    Widget &w = widgets[item]; w.x = position[0]; w.y = position[1]; w.width = size[0];
}
}

// 0x552780 / 0x51ff80: original master objects supply list positions and row height.
OptionsScrollerFNGObject::OptionsScrollerFNGObject(void *data) {
    memset(unidentified8, 0, sizeof(unidentified8));
    memset(unidentified24, 0, sizeof(unidentified24));
    memset(unidentified104, 0, sizeof(unidentified104));
    memset(unidentified124, 0, sizeof(unidentified124));
    memset(unidentified13C, 0, sizeof(unidentified13C));
    package4 = reinterpret_cast<const char *const *>(data)[0];
    value20 = static_cast<int *>(data)[3]; value134 = 7; value138 = 0;
    FengPackage *p = NativeFrontendPackage(package4);
    FengObject *title = p ? p->Find("TITLE_MASTER") : 0;
    FengObject *entry = p ? p->Find("DATA_MASTER") : 0;
    value0FC[0] = title ? title->position[0] - title->size[0] / 2 : 0;
    value0FC[1] = title ? title->position[1] - title->size[1] / 2 : 0;
    value10C[0] = title ? title->size[0] : 175; value10C[1] = title ? title->size[1] : 24;
    value114[0] = entry ? entry->size[0] : 175; value114[1] = entry ? entry->size[1] : 24;
    value11C[0] = entry ? entry->position[0] - entry->size[0] / 2 : 0;
    value11C[1] = entry ? entry->position[1] - entry->size[1] / 2 : value0FC[1];
    if (getenv("UG2_DEBUG_PAUSE")) fprintf(stderr, "options masters title=%p (%.1f %.1f %.1f %.1f) data=%p (%.1f %.1f %.1f %.1f)\n", (void*)title, value0FC[0],value0FC[1],value10C[0],value10C[1],(void*)entry,value11C[0],value11C[1],value114[0],value114[1]);
    List &list = lists[this]; list.startY = value0FC[1]; list.stepY = value10C[1];
    if (p) {
        // Unused numbered objects start hidden. Add/Show exposes only the recovered list.
        const char *prefixes[] = {"OPTION_NAME_", "OPTION_DATA_", "OPTION_BACKING_", "LEFT_ARROW_", "RIGHT_ARROW_",
            "SLIDER_BASE_", "SLIDER_KNOB_", "SLIDER_VALUE_", "SLIDER_HANDLE_", "SLIDER_BEG_", "SLIDER_END_"};
        for (int i = 1; i <= 20; ++i)
            for (size_t k = 0; k < sizeof(prefixes) / sizeof(prefixes[0]); ++k) FngHideObject(Named(package4, prefixes[k], i));
        FngHideObject(title); FngHideObject(entry);
    }
}
OptionsScrollerFNGObject::~OptionsScrollerFNGObject() { ClearList(this); lists.erase(this); }
int OptionsScrollerFNGObject::UnknownSlot04(int) { NativeTrap("options slot 0x40d080"); return 0; }
int OptionsScrollerFNGObject::UnknownSlot0C(int, int, int, int, int) { NativeTrap("options slot 0x570a70"); return 0; }
int OptionsScrollerFNGObject::AddOption54EF30(OptionItem *item, int arrows) { return Add(this, item, arrows, 1); }
int OptionsScrollerFNGObject::AddOption54F170(OptionItem *item, int arrows) { return AddOption54EF30(item, arrows); }
int OptionsScrollerFNGObject::AddSlider54F410(OptionItem *item, int arrows) { return Add(this, item, arrows, 2); }
int OptionsScrollerFNGObject::AddSlider54F800(OptionItem *item, int arrows) { return AddSlider54F410(item, arrows); }
int OptionsScrollerFNGObject::AddButton(OptionItem *item) { return Add(this, item, 0, 0); }
OptionItem *OptionsScrollerFNGObject::Item(int index) {
    List &list = lists[this]; return index > 0 && index <= (int)list.items.size() ? list.items[index - 1] : 0;
}
void OptionsScrollerFNGObject::StoreMessage(unsigned int message, int a, int b, int c) {
    value138 = message; int values[3] = {a, b, c}; memcpy(unidentified13C, values, sizeof(values));
}
// 0x539650 / 0x5390e0: one-based selection, skip disabled items, no wrap.
void OptionsScrollerFNGObject::Select(int index) {
    List &list = lists[this];
    if (!Item(index) || !Item(index)->enabled28) {
        index = 1; while (Item(index) && !Item(index)->enabled28) ++index;
    }
    if (!Item(index)) return;
    if (Item(list.selected)) Item(list.selected)->UnHighlight();
    list.selected = index; Item(index)->Highlight(package4); Layout(this);
}
// 0x556020: the demo's keyboard messages; mouse, controller reassignment and sound are seams.
void OptionsScrollerFNGObject::MessageHandler(unsigned int message, int a, int b, int c) {
    List &list = lists[this]; OptionItem *item = Item(list.selected);
    if (message == kUp || message == kDown) {
        int step = message == kUp ? -1 : 1;
        for (int index = list.selected + step; Item(index); index += step) {
            if (Item(index)->enabled28) { Select(index); FngSetPackageValue10(package4, index); break; }
        }
    } else if (message == kLeft || message == kRight || message == kAccept || message == kBack) {
        if (item && item->enabled28) {
            item->React(package4, message);
            if (message == kAccept || message == kBack) StoreMessage(message, a, b, c);
            if (getenv("UG2_DEBUG_PAUSE")) {
                OptionWidget *choice = dynamic_cast<OptionWidget *>(item);
                OptionSlider *slider = dynamic_cast<OptionSlider *>(item);
                fprintf(stderr, "options: edit row=%d name=%s data=%s slider=%.2f units=%d camera=%d gauges=%d prompt=%d autosave=%d\n",
                    list.selected, item->name2C ? Object(item->name2C)->text.c_str() : "",
                    choice && choice->data30 ? Object(choice->data30)->text.c_str() : "",
                    slider ? slider->GetValue() : 0, units83AA1A, playerSettings83AA64[0].value10,
                    playerSettings83AA64[0].value0, prompt844AD8, autosave844AD9);
            }
        }
    } else if (message == 0xaf0bbd92) {
        ClearList(this); Setup();
    } else if (message == 0x35f8620b && item && item->enabled28) item->Highlight(package4);
    else if (message == 0x84378bef && item && item->enabled28) item->UnHighlight();
    else if (message == 0x251bf83e || message == 0xc9d30688) FngSetScript(package4, 0x47ff4e7c, "UNDIM", 1);
}

// Widget constructors 0x51dfc0 / 0x51e1b0 / 0x51e230 / 0x51e840.
OptionButton::OptionButton(float *a, float *b, unsigned char e) { InitItem(this, a, b, e); memset(unidentified30, 0, sizeof(unidentified30)); }
OptionWidget::OptionWidget(float *a, float *b, float *c, float *d, unsigned char e) {
    InitItem(this, a, b, e); data30 = 0; memset(unidentified34, 0, sizeof(unidentified34));
    widgets[this].dataWidth = c[0]; widgets[this].dataX = d[0]; widgets[this].dataY = d[1];
}
OptionToggle::OptionToggle(float *a, float *b, float *c, float *d, unsigned char e) : OptionWidget(a,b,c,d,e), leftArrow4C(0), rightArrow50(0) {}
OptionSlider::OptionSlider(float *a, float *b, float *c, float *d, unsigned char e) : OptionWidget(a,b,c,d,e), leftArrow4C(0), rightArrow50(0) {
    memset(bar54.unidentified4, 0, sizeof(bar54.unidentified4));
    memset(bar54.unidentified34, 0, sizeof(bar54.unidentified34)); memset(bar54.unidentified3C, 0, sizeof(bar54.unidentified3C));
    bar54.max24 = bar54.min28 = bar54.previous2C = bar54.value30 = bar54.step38 = 0;
}
OptionItem::~OptionItem() { widgets.erase(this); }
OptionSlider::~OptionSlider() { bars.erase(&bar54); }
void OptionItem::Show() { Visible(name2C, true); }
void OptionItem::Hide() { Visible(name2C, false); }
void OptionItem::Enable() { enabled28 = 1; }
void OptionItem::Disable() { enabled28 = 0; }
void OptionItem::UnknownSlot2C() { Script(widgets[this].backing, "Blink"); }
void OptionItem::UnknownSlot30(int) {}
void OptionItem::UnknownSlot34() {}
int OptionItem::SoundColumn(unsigned int, int row) { return row; }
int OptionItem::SoundRow() { return 0; }
void OptionButton::Click(const char *, int, int) { NativeTrap("options mouse input"); }
void OptionButton::UnknownSlot10() { Widget &w = widgets[this]; Position(name2C, w.x, w.y); Position(w.backing, w.x + 276, w.y + 10); }
void OptionButton::Show() { OptionItem::Show(); Visible(widgets[this].backing, true); }
void OptionButton::Hide() { OptionItem::Hide(); Visible(widgets[this].backing, false); }
void OptionButton::Highlight(const char *p) { NativeFrontendSetFocus(p, Object(name2C)); Script(name2C, "Highlight"); Script(widgets[this].backing, "Highlight"); }
void OptionButton::UnHighlight() { Script(name2C, "UnHighlight"); Script(widgets[this].backing, "UnHighlight"); }
void OptionButton::UnknownSlot2C() {}
int OptionButton::SoundColumn(unsigned int, int row) { return row; }
int OptionButton::SoundRow() { return 0; }
void OptionToggle::Click(const char *, int, int) { NativeTrap("options mouse input"); }
void OptionToggle::UnknownSlot10() {
    Widget &w = widgets[this]; Position(name2C, w.x, w.y);
    Position(data30, w.dataX + w.dataWidth / 2, w.dataY);
    float arrowWidth = rightArrow50 ? Object(rightArrow50)->size[0] : 0;
    Position(leftArrow4C, w.dataX - arrowWidth, w.dataY); Position(rightArrow50, w.dataX + w.dataWidth, w.dataY);
    Position(w.backing, w.x + 276, w.y + 10);
}
void OptionToggle::Show() { OptionItem::Show(); Visible(data30, true); Visible(leftArrow4C, true); Visible(rightArrow50, true); Visible(widgets[this].backing, true); }
void OptionToggle::Hide() { OptionItem::Hide(); Visible(data30, false); Visible(leftArrow4C, false); Visible(rightArrow50, false); Visible(widgets[this].backing, false); }
void OptionToggle::Enable() { enabled28 = 1; HighlightWidget(this, 0, "UnHighlight"); HighlightArrows(leftArrow4C, rightArrow50, "UnHighlight"); }
void OptionToggle::Disable() { enabled28 = 0; HighlightWidget(this, 0, "Disable"); HighlightArrows(leftArrow4C, rightArrow50, "Disable"); }
void OptionToggle::Highlight(const char *p) { HighlightWidget(this, p, "Highlight"); HighlightArrows(leftArrow4C, rightArrow50, "Highlight"); }
void OptionToggle::UnHighlight() { HighlightWidget(this, 0, "UnHighlight"); HighlightArrows(leftArrow4C, rightArrow50, "UnHighlight"); }
void OptionToggle::UnknownSlot2C() {}
void OptionToggle::UnknownSlot30(int) {}
void OptionToggle::UnknownSlot34() {}
int OptionToggle::SoundRow() { return 1; }
void OptionToggle::AnimateArrows(unsigned int m) { Script(m == kLeft ? leftArrow4C : m == kRight ? rightArrow50 : 0, "Blink"); }
void OptionSlider::Click(const char *, int, int) { NativeTrap("options mouse input"); }
void OptionSlider::UnknownSlot10() {
    Widget &w = widgets[this]; Position(name2C, w.x, w.y);
    Bar &b = bars[&bar54]; float centre = w.dataX + w.dataWidth / 2;
    Position(b.base, centre - (b.base ? b.base->size[0] / 2 : 0), w.dataY);
    float arrowWidth = rightArrow50 ? Object(rightArrow50)->size[0] : 0;
    Position(leftArrow4C, w.dataX - arrowWidth, w.dataY); Position(rightArrow50, w.dataX + w.dataWidth, w.dataY);
    Position(w.backing, w.x + 276, w.y + 10); UpdateBar(&bar54);
}
void OptionSlider::Show() { OptionItem::Show(); Visible(leftArrow4C, true); Visible(rightArrow50, true); Visible(widgets[this].backing, true); BarVisible(&bar54, true); }
void OptionSlider::Hide() { OptionItem::Hide(); Visible(leftArrow4C, false); Visible(rightArrow50, false); Visible(widgets[this].backing, false); BarVisible(&bar54, false); }
void OptionSlider::Highlight(const char *p) { HighlightWidget(this, p, "Highlight"); HighlightArrows(leftArrow4C, rightArrow50, "Highlight"); Bar &b = bars[&bar54]; Script(b.base, "Highlight"); Script(b.knob, "Highlight"); Script(b.handle, "Highlight"); Script(b.begin, "Highlight"); Script(b.end, "Highlight"); }
void OptionSlider::UnHighlight() { HighlightWidget(this, 0, "UnHighlight"); HighlightArrows(leftArrow4C, rightArrow50, "UnHighlight"); Bar &b = bars[&bar54]; Script(b.base, "UnHighlight"); Script(b.knob, "UnHighlight"); Script(b.handle, "UnHighlight"); Script(b.begin, "UnHighlight"); Script(b.end, "UnHighlight"); }
void OptionSlider::UnknownSlot2C() {}
void OptionSlider::UnknownSlot30(int) {}
void OptionSlider::UnknownSlot34() {}
int OptionSlider::SoundColumn(unsigned int, int row) { return row; }
int OptionSlider::SoundRow() { return 2; }
void OptionSlider::AnimateArrows(unsigned int m) { Script(m == kLeft ? leftArrow4C : m == kRight ? rightArrow50 : 0, "Blink"); }
float OptionSlider::GetValue() { return bar54.value30; }
UIProgressBar::~UIProgressBar() {}
// 0x4ff480 / 0x4ff550, as read. Rendering update is a native seam.
bool UIProgressBar::HandleMessage(unsigned int m) {
    bool changed = false;
    if (m == kLeft) {
        previous2C = value30; value30 -= step38; if (value30 < min28) value30 = min28;
        changed = !(value30 == min28 && previous2C == min28);
    } else if (m == kRight) {
        previous2C = value30; value30 += step38; if (value30 > max24) value30 = max24;
        changed = !(value30 == max24 && previous2C == max24);
    }
    UpdateBar(this); return changed;
}
void UIProgressBar::SetValue(float v) { previous2C = value30; value30 = v; if (value30 > max24) value30 = max24; if (value30 < min28) value30 = min28; UpdateBar(this); }

// Game/platform interfaces. The demo has no audio backend, registry, save profiles or force
// feedback. Keep calls at their original boundaries and do not claim persistence or audio.
void Audio82B884::Apply(float *) {}
bool Value500B60() { return false; } // no career in the free-drive demo
unsigned char Value5B8240() { return 0; } // no feedback device
int Settings83AAB4::Value5026F0() { return 0; } // single-player demo
bool MemoryCard83A9E0::Value502FE0() { return false; }
void SetPresetDisplayLevelFromLevelOfDetailAndLimit() {} // native renderer has no retail quality presets
void SaveRegistrySettings() { fprintf(stderr, "ug2demo: display settings are session-only; registry persistence is not recovered\n"); }
void Value5CEC90() { fprintf(stderr, "ug2demo: retail display reset is not recovered; native renderer unchanged\n"); }
void Replay8650B8::Value578EF0(int, int, int, bool) {} // no replay recorder
int Player8900AC::Value5F2FA0(int step) {
    // Native camera seam. Original 0x5f2fa0 delegates to profile/camera helpers, still unrecovered.
    int &camera = playerSettings83AA64[PlayerSlot()].value10;
    // 0x502540 as read: four ordinary camera choices, fifth for drift/special tracks.
    int limit = (hudDrift89E7D9 || hudDriftOverride890104 ||
        (trackNumber89E7A0 > 0x104d && trackNumber89E7A0 < 0x1054)) ? 4 : 3;
    if (step == 1) { if (++camera > limit) camera = 0; }
    else if (step == -1) { if (--camera < 0) camera = limit; }
    return DriveCameraTypeForOption(camera);
}
void NativePauseOptionsLoad(const char *gameDirectory) {
    // Read the frontend track tint from the player's assets rather than invent a replacement sprite.
    std::vector<unsigned char> frontend;
    frontendTrackColours.clear();
    if (ReadNativeFile(std::string(gameDirectory) + "/FRONTEND/FrontB.lzc", &frontend)) {
        for (size_t offset = 0; offset + 8 <= frontend.size();) {
            unsigned int id, length;
            memcpy(&id, &frontend[offset], 4); memcpy(&length, &frontend[offset + 4], 4);
            if (length > frontend.size() - offset - 8) break;
            if (id == 0x30203 && length > 0x40 &&
                !memcmp(&frontend[offset + 0x30], "UI_Options.fng", sizeof("UI_Options.fng"))) {
                FengPackage theme; std::string error;
                if (theme.Load(&frontend[0], frontend.size(), offset, &error)) {
                    for (int i = 1; i <= 20; ++i) {
                        char name[32]; snprintf(name, sizeof(name), "SLIDER_BASE_%d", i);
                        FengObject *base = theme.Find(name);
                        if (base) frontendTrackColours[base->hash] = (base->colour[3] << 24) |
                            (base->colour[2] << 16) | (base->colour[1] << 8) | base->colour[0];
                    }
                }
                break;
            }
            offset += 8 + length;
        }
    }
    audio82B884 = &nativeAudio;
    // HUD normally installs slot 0. Supply one for --no-hud as well, without replacing it.
    if (!players8900AC[0]) { memset(&nativePlayer, 0, sizeof(nativePlayer)); players8900AC[0] = &nativePlayer; }
    for (int i = 0; i < 7; ++i) volumes83AA28[i] = 1.0f;
    levelOfDetail87099C = 3; resolution870D1C = 1;
    for (int i = 0; i < 6; ++i) resolutions86E880[i] = 1;
    for (int i = 0; i < 2; ++i) {
        memset(&playerSettings83AA64[i], 0, sizeof(playerSettings83AA64[i]));
        playerSettings83AA64[i].value0 = 1; playerSettings83AA64[i].value1 = 1;
        playerSettings83AA64[i].value2 = 1; playerSettings83AA64[i].value3 = 1;
        playerSettings83AA64[i].value5 = 1; playerSettings83AA64[i].value10 = 3;
    }
    mirror83AA18 = 1; prompt844AD8 = 1; autosave844AD9 = 0;
}

// Native entry point: the original PC Controls page belongs to the frontend Options menu,
// which the driving demo does not yet run. Keep this extra Player row outside recovered setup.
namespace {
class NativeControlsButton : public OptionButton {
public:
    NativeControlsButton(float *position, float *size) : OptionButton(position, size, 1) {}
    void SetLabels() { FengPackage::SetText(Object(name2C), "Controls"); }
    void React(const char *, unsigned int message) { if (message == kAccept) NativeFrontendOpenControls(); }
};
}

// FEng stores the layout baseline separately from script deltas; this native adapter reapplies
// list positions after NativeFng advances its animation state. Scroll animation is not modelled.
void NativePauseOptionsUpdate() {
    for (std::map<OptionsScrollerFNGObject *, List>::iterator it = lists.begin(); it != lists.end(); ++it)
    {
        OptionsScrollerFNGObject *owner = it->first;
        if (value83AA04 == 3 && !strcmp(owner->package4, "UI_PauseOptions.fng")) {
            bool added = false;
            for (size_t i = 0; i < it->second.items.size(); ++i)
                if (dynamic_cast<NativeControlsButton *>(it->second.items[i])) added = true;
            if (!added) owner->AddButton(new NativeControlsButton(owner->value0FC, owner->value10C));
        }
        Layout(owner);
    }
}

bool NativePauseOptionsPointer(void *object, float x, float y, bool click, bool drag) {
    OptionsScrollerFNGObject *owner = static_cast<OptionsScrollerFNGObject *>(object);
    std::map<OptionsScrollerFNGObject *, List>::iterator found = lists.find(owner);
    if (found == lists.end()) return false;
    List &list = found->second;
    FengPackage *package = NativeFrontendPackage(owner->package4);
    if(click && package) {
        if(package->Contains(package->Find(0x444969fd),x,y)) { owner->MessageHandler(kUp,0,0,0);return true; }
        if(package->Contains(package->Find(0x444969fe),x,y)) { owner->MessageHandler(kDown,0,0,0);return true; }
    }
    int visible = owner->value134 > 0 ? owner->value134 : 7;
    for (int i = list.top; i < (int)list.items.size() && i < list.top + visible; ++i) {
        OptionItem *item = list.items[i];
        Widget &w = widgets[item];
        if (!item->enabled28 || !package) continue;
        OptionToggle *toggle = dynamic_cast<OptionToggle *>(item);
        OptionSlider *slider = dynamic_cast<OptionSlider *>(item);
        void *left = slider ? slider->leftArrow4C : toggle ? toggle->leftArrow4C : 0;
        void *right = slider ? slider->rightArrow50 : toggle ? toggle->rightArrow50 : 0;
        bool hitLeft = package->Contains(Object(left), x, y);
        bool hitRight = package->Contains(Object(right), x, y);
        // Retail labels have glyph scale, so use the list's measured row geometry.
        bool hitRow = x >= w.x && x <= w.dataX + w.dataWidth && y >= w.y && y < w.y + list.stepY;
        if (!hitLeft && !hitRight && !hitRow) continue;
        if (list.selected != i + 1) { owner->Select(i + 1); FngSetPackageValue10(owner->package4, i + 1); }
        if(slider && (click || drag) && !hitLeft && !hitRight) {
            Bar &bar=bars[&slider->bar54];
            if(bar.base && bar.base->size[0]>92) {
                float left=bar.base->position[0]-bar.base->size[0]/2+46;
                float fraction=(x-left)/(bar.base->size[0]-92);
                if(fraction<0) fraction=0;if(fraction>1) fraction=1;
                float wanted=slider->bar54.min28+fraction*(slider->bar54.max24-slider->bar54.min28);
                // Map pointer position to recovered left/right item messages. No settings writes here.
                for(int step=0;step<256;++step) {
                    float before=slider->GetValue();
                    if(fabsf(wanted-before)<=slider->bar54.step38*.5f) break;
                    owner->MessageHandler(wanted<before ? kLeft : kRight,0,0,0);
                    if(slider->GetValue()==before) break;
                }
            }
        } else if (click) owner->MessageHandler(hitLeft ? kLeft : hitRight || toggle || slider ? kRight : kAccept, 0, 0, 0);
        return true;
    }
    return false;
}

void NativePause::SetViewSettings(bool metric, int camera) {
    units83AA1A = metric ? 1 : 0;
    playerSettings83AA64[0].value10 = camera;
}
bool NativePause::Metric() const { return units83AA1A != 0; }
int NativePause::CameraOption() const { return playerSettings83AA64[0].value10; }
