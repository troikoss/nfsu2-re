#include "PauseOptions.h"
#include "../Hud/HudFreeroam.h"

extern CareerData85AD40 careerData85AD40;

extern GameFlow8900D8 *gameFlow8900D8;
extern PCHelpBarFNGObject *pcHelpBar8384D8;
extern Replay8650B8 *replay8650B8;
extern Player8900AC *players8900AC[2];
extern int gameMode850078;
extern int value83AA04;
extern unsigned char value89E7E6;
extern unsigned char value89E7D8;
extern int value890108;
// Audio volumes at 0x83aa28: master, speech, menu music, game music, effects, engine, tire.
extern float volumes83AA28[7];
extern Audio82B884 *audio82B884;
extern float jumpCamera83AA20;
extern unsigned char mirror83AA18;
extern unsigned char units83AA1A;
extern unsigned char raceMap83AA1B;
extern unsigned char exploreMap83AA1C;
extern int difficulty83AC30;
extern PlayerSettings83AA64 playerSettings83AA64[2];
extern Settings83AAB4 settings83AAB4;
extern MemoryCard83A9E0 memoryCard83A9E0;
extern unsigned char prompt844AD8;
extern unsigned char autosave844AD9;
extern int resolution870D1C;
extern int resolutions86E880[6];
extern int levelOfDetail87099C;
extern unsigned char displayChanged8709D1;

static const unsigned int kAccept = 0x0c407210;
static const unsigned int kPadBack = 0x911ab364;
static const unsigned int kPadLeft = 0x9120409e;
static const unsigned int kPadRight = 0xb5971bf1;
static const unsigned int kPadStart = 0xb5af2461;
static const unsigned int kOff = 0xccfa;
static const unsigned int kOn = 0x63c;

// Inline in every original caller.
static inline void RemoveHelpBar() {
    if (FngIsLoaded(FngHelpBarName())) {
        FngRemove(FngHelpBarName());
    }
}

// Original 0x4a7ae0: 0 when the current player is the first table entry, else whether it is the second.
int PlayerSlot() {
    int player = gameFlow8900D8->player20;
    if (player == (players8900AC[0] ? players8900AC[0]->player8 : -1)) {
        return 0;
    }
    return player == (players8900AC[1] ? players8900AC[1]->player8 : -1);
}

static inline bool Horizontal(unsigned int message) {
    return message == kPadLeft || message == kPadRight;
}

// ---- UI_PauseOptionsMain.fng ---------------------------------------------------------------

// Original 0x4f47f0 (FNGINIT_UI_PauseOptionsMain.fng).
IconScrollerFNGObject *CreatePauseOptionsMainFNGObject(void *data) {
    return new PauseOptionsMainFNGObject(data);
}

// Original 0x4eb580.
PauseOptionsMainFNGObject::PauseOptionsMainFNGObject(void *data) : IconScrollerFNGObject(data) {
    value150 = 0;
    Setup();
    if (!FngIsLoaded(FngHelpBarName())) {
        FngAdd(FngHelpBarName(), package4, 0xff, 0);
    }
    pcHelpBar8384D8->Show();
    pcHelpBar8384D8->SyncByMask(value20, package4);
}

// Original 0x4eb640; scalar deleting destructor 0x4eb620 is compiler generated.
PauseOptionsMainFNGObject::~PauseOptionsMainFNGObject() {
}

// Original 0x49dfc0 (shared with PauseFNGObject).
int PauseOptionsMainFNGObject::UnknownSlot0C(int value4, int, int, int, int value14) {
    return FngFilterSlot0C(value4, value14);
}

// Original 0x4c3000.
void PauseOptionsMainFNGObject::Setup() {
    FngSetLabelString(FngFindObject(package4, FngHash("Header_text")), 0x1d7bb6c9);
    FngSetLabelString(FngFindObject(package4, FngHash("Back_button")), 0x933dd695);
    FngSetLabelString(FngFindObject(package4, FngHash("Select_button")), 0x8202c364);
    AddIcon(new PauseOptionsIcon7918B8(0xf37af144, 0xe76cd783, 0));
    AddIcon(new PauseOptionsIcon7918C0(0x8a006328, 0x4204d527, 0));
    AddIcon(new PauseOptionsIcon7918C8(0x4df98fb2, 0xd0cf6ee1, 0));
    AddIcon(new PauseOptionsIcon7918D0(0xd708efef, 0xf760eabe, 0));
    Scroller()->UnknownSlot20(FngGetPackageValue10(package4));
    SelectedIconChanged();
}

