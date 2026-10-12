#ifndef UG2_PAUSE_MENU_H
#define UG2_PAUSE_MENU_H

#include "../Physics/Ug2Layout.h"

// FEng/game-flow interfaces at the original call boundaries (all cdecl).
unsigned int FngHash(const char *name); // 0x505450
void *FngFindObject(const char *package, unsigned int hash); // 0x537a10
void FngSetLabelString(void *object, unsigned int hash); // 0x50c900
void FngSetPackageValue10(const char *package, int value); // 0x50b8c0
int FngGetPackageValue10(const char *package); // 0x50b8a0
const char *FngHelpBarName(); // 0x50cea0
bool FngIsLoaded(const char *package); // 0x52cf60
void FngRemove(const char *package); // 0x5379a0
void FngAdd(const char *package, const char *parent, int value8, int valueC); // 0x555f10
void FngShow(const char *package, const char *name, int value8); // 0x537980
void FngSetControlMask(const char *package, unsigned int mask); // 0x537950
unsigned int FngControlMask(int player); // 0x50d2d0
bool FngScriptActive(const char *package, unsigned int object, const char *script); // 0x537cc0
void FngSetScript(const char *package, unsigned int object, const char *script, int restart); // 0x537c00
void FngSendMessage(unsigned int message, const char *package); // 0x55dda0
void FngShowDialog(const char *package, const char *dialog, int value8, unsigned int valueC,
                   unsigned int value10, unsigned int value14, unsigned int value18,
                   int value1C, unsigned int value20); // 0x558790
void FngSetReturnPackage(const char *package); // 0x50c8d0
int ControllerAssigned(int player); // 0x571a60, int result (callers test EAX)
int FngFilterSlot0C(int value4, int value14); // 0x50d1d0

// Game flow at *0x8900d8; only observed fields are named.
struct GameFlow8900D8 {
    int state0;
    char unidentified4[0x1c];
    int player20;
    int value24;
    void Queue(int type, int value); // 0x5f41e0
    int Query5EA190(); // 0x5ea190
    void Queue8(int value); // 0x5f4240
};
UG2_CHECK_OFFSET(GameFlow8900D8, player20, 0x20);
UG2_CHECK_OFFSET(GameFlow8900D8, value24, 0x24);

struct PCHelpBarFNGObject {
    void Show(); // 0x54e6a0
    void SyncByMask(int value, const char *package); // 0x54e6e0
};
struct Career85AD40 {
    void Value513B30(); // 0x513b30
    void Value5139C0(); // 0x5139c0
};
struct ProfileData83A9D0 {
    const char *Value502FF0(int value); // 0x502ff0
    bool Query457BA0(); // 0x457ba0
    char unidentified0[0xe4];
    int valueE4;
    char unidentifiedE8[0x156a8 - 0xe8];
    int value156A8;
};
UG2_CHECK_OFFSET(ProfileData83A9D0, value156A8, 0x156a8);
struct Network89CF48 {
    bool Value5F0120(); // 0x5f0120
};
// Object at *0x8650b8; only the fields read by the replay item are named.
struct Replay8650B8 {
    void Value578EF0(int type, int player, int value, bool record); // 0x578ef0
    void Overflow571830(); // 0x571830, event table full
    char unidentified0[0x78];
    int value78;          // -1: not recording
    char unidentified7C[4];
    int value80;          // nonzero: playing back
    char unidentified84[8];
    int value8C;
    char unidentified90[0x28];
    int valueB8;
    char unidentifiedBC[0x40e4 - 0xbc];
    int eventCount40E4;
    char unidentified40E8[4];
    struct ReplayEvent {
        int type0;
        int frame4;
        int player8;
        int valueC;
    } events40EC[0x100];
    unsigned char value50EC;
};
UG2_CHECK_OFFSET(Replay8650B8, valueB8, 0xb8);
UG2_CHECK_OFFSET(Replay8650B8, value50EC, 0x50ec);
// Entries of the two-pointer table at 0x8900ac.
struct Player8900AC {
    int Value5F2FA0(int step); // 0x5f2fa0
    char unidentified0[8];
    int player8;
    char unidentifiedC[0x44];
    int value50;
};
UG2_CHECK_OFFSET(Player8900AC, value50, 0x50);

