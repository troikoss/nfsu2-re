#ifndef UG2_PAUSE_OPTIONS_H
#define UG2_PAUSE_OPTIONS_H

// In-game options: UI_PauseOptionsMain.fng (PauseOptionsMainFNGObject, the Audio/Display/
// Gameplay/Player scroller) and UI_PauseOptions.fng (PauseOptionsFNGObject, the option list of
// the chosen category). Class names other than the two community IDC names are descriptive;
// option classes keep their retail vtables.
#include "PauseMenu.h"
#include "../Audio/AudioSystem.h"

// FEng interfaces at the original call boundaries (cdecl).
void *FngFindObject5379C0(const char *package, unsigned int hash); // 0x5379c0
void FngHideObject(void *object); // 0x50ca00
void FngShowObject(void *object); // 0x50ca50
bool FngScriptRunning(const char *package, unsigned int object, unsigned int script); // 0x537d40
void FngSetScriptHash(const char *package, unsigned int object, unsigned int script, int restart); // 0x537c60
void FngPrintf(void *object, const char *format, ...); // 0x537be0
void FngSetObjectScript(void *object, const char *script, bool restart); // 0x51cf70
void FngPlayMenuSound(const char *package, int row, int column); // 0x50d150, table 0x837fe0

// Game interfaces.
unsigned char Value5B8240(); // 0x5b8240, returns 0x87078c
void SetPresetDisplayLevelFromLevelOfDetailAndLimit(); // 0x5be9f0 (IDC name)
void SaveRegistrySettings(); // 0x5beea0 (IDC name)
void Value5CEC90(); // 0x5cec90, display settings reset
// Profile settings block at 0x83aab4 (profile 0x83a9d0 +0xe4): +0 mode, then a 0x14-byte row per
// mode 0..11 and the mode-12 value at +0x100.
struct Settings83AAB4 {
    int mode0;
    char unidentified4[0xc];
    unsigned char modeRows10[12][0x14];
    unsigned char value100;
    int Value5026F0(); // 0x5026f0
};
UG2_CHECK_OFFSET(Settings83AAB4, value100, 0x100);
// Memory-card state at 0x83a9e0 (profile 0x83a9d0 +0x10).
struct MemoryCard83A9E0 {
    char unidentified0[0x20];
    unsigned char value20;
    unsigned char Value502FE0(); // 0x502fe0, returns +0x20
};

// Per-player settings at 0x83aa64, stride 0x28, indexed by PlayerSlot (0x4a7ae0). Labels are the
// option names that edit each field.
struct PlayerSettings83AA64 {
    unsigned char value0; // Gauges
    unsigned char value1; // Race Position
    unsigned char value2; // Lap Info
    unsigned char value3; // Score
    unsigned char value4; // Feedback
    unsigned char value5; // Leaderboard
    char unidentified6[0xa];
    int value10;          // Camera
    unsigned char value14; // Split Time
    unsigned char value15; // Transmission
    unsigned char value16; // Stability Control
    char unidentified17[0x11];
};
UG2_CHECK_OFFSET(PlayerSettings83AA64, value10, 0x10);
UG2_CHECK_OFFSET(PlayerSettings83AA64, value16, 0x16);
UG2_CHECK_SIZE(PlayerSettings83AA64, 0x28);

int PlayerSlot(); // 0x4a7ae0, inline in most callers

// UIProgressBar (vtable 0x78f544) embedded in slider options at +0x54.
class UIProgressBar {
public:
    virtual ~UIProgressBar();
    virtual bool HandleMessage(unsigned int message); // 0x4ff480
    void SetValue(float value); // 0x4ff550 (IDC name UIProgressBar::SetValue)
    char unidentified4[0x20];
    float max24;
    float min28;
    float previous2C;
    float value30;
    char unidentified34[4];
    float step38;
    char unidentified3C[8];
};
UG2_CHECK_OFFSET(UIProgressBar, max24, 0x24);
UG2_CHECK_OFFSET(UIProgressBar, step38, 0x38);
UG2_CHECK_SIZE(UIProgressBar, 0x44);