// Original 0x4a7d60.
void PauseOptionsMainFNGObject::MessageHandler(unsigned int message, int value8, int valueC, int value10) {
    IconScrollerFNGObject::MessageHandler(message, value8, valueC, value10);
    switch (message) {
    case 0xc98356ba:
        if (value150) {
            FngSetPackageValue10(package4, 0);
            FngRemove(package4);
            gameFlow8900D8->Queue8(1);
        }
        break;
    case kPadBack:
        if (!FngScriptActive(package4, 0x47ff4e7c, "FORWARD")) {
            FngSetPackageValue10(package4, 0);
            FngSetScript(package4, 0x47ff4e7c, "FORWARD", 1);
            RemoveHelpBar();
        }
        break;
    case kPadStart:
        if (ControllerAssigned(gameFlow8900D8->player20) != 0 && !FngScriptActive(package4, 0x47ff4e7c, "FORWARD")) {
            RemoveHelpBar();
            StopInput();
            Value50DA20(kPadStart, value8, valueC, value10);
            FngSetScript(package4, 0x47ff4e7c, "FORWARD", 1);
            FngSetPackageValue10(package4, 0);
            FngSetPackageValue10("UI_Pause.fng", 0);
        }
        break;
    case 0xe1fde1d1:
        switch (value13C) {
        case kAccept:
            FngShow(package4, "UI_PauseOptions.fng", 0);
            break;
        case kPadBack:
            FngShow(package4, value89E7E6 ? "UI_PerformanceDyno_MAIN.fng" : "UI_Pause.fng", 0);
            break;
        case kPadStart:
            value150 = 1;
            break;
        }
        break;
    }
}

// Originals 0x49a620, 0x49a640, 0x49a660, 0x49a680.
void PauseOptionsIcon7918B8::React(const char *, unsigned int message, int, int, int) {
    if (message == kAccept) {
        value83AA04 = 0;
    }
}

void PauseOptionsIcon7918C0::React(const char *, unsigned int message, int, int, int) {
    if (message == kAccept) {
        value83AA04 = 1;
    }
}

void PauseOptionsIcon7918C8::React(const char *, unsigned int message, int, int, int) {
    if (message == kAccept) {
        value83AA04 = 2;
    }
}

void PauseOptionsIcon7918D0::React(const char *, unsigned int message, int, int, int) {
    if (message == kAccept) {
        value83AA04 = 3;
    }
}

// ---- UI_PauseOptions.fng --------------------------------------------------------------------

// Original 0x4eb650 (FNGINIT_UI_PauseOptions.fng).
OptionsScrollerFNGObject *CreatePauseOptionsFNGObject(void *data) {
    return new PauseOptionsFNGObject(data);
}

// Original 0x4d93c0.
PauseOptionsFNGObject::PauseOptionsFNGObject(void *data) : OptionsScrollerFNGObject(data) {
    value150 = 0;
    feedback154 = 0;
    value158 = 0;
    stability14C = 0;
    value134 = 8;
    Setup();
    if (!FngIsLoaded(FngHelpBarName())) {
        FngAdd(FngHelpBarName(), package4, 0xff, 0);
    }
    dialog15B = 0;
}

// Original 0x4d9470; scalar deleting destructor 0x4eb6b0 is compiler generated.
PauseOptionsFNGObject::~PauseOptionsFNGObject() {
}

// Original 0x4c4bd0.
void PauseOptionsFNGObject::Setup() {
    value150 = 0;
    FngHideObject(FngFindObject5379C0(package4, 0x444969fd));
    FngHideObject(FngFindObject5379C0(package4, 0x444969fe));
    switch (value83AA04) {
    case 0:
        SetupAudio();
        break;
    case 1:
        SetupDisplay();
        break;
    case 2:
        SetupGameplay();
        break;
    case 3:
        SetupPlayer();
        FngShowObject(FngFindObject5379C0(package4, 0x444969fd));
        FngShowObject(FngFindObject5379C0(package4, 0x444969fe));
        break;
    }
    Select(FngGetPackageValue10(package4));
}