// IconScrollerIcon, 0x48 bytes. The constructor is retail 0x51f670; the root
// destructor 0x527770 restores vtable 0x79afdc. Items' scalar deleting
// destructors fold to 0x4aeb70.
class IconScrollerIcon {
public:
    IconScrollerIcon(unsigned int value0C, unsigned int value2C, unsigned int value30); // 0x51f670
    virtual ~IconScrollerIcon(); // 0x527770
    virtual void React(const char *package, unsigned int message, int value8, int valueC, int value10) = 0;
    char unidentified4[8];
    unsigned int valueC;
    char unidentified10[0x10];
    unsigned char value20;
    char unidentified21[0xb];
    unsigned int value2C;
    unsigned int value30;
    char unidentified34[0x11];
    unsigned char value45;
    char unidentified46[2];
};
UG2_CHECK_OFFSET(IconScrollerIcon, valueC, 0xc);
UG2_CHECK_OFFSET(IconScrollerIcon, value20, 0x20);
UG2_CHECK_OFFSET(IconScrollerIcon, value2C, 0x2c);
UG2_CHECK_OFFSET(IconScrollerIcon, value45, 0x45);
UG2_CHECK_SIZE(IconScrollerIcon, 0x48);

// Pause option items. Names keep their retail vtables; only React is overridden.
class PauseIcon7954D0 : public IconScrollerIcon { // React 0x4c24c0
public:
    PauseIcon7954D0(unsigned int a, unsigned int b, unsigned int c) : IconScrollerIcon(a, b, c) { value45 = 1; }
    void React(const char *package, unsigned int message, int value8, int valueC, int value10);
};
class PauseIcon7954D8 : public IconScrollerIcon { // React 0x4c2510
public:
    PauseIcon7954D8(unsigned int a, unsigned int b, unsigned int c) : IconScrollerIcon(a, b, c) {}
    void React(const char *package, unsigned int message, int value8, int valueC, int value10);
};
class PauseIcon7954E0 : public IconScrollerIcon { // React 0x4c2560
public:
    PauseIcon7954E0(unsigned int a, unsigned int b, unsigned int c) : IconScrollerIcon(a, b, c) { value45 = 1; }
    void React(const char *package, unsigned int message, int value8, int valueC, int value10);
};
class PauseIcon7954E8 : public IconScrollerIcon { // React 0x4c25d0
public:
    PauseIcon7954E8(unsigned int a, unsigned int b, unsigned int c) : IconScrollerIcon(a, b, c) { value45 = 1; }
    void React(const char *package, unsigned int message, int value8, int valueC, int value10);
};
class PauseIcon7954F0 : public IconScrollerIcon { // React 0x4c26b0
public:
    PauseIcon7954F0(unsigned int a, unsigned int b, unsigned int c); // 0x4c2640
    void React(const char *package, unsigned int message, int value8, int valueC, int value10);
};
class PauseIcon795510 : public IconScrollerIcon { // React 0x4c2700
public:
    PauseIcon795510(unsigned int a, unsigned int b, unsigned int c) : IconScrollerIcon(a, b, c) {}
    void React(const char *package, unsigned int message, int value8, int valueC, int value10);
};
class PauseIcon795518 : public IconScrollerIcon { // React 0x4c2730
public:
    PauseIcon795518(unsigned int a, unsigned int b, unsigned int c) : IconScrollerIcon(a, b, c) { value45 = 1; }
    void React(const char *package, unsigned int message, int value8, int valueC, int value10);
};
class PauseIcon795520 : public IconScrollerIcon { // React 0x4c2780
public:
    PauseIcon795520(unsigned int a, unsigned int b, unsigned int c) : IconScrollerIcon(a, b, c) { value45 = 1; }
    void React(const char *package, unsigned int message, int value8, int valueC, int value10);
};
class PauseIcon795528 : public IconScrollerIcon { // React 0x4c2820
public:
    PauseIcon795528(unsigned int a, unsigned int b, unsigned int c); // 0x4c27f0
    void React(const char *package, unsigned int message, int value8, int valueC, int value10);
};