// Option list item root, vtable 0x790b98. Slot names follow what the retail bodies do.
class OptionItem {
public:
    virtual ~OptionItem(); // 0x4cf650 (scalar 0x4aea10)
    virtual void React(const char *package, unsigned int message) = 0;
    virtual void Click(const char *package, int x, int y) = 0;
    virtual void SetLabels() = 0;
    virtual void UnknownSlot10() = 0;
    virtual void Show(); // 0x4ae970
    virtual void Hide(); // 0x4ae980
    virtual void Enable(); // 0x50d7c0
    virtual void Disable(); // 0x50d7d0
    virtual void Highlight(const char *package) = 0;
    virtual void UnHighlight() = 0;
    virtual void UnknownSlot2C(); // 0x4ae990
    virtual void UnknownSlot30(int value); // 0x4047f0
    virtual void UnknownSlot34(); // 0x4ae9b0
    virtual int SoundColumn(unsigned int message, int row); // 0x50d500
    virtual int SoundRow(); // 0x50d510
    char unidentified4[0x24];
    unsigned char enabled28;
    char unidentified29[3];
    void *name2C;
};
UG2_CHECK_OFFSET(OptionItem, enabled28, 0x28);
UG2_CHECK_OFFSET(OptionItem, name2C, 0x2c);

// Button item, constructor 0x51dfc0 (IDC ControllerOptionsRelated::ctor_data), vtable 0x79b2f8.
class OptionButton : public OptionItem {
public:
    OptionButton(float *position, float *size, unsigned char enabled); // 0x51dfc0
    void Click(const char *package, int x, int y); // 0x52d650
    void UnknownSlot10(); // 0x52d6b0
    void Show(); // 0x50d530
    void Hide(); // 0x50d550
    void Highlight(const char *package); // 0x52d770
    void UnHighlight(); // 0x51e020
    void UnknownSlot2C(); // 0x4ae9c0
    int SoundColumn(unsigned int message, int row); // 0x50d800
    int SoundRow(); // 0x40cbb0
    char unidentified30[8];
};
UG2_CHECK_SIZE(OptionButton, 0x38);

// Name/data item base, constructor 0x51e1b0, vtable 0x79b378.
class OptionWidget : public OptionItem {
public:
    OptionWidget(float *position, float *size, float *value3C, float *value44, unsigned char enabled); // 0x51e1b0
    void *data30;
    char unidentified34[0x18];
};
UG2_CHECK_OFFSET(OptionWidget, data30, 0x30);
UG2_CHECK_SIZE(OptionWidget, 0x4c);

// Left/right choice, constructor 0x51e230, vtable 0x79b3b8.
class OptionToggle : public OptionWidget {
public:
    OptionToggle(float *position, float *size, float *value3C, float *value44, unsigned char enabled); // 0x51e230
    void Click(const char *package, int x, int y); // 0x52d9f0
    void UnknownSlot10(); // 0x52dac0
    void Show(); // 0x50d5d0
    void Hide(); // 0x50d610
    void Enable(); // 0x51e270
    void Disable(); // 0x51e3e0
    void Highlight(const char *package); // 0x52db40
    void UnHighlight(); // 0x51e550
    void UnknownSlot2C(); // 0x4ae9c0
    void UnknownSlot30(int value); // 0x52d930
    void UnknownSlot34(); // 0x52d990
    int SoundRow(); // 0x408cd0
    virtual void AnimateArrows(unsigned int message); // 0x51e6b0
    void *leftArrow4C;
    void *rightArrow50;
};
UG2_CHECK_SIZE(OptionToggle, 0x54);

// Slider, constructor 0x51e840, vtable 0x79b400.
class OptionSlider : public OptionWidget {
public:
    OptionSlider(float *position, float *size, float *value3C, float *value44, unsigned char enabled); // 0x51e840
    ~OptionSlider(); // 0x4cf640 (scalar 0x4cf620)
    void Click(const char *package, int x, int y); // 0x52d9f0
    void UnknownSlot10(); // 0x52def0
    void Show(); // 0x50d740
    void Hide(); // 0x50d780
    void Highlight(const char *package); // 0x52dff0
    void UnHighlight(); // 0x51e8d0
    void UnknownSlot2C(); // 0x4ae9c0
    void UnknownSlot30(int value); // 0x52d930
    void UnknownSlot34(); // 0x52d990
    int SoundColumn(unsigned int message, int row); // 0x50d6f0
    int SoundRow(); // 0x40cb20
    virtual void AnimateArrows(unsigned int message); // 0x51e6b0
    virtual void InitSlider() = 0;
    virtual float GetValue(); // 0x45d300
    void *leftArrow4C;
    void *rightArrow50;
    UIProgressBar bar54;
};
UG2_CHECK_OFFSET(OptionSlider, bar54, 0x54);
UG2_CHECK_SIZE(OptionSlider, 0x98);