// Original 0x4a7f70: asks about the save mode the Gameplay page changed; false while the dialog is up.
bool PauseOptionsFNGObject::PromptSave(unsigned int result) {
    if (!prompt844AD8 && prompt159 == 1) {
        dialog15B = 1;
        FngShowDialog(package4, "", 0xf770, 0x61c, result, 0x22222222, 0x22222222, 1, 0x22bc94af);
        return false;
    }
    if (autosave844AD9 == 1 && !autosave15A) {
        dialog15B = 1;
        FngShowDialog(package4, "", 0xf770, 0x61c, result, 0x22222222, 0x22222222, 1, 0xf3568b86);
        return false;
    }
    return true;
}

// Original 0x4a8010.
void PauseOptionsFNGObject::MessageHandler(unsigned int message, int value8, int valueC, int value10) {
    if (dialog15B && (message == kPadBack || message == kAccept)) {
        return;
    }
    if (message == 0xc98356ba) {
        if (value158) {
            RemoveHelpBar();
            FngRemove(package4);
            gameFlow8900D8->Queue8(1);
            return;
        }
    } else if (message == kPadBack) {
        if (value83AA04 == 2 && !PromptSave(0x11111111)) {
            return;
        }
        if (!FngScriptRunning(package4, 0x47ff4e7c, 0xde6eff34)) {
            FngSetScriptHash(package4, 0x47ff4e7c, 0xde6eff34, 1);
        }
        OptionsScrollerFNGObject::MessageHandler(kPadBack, value8, valueC, value10);
        return;
    }
    OptionsScrollerFNGObject::MessageHandler(message, value8, valueC, value10);
    switch (message) {
    case 0x22222222:
        value138 = 0;
        FngSetScript(package4, 0x47ff4e7c, "CALL", 1);
        dialog15B = 0;
        return;
    case 0x11111111:
        dialog15B = 0;
        value138 = kPadBack;
        FngSetScript(package4, 0x47ff4e7c, "FORWARD", 1);
        FngSetPackageValue10(package4, 0);
        FngShow(package4, "UI_PauseOptionsMain.fng", 0);
        return;
    case 0x33333333:
        dialog15B = 0;
        value158 = 1;
        FngSetPackageValue10(package4, 0);
        FngSetPackageValue10("UI_PauseOptionsMain.fng", 0);
        FngSetPackageValue10("UI_Pause.fng", 0);
        return;
    case 0x35f8620b: {
        OptionItem *item = Item(feedback154);
        if (item && !item->enabled28) {
            item->Disable();
            item->SetLabels();
        }
        break;
    }
    case kPadStart:
        StoreMessage(kPadStart, value8, valueC, value10);
        FngSetScript(package4, 0x47ff4e7c, "FORWARD", 1);
        if (value83AA04 == 2 && !PromptSave(0x33333333)) {
            return;
        }
        RemoveHelpBar();
        FngSendMessage(0x33333333, package4);
        return;
    case 0xd05fc3a3:
        if (value83AA04 == 1) {
            Value5CEC90();
            displayChanged8709D1 = 1;
            FngSendMessage(0xaf0bbd92, package4);
        }
        return;
    case 0xe1fde1d1:
        break;
    default:
        return;
    }
    switch (value138) {
    case kAccept:
        FngSetPackageValue10(package4, 0);
        FngShow(package4, "UI_PC_Customize_Options.fng", 0);
        break;
    case kPadBack:
        if (!dialog15B) {
            if (value83AA04 == 1) {
                SaveRegistrySettings();
            }
            FngSetPackageValue10(package4, 0);
            FngShow(package4, "UI_PauseOptionsMain.fng", 0);
        }
        break;
    case kPadStart:
        if (!dialog15B) {
            value158 = 1;
        }
        break;
    }
}

static inline void SetHeader(const char *package, unsigned int category) {
    FngSetLabelString(FngFindObject(package, FngHash("Header_text")), 0x1d7bb6c9);
    FngSetLabelString(FngFindObject(package, FngHash("Header_text2")), category);
    FngSetLabelString(FngFindObject(package, FngHash("Back_button")), 0x933dd695);
    FngSetLabelString(FngFindObject(package, FngHash("Select_button")), 0x8202c364);
}