// Polymorphic scroller embedded at IconScrollerFNGObject +0x4c (base vtable
// 0x795cbc). The pause menu dispatches slots +0x04 and +0x20 through it.
class IconScroller4C {
public:
    virtual void UnknownSlot00() = 0;
    virtual void UnknownSlot04() = 0;
    virtual void UnknownSlot08() = 0;
    virtual void UnknownSlot0C() = 0;
    virtual void UnknownSlot10() = 0;
    virtual void UnknownSlot14() = 0;
    virtual void UnknownSlot18() = 0;
    virtual void UnknownSlot1C() = 0;
    virtual void UnknownSlot20(int value) = 0;
};

// IconScrollerFNGObject: constructor 0x552330, destructor 0x4defb0, vtable 0x7974d4.
class IconScrollerFNGObject {
public:
    IconScrollerFNGObject(void *data); // 0x552330
    virtual ~IconScrollerFNGObject(); // 0x4defb0, scalar 0x4df010
    virtual int UnknownSlot04(int value); // 0x40d080
    virtual void MessageHandler(unsigned int message, int value8, int valueC, int value10); // 0x543d40
    virtual int UnknownSlot0C(int value4, int value8, int valueC, int value10, int value14); // 0x570a70
    virtual void SelectedIconChanged(); // 0x50da50
    virtual void Setup() = 0;
    virtual void AddIcon(IconScrollerIcon *icon); // 0x50dab0
    void StopInput(); // 0x52fbf0
    void Value50DA20(unsigned int message, int value8, int valueC, int value10); // 0x50da20
    IconScroller4C *Scroller() { return reinterpret_cast<IconScroller4C *>(scroller4C); }

    const char *package4;
    char unidentified8[0x18];
    int value20;
    char unidentified24[0x28];
    char scroller4C[0x13c - 0x4c]; // embedded scroller; extent unidentified
    unsigned int value13C;
    char unidentified140[0x10];
};
UG2_CHECK_OFFSET(IconScrollerFNGObject, package4, 4);
UG2_CHECK_OFFSET(IconScrollerFNGObject, value20, 0x20);
UG2_CHECK_OFFSET(IconScrollerFNGObject, scroller4C, 0x4c);
UG2_CHECK_OFFSET(IconScrollerFNGObject, value13C, 0x13c);
UG2_CHECK_SIZE(IconScrollerFNGObject, 0x150);

// Timer value; the pause constructor zeroes +0x150 before copying 0x8651ac.
struct Timer {
    unsigned int ticks0;
    Timer() : ticks0(0) {}
};

// PauseFNGObject (UI_Pause.fng): constructor 0x4eb410, vtable 0x797cb0, 0x158 bytes.
class PauseFNGObject : public IconScrollerFNGObject {
public:
    PauseFNGObject(void *data); // 0x4eb410
    ~PauseFNGObject(); // 0x4eb510, scalar 0x4eb4f0
    void MessageHandler(unsigned int message, int value8, int valueC, int value10); // 0x4c2170
    int UnknownSlot0C(int value4, int value8, int valueC, int value10, int value14); // 0x49dfc0
    void Setup(); // 0x4d9300
    bool AcceptsInput(); // 0x4a7bc0
    bool OptionsAvailable(); // 0x4a7bf0
    void QuitToCrib(); // 0x4a7c70
    void QuitToCribKeepHelpBar(); // 0x4a7cc0
    void QuitToProfile(); // 0x4a7cf0
    void SetupFreeroam(); // 0x4c2900
    void SetupRace(); // 0x4c2bd0

    Timer opened150;
    char unidentified154[4];
};
UG2_CHECK_OFFSET(PauseFNGObject, opened150, 0x150);
UG2_CHECK_SIZE(PauseFNGObject, 0x158);

IconScrollerFNGObject *CreatePauseFNGObject(void *data); // 0x4f4790

#endif