// Options list screen base: constructor 0x552780, destructor 0x4cbc30, vtable 0x795cdc, 0x14c bytes.
class OptionsScrollerFNGObject {
public:
    OptionsScrollerFNGObject(void *data); // 0x552780
    virtual ~OptionsScrollerFNGObject(); // 0x4cbc30
    virtual int UnknownSlot04(int value); // 0x40d080
    virtual void MessageHandler(unsigned int message, int value8, int valueC, int value10); // 0x556020
    virtual int UnknownSlot0C(int value4, int value8, int valueC, int value10, int value14); // 0x570a70
    virtual void Setup() = 0;
    int AddOption54EF30(OptionItem *item, int arrows); // 0x54ef30
    int AddOption54F170(OptionItem *item, int arrows); // 0x54f170, tail call to 0x54ef30
    int AddSlider54F410(OptionItem *item, int arrows); // 0x54f410
    int AddSlider54F800(OptionItem *item, int arrows); // 0x54f800, tail call to 0x54f410
    int AddButton(OptionItem *item); // 0x54ee40
    OptionItem *Item(int index); // 0x51fe70, 1-based
    void StoreMessage(unsigned int message, int value8, int valueC, int value10); // 0x50dae0
    void Select(int index); // 0x539650

    const char *package4;
    char unidentified8[0x18];
    int value20;
    char unidentified24[0xd8];
    float value0FC[2];
    char unidentified104[8];
    float value10C[2];
    float value114[2];
    float value11C[2];
    char unidentified124[0x10];
    int value134;
    unsigned int value138;
    char unidentified13C[0x10];
};
UG2_CHECK_OFFSET(OptionsScrollerFNGObject, value0FC, 0xfc);
UG2_CHECK_OFFSET(OptionsScrollerFNGObject, value11C, 0x11c);
UG2_CHECK_OFFSET(OptionsScrollerFNGObject, value134, 0x134);
UG2_CHECK_OFFSET(OptionsScrollerFNGObject, value138, 0x138);
UG2_CHECK_SIZE(OptionsScrollerFNGObject, 0x14c);

// PauseOptionsMainFNGObject (UI_PauseOptionsMain.fng): constructor 0x4eb580, vtable 0x797ccc.
class PauseOptionsMainFNGObject : public IconScrollerFNGObject {
public:
    PauseOptionsMainFNGObject(void *data); // 0x4eb580
    ~PauseOptionsMainFNGObject(); // 0x4eb640, scalar 0x4eb620
    void MessageHandler(unsigned int message, int value8, int valueC, int value10); // 0x4a7d60
    int UnknownSlot0C(int value4, int value8, int valueC, int value10, int value14); // 0x49dfc0
    void Setup(); // 0x4c3000

    unsigned char value150;
    char unidentified151[3];
};
UG2_CHECK_OFFSET(PauseOptionsMainFNGObject, value150, 0x150);
UG2_CHECK_SIZE(PauseOptionsMainFNGObject, 0x154);

// Category icons; React stores the category read by PauseOptionsFNGObject::Setup.
class PauseOptionsIcon7918B8 : public IconScrollerIcon { // React 0x49a620 (Audio)
public:
    PauseOptionsIcon7918B8(unsigned int a, unsigned int b, unsigned int c) : IconScrollerIcon(a, b, c) {}
    void React(const char *package, unsigned int message, int value8, int valueC, int value10);
};
class PauseOptionsIcon7918C0 : public IconScrollerIcon { // React 0x49a640 (Display)
public:
    PauseOptionsIcon7918C0(unsigned int a, unsigned int b, unsigned int c) : IconScrollerIcon(a, b, c) {}
    void React(const char *package, unsigned int message, int value8, int valueC, int value10);
};
class PauseOptionsIcon7918C8 : public IconScrollerIcon { // React 0x49a660 (Gameplay)
public:
    PauseOptionsIcon7918C8(unsigned int a, unsigned int b, unsigned int c) : IconScrollerIcon(a, b, c) {}
    void React(const char *package, unsigned int message, int value8, int valueC, int value10);
};
class PauseOptionsIcon7918D0 : public IconScrollerIcon { // React 0x49a680 (Player)
public:
    PauseOptionsIcon7918D0(unsigned int a, unsigned int b, unsigned int c) : IconScrollerIcon(a, b, c) {}
    void React(const char *package, unsigned int message, int value8, int valueC, int value10);
};