// Original 0x4c3370.
void PauseOptionsFNGObject::SetupAudio() {
    SetHeader(package4, 0x737d1b97);
    AddSlider54F800(new PauseOption7918F8(value0FC, value10C, value114, value11C, 1), 1);
    AddSlider54F800(new PauseOption791A88(value0FC, value10C, value114, value11C, 1), 1);
    AddSlider54F800(new PauseOption791AD8(value0FC, value10C, value114, value11C, 1), 1);
    AddSlider54F800(new PauseOption791A38(value0FC, value10C, value114, value11C, 1), 1);
    AddSlider54F800(new PauseOption791948(value0FC, value10C, value114, value11C, 1), 1);
    AddSlider54F800(new PauseOption791998(value0FC, value10C, value114, value11C, 1), 1);
    AddSlider54F800(new PauseOption7919E8(value0FC, value10C, value114, value11C, 1), 1);
}

// Original 0x4c36c0.
void PauseOptionsFNGObject::SetupDisplay() {
    SetHeader(package4, 0xef1bb23b);
    PauseOption791F30 *resolution = new PauseOption791F30(value0FC, value10C, value114, value11C, 1);
    AddOption54EF30(resolution, 1);
    PauseOption791F78 *detail = new PauseOption791F78(value0FC, value10C, value114, value11C, 1);
    AddSlider54F410(detail, 1);
    detail->resolution98 = resolution;
    AddButton(new PauseOption792060(value0FC, value10C, 1));
    AddButton(new PauseOption7920A0(value0FC, value10C, 1));
}

// Original 0x4c38f0.
void PauseOptionsFNGObject::SetupGameplay() {
    prompt159 = prompt844AD8;
    autosave15A = autosave844AD9;
    SetHeader(package4, 0xe8647e75);
    if (careerData85AD40.Query500B60()) {
        AddOption54EF30(new PauseOption791D20(value0FC, value10C, value114, value11C, 1), 1);
    }
    AddOption54F170(new PauseOption791C00(value0FC, value10C, value114, value11C, 1), 1);
    if (settings83AAB4.Value5026F0() != 2) {
        AddOption54F170(new PauseOption791C48(value0FC, value10C, value114, value11C, 1), 1);
    }
    AddOption54F170(new PauseOption791C90(value0FC, value10C, value114, value11C, 1), 1);
    if (settings83AAB4.Value5026F0() != 2) {
        AddOption54F170(new PauseOption791D68(value0FC, value10C, value114, value11C, 1), 1);
    }
    AddOption54F170(new PauseOption791CD8(value0FC, value10C, value114, value11C, 1), 1);
    AddOption54F170(new PauseOption793D68(value0FC, value10C, value114, value11C, 1), 1);
}

// Original 0x4c4740.
void PauseOptionsFNGObject::SetupPlayer() {
    SetHeader(package4, 0xd9dc2f12);
    if (!value89E7D8 && value890108 == 0) {
        AddOption54F170(new PauseOption795660(value0FC, value10C, value114, value11C, 1), 1);
    }
    OptionItem *stability = new PauseOption7955F0(value0FC, value10C, value114, value11C, 1);
    stability14C = stability;
    AddOption54F170(stability, 1);
    AddOption54F170(new PauseOption7956A8(value0FC, value10C, value114, value11C, 1), 1);
    feedback154 = AddOption54F170(new PauseOption791DB0(value0FC, value10C, value114, value11C, PlayerSlot(),
                                                        Value5B8240()), 1);
    AddOption54F170(new PauseOption7956F0(value0FC, value10C, value114, value11C, 1), 1);
    if (!value89E7E6) {
        AddOption54F170(new PauseOption795738(value0FC, value10C, value114, value11C, 1), 1);
        AddOption54F170(new PauseOption795780(value0FC, value10C, value114, value11C, 1), 1);
        AddOption54F170(new PauseOption795810(value0FC, value10C, value114, value11C, 1), 1);
        AddOption54F170(new PauseOption795858(value0FC, value10C, value114, value11C, 1), 1);
    }
    AddOption54F170(new PauseOption7957C8(value0FC, value10C, value114, value11C, 1), 1);
}

// ---- Audio ----------------------------------------------------------------------------------

// Shared body of the seven volume sliders: the bar takes the message, the volume follows it.
#define UG2_VOLUME_OPTION(vtable, index, label)                         \
    void PauseOption##vtable::React(const char *, unsigned int message) { \
        if (bar54.HandleMessage(message)) {                              \
            AnimateArrows(message);                                       \
        }                                                                 \
        volumes83AA28[index] = GetValue();                               \
        audio82B884->Apply(volumes83AA28);                               \
        SetLabels();                                                      \
    }                                                                     \
    void PauseOption##vtable::SetLabels() {                              \
        FngSetLabelString(name2C, label);                                 \
    }                                                                     \
    void PauseOption##vtable::InitSlider() {                             \
        bar54.min28 = 0.0f;                                               \
        bar54.max24 = 1.0f;                                               \
        bar54.step38 = 0.05f;                                             \
        bar54.SetValue(volumes83AA28[index]);                            \
    }

UG2_VOLUME_OPTION(7918F8, 0, 0x26782c3e) // 0x49a720, 0x49a770, 0x49a790
UG2_VOLUME_OPTION(791A88, 2, 0x418e681d) // 0x49aa40, 0x49aa90, 0x49aab0
UG2_VOLUME_OPTION(791AD8, 3, 0xdf21eac2) // 0x49aae0, 0x49ab30, 0x49ab50
UG2_VOLUME_OPTION(791A38, 1, 0x9e5fb82a) // 0x49a9a0, 0x49a9f0, 0x49aa10
UG2_VOLUME_OPTION(791948, 4, 0xfd487543) // 0x49a7c0, 0x49a810, 0x49a830
UG2_VOLUME_OPTION(791998, 6, 0x69b745c6) // 0x49a860, 0x49a8b0, 0x49a8d0
UG2_VOLUME_OPTION(7919E8, 5, 0xa2b1f888) // 0x49a900, 0x49a950, 0x49a970

#undef UG2_VOLUME_OPTION

// ---- Display --------------------------------------------------------------------------------

// Original 0x49b600: steps over unavailable modes; keeps the old mode when none is left.
void PauseOption791F30::React(const char *, unsigned int message) {
    int previous = resolution870D1C;
    int mode = previous;
    if (message == kPadRight) {
        if (mode != 5) {
            do {
                ++mode;
            } while (!resolutions86E880[mode] && mode != 5);
            resolution870D1C = mode;
        }
    } else if (message == kPadLeft) {
        if (mode != 0) {
            do {
                --mode;
            } while (!resolutions86E880[mode] && mode != 0);
            resolution870D1C = mode;
        }
    }
    if (!resolutions86E880[mode]) {
        resolution870D1C = previous;
    } else if (mode != previous) {
        displayChanged8709D1 = 1;
    }
    SetLabels();
}

// Original 0x49b690.
void PauseOption791F30::SetLabels() {
    FngSetLabelString(name2C, 0xa9a76bba);
    switch (resolution870D1C) {
    case 0:
        FngPrintf(data30, "640x480");
        break;
    case 1:
        FngPrintf(data30, "800x600");
        break;
    case 2:
        FngPrintf(data30, "1024x768");
        break;
    case 3:
        FngPrintf(data30, "1280x960");
        break;
    case 4:
        FngPrintf(data30, "1280x1024");
        break;
    case 5:
        FngPrintf(data30, "1600x1200");
        break;
    }
}

// Original 0x49b740.
void PauseOption791F78::React(const char *, unsigned int message) {
    if (bar54.HandleMessage(message)) {
        AnimateArrows(message);
        levelOfDetail87099C = (int)GetValue();
        SetPresetDisplayLevelFromLevelOfDetailAndLimit();
        displayChanged8709D1 = 1;
        resolution98->SetLabels();
        SetLabels();
    }
}

// Original 0x49b7a0.
void PauseOption791F78::SetLabels() {
    FngSetLabelString(name2C, 0xb55e7665);
}

// Original 0x49b7c0.
void PauseOption791F78::InitSlider() {
    bar54.min28 = 1.0f;
    bar54.step38 = 1.0f;
    bar54.max24 = 5.0f;
    bar54.SetValue((float)levelOfDetail87099C);
}

// Original 0x49b930.
void PauseOption792060::React(const char *package, unsigned int message) {
    if (message == kAccept) {
        FngSetScript(package, 0x47ff4e7c, "FORWARD", 1);
    }
}

// Original 0x49b960.
void PauseOption792060::SetLabels() {
    FngSetLabelString(name2C, 0x381fe1e9);
}

// Original 0x49b980.
void PauseOption7920A0::React(const char *package, unsigned int message) {
    if (message == kAccept || message == 0x406415e3) {
        FngPlayMenuSound(package, 1, 0);
        FngShowDialog(package, "GenericDialog_SMALL.fng", 0xf770, 0x61c, 0xd05fc3a3, 0x34dc1bcf, 0x34dc1bcf, 0,
                      0x2bbcca6b);
    }
}