// PauseOptionsFNGObject (UI_PauseOptions.fng): constructor 0x4d93c0, vtable 0x796504, 0x15c bytes.
class PauseOptionsFNGObject : public OptionsScrollerFNGObject {
public:
    PauseOptionsFNGObject(void *data); // 0x4d93c0
    ~PauseOptionsFNGObject(); // 0x4d9470, scalar 0x4eb6b0
    void MessageHandler(unsigned int message, int value8, int valueC, int value10); // 0x4a8010
    void Setup(); // 0x4c4bd0
    bool PromptSave(unsigned int result); // 0x4a7f70
    void SetupAudio(); // 0x4c3370
    void SetupDisplay(); // 0x4c36c0
    void SetupGameplay(); // 0x4c38f0
    void SetupPlayer(); // 0x4c4740

    OptionItem *stability14C;
    unsigned char value150;
    char unidentified151[3];
    int feedback154;
    unsigned char value158;
    unsigned char prompt159;
    unsigned char autosave15A;
    unsigned char dialog15B;
};
UG2_CHECK_OFFSET(PauseOptionsFNGObject, stability14C, 0x14c);
UG2_CHECK_OFFSET(PauseOptionsFNGObject, feedback154, 0x154);
UG2_CHECK_OFFSET(PauseOptionsFNGObject, dialog15B, 0x15b);
UG2_CHECK_SIZE(PauseOptionsFNGObject, 0x15c);

IconScrollerFNGObject *CreatePauseOptionsMainFNGObject(void *data); // 0x4f47f0
OptionsScrollerFNGObject *CreatePauseOptionsFNGObject(void *data); // 0x4eb650

// Option items. Each overrides React (slot 1) and SetLabels (slot 3); sliders also InitSlider.
#define UG2_SLIDER_OPTION(vtable)                                                                    \
    class PauseOption##vtable : public OptionSlider {                                                \
    public:                                                                                           \
        PauseOption##vtable(float *a, float *b, float *c, float *d, unsigned char e) : OptionSlider(a, b, c, d, e) {} \
        void React(const char *package, unsigned int message);                                       \
        void SetLabels();                                                                             \
        void InitSlider();                                                                            \
    }
#define UG2_TOGGLE_OPTION(vtable)                                                                    \
    class PauseOption##vtable : public OptionToggle {                                                \
    public:                                                                                           \
        PauseOption##vtable(float *a, float *b, float *c, float *d, unsigned char e) : OptionToggle(a, b, c, d, e) {} \
        void React(const char *package, unsigned int message);                                       \
        void SetLabels();                                                                             \
    }
#define UG2_BUTTON_OPTION(vtable)                                                                    \
    class PauseOption##vtable : public OptionButton {                                                \
    public:                                                                                           \
        PauseOption##vtable(float *a, float *b, unsigned char e) : OptionButton(a, b, e) {}                   \
        void React(const char *package, unsigned int message);                                       \
        void SetLabels();                                                                             \
    }

// Audio (0x4c3370): volumes at 0x83aa28..0x83aa40.
UG2_SLIDER_OPTION(7918F8); // Master Volume, 0x49a720/0x49a770/0x49a790
UG2_SLIDER_OPTION(791A88); // Menu Music Volume, 0x49aa40/0x49aa90/0x49aab0
UG2_SLIDER_OPTION(791AD8); // Game Music Volume, 0x49aae0/0x49ab30/0x49ab50
UG2_SLIDER_OPTION(791A38); // Speech & Movie Volume, 0x49a9a0/0x49a9f0/0x49aa10
UG2_SLIDER_OPTION(791948); // Sound Effects Volume, 0x49a7c0/0x49a810/0x49a830
UG2_SLIDER_OPTION(791998); // Tire Volume, 0x49a860/0x49a8b0/0x49a8d0
UG2_SLIDER_OPTION(7919E8); // Engine Volume, 0x49a900/0x49a950/0x49a970