// Original 0x49b9e0.
void PauseOption7920A0::SetLabels() {
    FngSetLabelString(name2C, 0x13f0edeb);
}

// ---- Gameplay -------------------------------------------------------------------------------

// Original 0x49af20.
void PauseOption791D20::React(const char *, unsigned int message) {
    int difficulty = difficulty83AC30;
    if (message == kPadLeft) {
        if (--difficulty < 0) {
            difficulty = 2;
        }
    } else if (message == kPadRight) {
        if (++difficulty > 2) {
            difficulty = 0;
        }
    }
    difficulty83AC30 = difficulty;
    AnimateArrows(message);
    SetLabels();
}

// Original 0x49af70: Easy, Medium, Hard.
void PauseOption791D20::SetLabels() {
    FngSetLabelString(name2C, 0xbc0e6c55);
    unsigned int text = 0;
    switch (difficulty83AC30) {
    case 0:
        text = 0xdbbb4e8f;
        break;
    case 1:
        text = 0xcab262de;
        break;
    case 2:
        text = 0xdbbcf37c;
        break;
    }
    FngSetLabelString(data30, text);
}

// Original 0x49ad40: the jump camera is off at 0 and on at 127; the change is recorded for replays.
void PauseOption791C00::React(const char *, unsigned int message) {
    if (message == kPadRight || message == kPadLeft) {
        jumpCamera83AA20 = jumpCamera83AA20 == 0.0f ? 127.0f : 0.0f;
        if (replay8650B8) {
            replay8650B8->Value578EF0(3, 0, (int)jumpCamera83AA20, true);
        }
    }
    SetLabels();
}

// Original 0x49adc0.
void PauseOption791C00::SetLabels() {
    FngSetLabelString(name2C, 0xf26a5cbf);
    FngSetLabelString(data30, jumpCamera83AA20 == 0.0f ? kOff : kOn);
}

// Original 0x49ae00.
void PauseOption791C48::React(const char *, unsigned int message) {
    if (Horizontal(message)) {
        mirror83AA18 = !mirror83AA18;
    }
    AnimateArrows(message);
    SetLabels();
}

// Original 0x49ae40.
void PauseOption791C48::SetLabels() {
    FngSetLabelString(name2C, 0x85a6ce05);
    FngSetLabelString(data30, mirror83AA18 ? kOn : kOff);
}

// Original 0x49ae80.
void PauseOption791C90::React(const char *, unsigned int message) {
    if (Horizontal(message)) {
        if (units83AA1A == 0) {
            units83AA1A = 1;
        } else if (units83AA1A == 1) {
            units83AA1A = 0;
        }
    }
    AnimateArrows(message);
    SetLabels();
}

// Original 0x49aee0: MPH or KM/H.
void PauseOption791C90::SetLabels() {
    FngSetLabelString(name2C, 0xf86a6272);
    FngSetLabelString(data30, units83AA1A ? 0x8569a25f : 0x8569ab44);
}

// Original 0x49b0b0.
void PauseOption791D68::React(const char *, unsigned int message) {
    int mode = exploreMap83AA1C;
    if (message == kPadLeft) {
        if (--mode < 0) {
            mode = 2;
        }
    } else if (message == kPadRight) {
        if (++mode > 2) {
            mode = 0;
        }
    }
    exploreMap83AA1C = (unsigned char)mode;
    AnimateArrows(message);
    SetLabels();
}

// Map mode labels: Off, Static, Rotate.
static inline unsigned int MapModeLabel(int mode) {
    unsigned int text = 0;
    switch (mode) {
    case 0:
        text = kOff;
        break;
    case 1:
        text = 0xf75595f2;
        break;
    case 2:
        text = 0xf4b00e99;
        break;
    }
    return text;
}

// Original 0x49b100.
void PauseOption791D68::SetLabels() {
    FngSetLabelString(name2C, 0xc6269082);
    FngSetLabelString(data30, MapModeLabel(exploreMap83AA1C));
}

// Original 0x49afe0: the race map has no Rotate setting in game mode 4.
void PauseOption791CD8::React(const char *, unsigned int message) {
    int last = (gameMode850078 != 4) + 1;
    int mode = raceMap83AA1B;
    if (message == kPadLeft) {
        if (--mode < 0) {
            mode = last;
        }
    } else if (message == kPadRight) {
        if (++mode > last) {
            mode = 0;
        }
    }
    raceMap83AA1B = (unsigned char)mode;
    AnimateArrows(message);
    SetLabels();
}

// Original 0x49b040.
void PauseOption791CD8::SetLabels() {
    FngSetLabelString(name2C, 0x9fa5ec9e);
    FngSetLabelString(data30, MapModeLabel(raceMap83AA1B));
}

// Original 0x49b170: Off, Prompt (0x844ad8) and Autosave (both flags); Autosave needs 0x502fe0.
void PauseOption793D68::React(const char *, unsigned int message) {
    if (message == kPadRight) {
        if (autosave844AD9) {
            prompt844AD8 = 1;
            autosave844AD9 = 0;
        } else if (prompt844AD8) {
            prompt844AD8 = 0;
            autosave844AD9 = 0;
        } else if (memoryCard83A9E0.Value502FE0()) {
            prompt844AD8 = 1;
            autosave844AD9 = 1;
        } else {
            prompt844AD8 = 1;
            autosave844AD9 = 0;
        }
    } else if (message == kPadLeft) {
        if (autosave844AD9) {
            prompt844AD8 = 0;
            autosave844AD9 = 0;
        } else if (!prompt844AD8) {
            prompt844AD8 = 1;
            autosave844AD9 = 0;
        } else if (memoryCard83A9E0.Value502FE0()) {
            prompt844AD8 = 1;
            autosave844AD9 = 1;
        } else {
            prompt844AD8 = 0;
            autosave844AD9 = 0;
        }
    }
    AnimateArrows(message);
    SetLabels();
}

// Original 0x49b210.
void PauseOption793D68::SetLabels() {
    FngSetLabelString(name2C, 0x996463e6);
    if (autosave844AD9) {
        FngSetLabelString(data30, 0x1b7abf8e);
    } else {
        FngSetLabelString(data30, prompt844AD8 ? 0x1b7abf8f : 0x1b7abf90);
    }
}

// ---- Player ---------------------------------------------------------------------------------

// Original 0x4c3c70: Auto/Manual; the change is recorded for replays.
void PauseOption795660::React(const char *, unsigned int message) {
    if (Horizontal(message)) {
        int value = playerSettings83AA64[PlayerSlot()].value15;
        switch (value) {
        case 0:
            value = 1;
            break;
        case 1:
            value = 0;
            break;
        }
        playerSettings83AA64[PlayerSlot()].value15 = (unsigned char)value;
        replay8650B8->Value578EF0(1, PlayerSlot(), value, true);
    }
    AnimateArrows(message);
    SetLabels();
}

// Original 0x4c3d80.
void PauseOption795660::SetLabels() {
    FngSetLabelString(name2C, 0xd31407e7);
    unsigned int text = 0;
    switch (playerSettings83AA64[PlayerSlot()].value15) {
    case 0:
        text = 0x61cb0720;
        break;
    case 1:
        text = 0x1b3a2485;
        break;
    }
    FngSetLabelString(data30, text);
}

// Original 0x4c31d0.
void PauseOption7955F0::React(const char *, unsigned int message) {
    if (Horizontal(message)) {
        int value = playerSettings83AA64[PlayerSlot()].value16;
        switch (value) {
        case 0:
            value = 1;
            break;
        case 1:
            value = 0;
            break;
        }
        playerSettings83AA64[PlayerSlot()].value16 = (unsigned char)value;
        replay8650B8->Value578EF0(2, PlayerSlot(), value, true);
    }
    AnimateArrows(message);
    SetLabels();
}

// Original 0x4c32e0: 0 is On.
void PauseOption7955F0::SetLabels() {
    FngSetLabelString(name2C, 0xdf06ed22);
    unsigned int text = 0;
    switch (playerSettings83AA64[PlayerSlot()].value16) {
    case 0:
        text = kOn;
        break;
    case 1:
        text = kOff;
        break;
    }
    FngSetLabelString(data30, text);
}

// Original 0x4c3e10.
void PauseOption7956A8::React(const char *, unsigned int message) {
    if (message == kPadLeft) {
        players8900AC[PlayerSlot()]->Value5F2FA0(-1);
    } else if (message == kPadRight) {
        players8900AC[PlayerSlot()]->Value5F2FA0(1);
    }
    AnimateArrows(message);
    SetLabels();
}