// Display (0x4c36c0).
UG2_TOGGLE_OPTION(791F30); // Resolution, 0x49b600/0x49b690
class PauseOption791F78 : public OptionSlider { // Level Of Detail, 0x49b740/0x49b7a0/0x49b7c0
public:
    PauseOption791F78(float *a, float *b, float *c, float *d, unsigned char e) : OptionSlider(a, b, c, d, e), resolution98(0) {}
    void React(const char *package, unsigned int message);
    void SetLabels();
    void InitSlider();
    OptionItem *resolution98;
};
UG2_CHECK_SIZE(PauseOption791F78, 0x9c);
UG2_BUTTON_OPTION(792060); // Customize Display Settings, 0x49b930/0x49b960
UG2_BUTTON_OPTION(7920A0); // Default Settings, 0x49b980/0x49b9e0

// Gameplay (0x4c38f0).
UG2_TOGGLE_OPTION(791D20); // Career Difficulty, 0x49af20/0x49af70
UG2_TOGGLE_OPTION(791C00); // Jump Camera, 0x49ad40/0x49adc0
UG2_TOGGLE_OPTION(791C48); // Rearview Mirror, 0x49ae00/0x49ae40
UG2_TOGGLE_OPTION(791C90); // Speedometer Units, 0x49ae80/0x49aee0
UG2_TOGGLE_OPTION(791D68); // Explore Map Mode, 0x49b0b0/0x49b100
UG2_TOGGLE_OPTION(791CD8); // Race Map Mode, 0x49afe0/0x49b040
class PauseOption793D68 : public OptionToggle { // Save, 0x49b170/0x49b210 (0x58-byte allocation)
public:
    PauseOption793D68(float *a, float *b, float *c, float *d, unsigned char e) : OptionToggle(a, b, c, d, e) {}
    void React(const char *package, unsigned int message);
    void SetLabels();
    char unidentified54[4];
};
UG2_CHECK_SIZE(PauseOption793D68, 0x58);

// Player (0x4c4740): per-player settings 0x83aa64.
UG2_TOGGLE_OPTION(795660); // Transmission, 0x4c3c70/0x4c3d80
UG2_TOGGLE_OPTION(7955F0); // Stability Control, 0x4c31d0/0x4c32e0
UG2_TOGGLE_OPTION(7956A8); // Camera, 0x4c3e10/0x4c3e60
class PauseOption791DB0 : public OptionToggle { // Feedback:, 0x49b260/0x49b2b0, slots 7/8 0x4b5f70/0x4b5fa0
public:
    // Parameter order keeps the original evaluation order (0x5b8240 before 0x4a7ae0).
    PauseOption791DB0(float *a, float *b, float *c, float *d, int player, unsigned char e)
        : OptionToggle(a, b, c, d, e), player54(player) {}
    void React(const char *package, unsigned int message);
    void SetLabels();
    void Enable(); // 0x4b5f70
    void Disable(); // 0x4b5fa0
    int player54;
};
UG2_CHECK_SIZE(PauseOption791DB0, 0x58);
UG2_TOGGLE_OPTION(7956F0); // Gauges, 0x4c3f50/0x4c4010
UG2_TOGGLE_OPTION(795738); // Race Position, 0x4c4090/0x4c4150
UG2_TOGGLE_OPTION(795780); // Lap Info, 0x4c41d0/0x4c4290
UG2_TOGGLE_OPTION(795810); // Split Time, 0x4c4450/0x4c4510
UG2_TOGGLE_OPTION(795858); // Leaderboard, 0x4c4600/0x4c46c0
UG2_TOGGLE_OPTION(7957C8); // Score, 0x4c4310/0x4c43d0

#undef UG2_SLIDER_OPTION
#undef UG2_TOGGLE_OPTION
#undef UG2_BUTTON_OPTION

#endif