// Original 0x4c3e60: Bumper, Hood, Close, Far, Super Far.
void PauseOption7956A8::SetLabels() {
    FngSetLabelString(name2C, 0xf6ccdc5f);
    unsigned int text = 0;
    switch (playerSettings83AA64[PlayerSlot()].value10) {
    case 0:
        text = 0xc3e9ae58;
        break;
    case 1:
        text = 0x414f19d7;
        break;
    case 2:
        text = 0x5ae3441f;
        break;
    case 3:
        text = 0x1ea4cec2;
        break;
    case 4:
        text = 0x916039b4;
        break;
    }
    FngSetLabelString(data30, text);
}

// Original 0x49b260: the feedback entry belongs to the player captured at construction.
void PauseOption791DB0::React(const char *, unsigned int message) {
    if (Horizontal(message)) {
        playerSettings83AA64[player54].value4 = !playerSettings83AA64[player54].value4;
    }
    AnimateArrows(message);
    SetLabels();
}

// Original 0x49b2b0.
void PauseOption791DB0::SetLabels() {
    FngSetLabelString(name2C, 0x358cdcfd);
    FngSetLabelString(data30, playerSettings83AA64[player54].value4 ? kOn : kOff);
}

// Original 0x4b5f70.
void PauseOption791DB0::Enable() {
    enabled28 = 1;
    FngSetObjectScript(name2C, "UnHighlight", true);
    FngSetObjectScript(data30, "UnHighlight", true);
}

// Original 0x4b5fa0.
void PauseOption791DB0::Disable() {
    enabled28 = 0;
    FngSetObjectScript(name2C, "Disabled", true);
    FngSetObjectScript(data30, "Disabled", true);
}

// On/off settings of the current player: Gauges, Race Position, Lap Info, Score, Leaderboard.
#define UG2_PLAYER_SWITCH(vtable, field, label)                                    \
    void PauseOption##vtable::React(const char *, unsigned int message) {            \
        if (Horizontal(message)) {                                                  \
            playerSettings83AA64[PlayerSlot()].field = !playerSettings83AA64[PlayerSlot()].field; \
        }                                                                            \
        AnimateArrows(message);                                                      \
        SetLabels();                                                                 \
    }                                                                                \
    void PauseOption##vtable::SetLabels() {                                         \
        FngSetLabelString(name2C, label);                                            \
        FngSetLabelString(data30, playerSettings83AA64[PlayerSlot()].field ? kOn : kOff); \
    }

UG2_PLAYER_SWITCH(7956F0, value0, 0xac148579) // 0x4c3f50, 0x4c4010
UG2_PLAYER_SWITCH(795738, value1, 0x82cd8f92) // 0x4c4090, 0x4c4150
UG2_PLAYER_SWITCH(795780, value2, 0x969f96a5) // 0x4c41d0, 0x4c4290
UG2_PLAYER_SWITCH(7957C8, value3, 0xc03f9f19) // 0x4c4310, 0x4c43d0
UG2_PLAYER_SWITCH(795858, value5, 0xc93fbfb5) // 0x4c4600, 0x4c46c0

#undef UG2_PLAYER_SWITCH

// Original 0x4c4450.
void PauseOption795810::React(const char *, unsigned int message) {
    int value = playerSettings83AA64[PlayerSlot()].value14;
    if (message == kPadLeft) {
        if (--value < 0) {
            value = 4;
        }
    } else if (message == kPadRight) {
        if (++value > 4) {
            value = 0;
        }
    }
    playerSettings83AA64[PlayerSlot()].value14 = (unsigned char)value;
    AnimateArrows(message);
    SetLabels();
}

// Original 0x4c4510: Leader, Lap Record, Best Lap, Last Lap, Off.
void PauseOption795810::SetLabels() {
    FngSetLabelString(name2C, 0x084bc378);
    unsigned int text = 0;
    switch (playerSettings83AA64[PlayerSlot()].value14) {
    case 0:
        text = 0xd93f8f55;
        break;
    case 1:
        text = 0xc44d3943;
        break;
    case 2:
        text = 0x17fafc32;
        break;
    case 3:
        text = 0x1ea459f8;
        break;
    case 4:
        text = kOff;
        break;
    }
    FngSetLabelString(data30, text);
}
