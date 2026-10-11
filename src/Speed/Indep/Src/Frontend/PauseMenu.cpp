#include "PauseMenu.h"

extern GameFlow8900D8 *gameFlow8900D8;
extern Timer realTimer8651AC;
extern int gameMode850078;
extern int raceType89E7B0;
extern int value83AA04;
extern PCHelpBarFNGObject *pcHelpBar8384D8;
extern Career85AD40 career85AD40;
extern ProfileData83A9D0 profileData83A9D0;
extern Network89CF48 network89CF48;
extern Replay8650B8 *replay8650B8;
extern int value88F1A8;
extern const char *replayPackage8363E4;
extern unsigned char value850098;
extern unsigned char value8635A8;
extern unsigned char value85BA67;
extern int value8500D0;
extern Player8900AC *players8900AC[2];

// Inline in every original caller.
static inline void RemoveHelpBar() {
    if (FngIsLoaded(FngHelpBarName())) {
        FngRemove(FngHelpBarName());
    }
}

static inline bool FlowState6Value4() {
    return gameFlow8900D8->state0 == 6 && gameFlow8900D8->value24 == 4;
}

static inline bool ReplayAvailable() {
    return replay8650B8->value50EC && replay8650B8->valueB8 > 0 && value88F1A8 == 0;
}

// Original 0x4f4790 (FNGINIT_UI_Pause.fng).
IconScrollerFNGObject *CreatePauseFNGObject(void *data) {
    return new PauseFNGObject(data);
}

// Original 0x4eb410.
PauseFNGObject::PauseFNGObject(void *data) : IconScrollerFNGObject(data) {
    opened150 = realTimer8651AC;
    FngSetControlMask(package4, FngControlMask(gameFlow8900D8->player20));
    value83AA04 = 0;
    Setup();
    if (!FngIsLoaded(FngHelpBarName())) {
        FngAdd(FngHelpBarName(), package4, 0xff, 0);
        pcHelpBar8384D8->Show();
        pcHelpBar8384D8->SyncByMask(value20, package4);
    }
}

// Original 0x4eb510; scalar deleting destructor 0x4eb4f0 is compiler generated.
PauseFNGObject::~PauseFNGObject() {
    RemoveHelpBar();
}

// Original 0x49dfc0 (shared body, also used outside the pause menu).
int PauseFNGObject::UnknownSlot0C(int value4, int, int, int, int value14) {
    return FngFilterSlot0C(value4, value14);
}

// Original 0x4d9300.
void PauseFNGObject::Setup() {
    FngSetLabelString(FngFindObject(package4, FngHash("Header_text")), 0x6c839fbe);
    FngSetLabelString(FngFindObject(package4, FngHash("Back_button")), 0x933dd695);
    FngSetLabelString(FngFindObject(package4, FngHash("Select_button")), 0x8202c364);
    if (raceType89E7B0 == 0) {
        SetupFreeroam();
    } else {
        SetupRace();
    }
    FngSetPackageValue10(package4, 0);
    Scroller()->UnknownSlot20(FngGetPackageValue10(package4));
    Scroller()->UnknownSlot04();
    SelectedIconChanged();
}

// Original 0x4a7bc0.
bool PauseFNGObject::AcceptsInput() {
    return opened150.ticks0 != realTimer8651AC.ticks0 && ControllerAssigned(gameFlow8900D8->player20) != 0;
}

// Original 0x4a7bf0.
bool PauseFNGObject::OptionsAvailable() {
    for (Player8900AC **slot = players8900AC; slot < players8900AC + 2; ++slot) {
        Player8900AC *player = *slot;
        if (player && player->player8 == gameFlow8900D8->player20) {
            return player->value50 == 0 || raceType89E7B0 == 0;
        }
    }
    return false;
}

// Original 0x4a7c70.
void PauseFNGObject::QuitToCrib() {
    career85AD40.Value513B30();
    FngSetReturnPackage("UI_CareerCrib.fng");
    gameFlow8900D8->Queue8(6);
    RemoveHelpBar();
}

// Original 0x4a7cc0.
void PauseFNGObject::QuitToCribKeepHelpBar() {
    career85AD40.Value513B30();
    FngSetReturnPackage("UI_CareerCrib.fng");
    gameFlow8900D8->Queue8(6);
}

// Original 0x4a7cf0.
void PauseFNGObject::QuitToProfile() {
    FngSetReturnPackage(profileData83A9D0.Value502FF0(0));
    FngSetPackageValue10(package4, 0);
    gameFlow8900D8->Queue8(6);
    if (gameMode850078 == 1) {
        career85AD40.Value5139C0();
    }
    RemoveHelpBar();
}

// Original 0x4c2170.
void PauseFNGObject::MessageHandler(unsigned int message, int value8, int valueC, int value10) {
    if (AcceptsInput()) {
        IconScrollerFNGObject::MessageHandler(message, value8, valueC, value10);
    }
    switch (message) {
    case 0x451e768e:
        gameFlow8900D8->Queue(0xb, 0);
        RemoveHelpBar();
        break;
    case 0x30f32a49:
        gameFlow8900D8->Queue8(4);
        break;
    case 0x0c407210:
        if (AcceptsInput() && !FngScriptActive(package4, 0x47ff4e7c, "FORWARD")) {
            FngSetScript(package4, 0x47ff4e7c, "FORWARD", 1);
            RemoveHelpBar();
        }
        break;
    case 0x13999fc2:
    case 0x4a05ae7f:
        FngSendMessage(0x251bf83e, package4);
        break;
    case 0x30ed2368:
        QuitToProfile();
        break;
    case 0x33c42e03:
        QuitToCribKeepHelpBar();
        break;
    case 0x43da9fd0:
        QuitToCrib();
        break;
    case 0xb4623f67:
        FngSendMessage(0x251bf83e, package4);
        if (!FngIsLoaded(FngHelpBarName())) {
            FngAdd(FngHelpBarName(), package4, 0xff, 0);
        }
        pcHelpBar8384D8->Show();
        pcHelpBar8384D8->SyncByMask(value20, package4);
        break;
    case 0x911ab364:
    case 0xb5af2461:
        if (AcceptsInput() && !FngScriptActive(package4, 0x47ff4e7c, "FORWARD")) {
            RemoveHelpBar();
            StopInput();
            Value50DA20(message, value8, valueC, value10);
            FngSetScript(package4, 0x47ff4e7c, "FORWARD", 1);
            FngSetPackageValue10(package4, 0);
        }
        break;
    case 0xe1a57d51:
        gameFlow8900D8->Queue8(3);
        break;
    case 0xe1fde1d1:
        switch (value13C) {
        case 0x30ed2368:
            QuitToProfile();
            break;
        case 0x30f32a49:
            gameFlow8900D8->Queue8(4);
            break;
        case 0x43da9fd0:
            QuitToCrib();
            break;
        case 0x911ab364:
        case 0xb5af2461:
            gameFlow8900D8->Queue8(1);
            break;
        case 0xe1a57d51:
            gameFlow8900D8->Queue8(3);
            break;
        case 0x13999fc2:
        case 0x4a05ae7f:
            break;
        }
        break;
    }
}

// Original 0x4c2900, used when 0x89e7b0 is zero.
void PauseFNGObject::SetupFreeroam() {
    if (gameMode850078 == 1) {
        if (raceType89E7B0 != 7) {
            AddIcon(new PauseIcon7954D8(0x62aa0df6, 0xc3e5bcdc, 0));
        }
    } else {
        AddIcon(new PauseIcon7954D8(0x62aa0df6, 0xc44a6515, 0));
    }
    if (gameMode850078 == 1 && value8635A8) {
        AddIcon(new PauseIcon795518(0xb8038ac0, 0x453890e1, 0));
    }
    if (OptionsAvailable()) {
        AddIcon(new PauseIcon795510(0x520de4e3, 0xb34993c9, 0));
    }
    if (gameMode850078 != 4 && gameMode850078 != 8 && gameMode850078 != 0x200) {
        AddIcon(new PauseIcon7954F0(0x84d1d864, 0x7840428a, 0));
    }
    if (gameMode850078 == 8) {
        AddIcon(new PauseIcon795528(0x4c9e34e6, 0xff956af2, 0));
    } else if (gameMode850078 == 0x200) {
        AddIcon(new PauseIcon795528(0x4c9e34e6, 0xf39723a1, 0));
    } else {
        AddIcon(new PauseIcon795528(0x4c9e34e6, 0xc7e1420c, 0));
    }
}

// Original 0x4c2bd0, used when 0x89e7b0 is non-zero.
void PauseFNGObject::SetupRace() {
    if (value8500D0 < 3) {
        if (gameMode850078 == 1 && value85BA67) {
            AddIcon(new PauseIcon7954E0(0x3109913c, 0x5e670a9e, 0));
        }
        AddIcon(new PauseIcon7954D0(0x12bb5ea2, 0xf6601088, 0));
        if (raceType89E7B0 != 7 && gameMode850078 != 8 && gameMode850078 != 0x200) {
            AddIcon(new PauseIcon7954E8(0xb295a6b6, 0x80833fc2, 0));
        }
        if (gameMode850078 == 1 && raceType89E7B0 != 7) {
            AddIcon(new PauseIcon795520(0x62aa0df6, 0xdb4e3c0d, 0));
        }
        if (OptionsAvailable()) {
            AddIcon(new PauseIcon795510(0x520de4e3, 0xb34993c9, 0));
        }
        if (gameMode850078 != 4 && gameMode850078 != 8 && gameMode850078 != 0x200) {
            AddIcon(new PauseIcon7954F0(0x84d1d864, 0x7840428a, 0));
        }
        if (gameMode850078 == 8) {
            AddIcon(new PauseIcon795528(0x4c9e34e6, 0xff956af2, 0));
        } else if (gameMode850078 == 0x200) {
            AddIcon(new PauseIcon795528(0x4c9e34e6, 0xf39723a1, 0));
        } else {
            AddIcon(new PauseIcon795528(0x4c9e34e6, 0xc7e1420c, 0));
        }
    } else if (gameMode850078 == 8) {
        AddIcon(new PauseIcon795528(0x4c9e34e6, 0xff956af2, 0));
    } else if (gameMode850078 == 0x200) {
        AddIcon(new PauseIcon795528(0x4c9e34e6, 0xf39723a1, 0));
    } else {
        AddIcon(new PauseIcon795528(0x4c9e34e6, 0xc7e1420c, 0));
    }
}

// Original 0x4c2640.
PauseIcon7954F0::PauseIcon7954F0(unsigned int a, unsigned int b, unsigned int c) : IconScrollerIcon(a, b, c) {
    value20 = !ReplayAvailable();
}

// Original 0x4c27f0.
PauseIcon795528::PauseIcon795528(unsigned int a, unsigned int b, unsigned int c) : IconScrollerIcon(a, b, c) {
    value45 = 1;
}

// Original 0x4c24c0.
void PauseIcon7954D0::React(const char *package, unsigned int message, int, int, int) {
    if (message == 0x0c407210) {
        gameFlow8900D8->Queue8(1);
        FngSetPackageValue10(package, 0);
        RemoveHelpBar();
    }
}

// Original 0x4c2510.
void PauseIcon7954D8::React(const char *package, unsigned int message, int, int, int) {
    if (message == 0x0c407210) {
        gameFlow8900D8->Queue8(2);
        FngSetPackageValue10(package, 0);
        RemoveHelpBar();
    }
}

// Original 0x4c2560.
void PauseIcon7954E0::React(const char *package, unsigned int message, int, int, int) {
    if (message == 0x0c407210) {
        FngShowDialog(package, "UI_InGameDialog.fng", 0x639, 0x541389e5, 0x33c42e03, 0xb4623f67, 0xb4623f67, 1,
                      FlowState6Value4() ? 0xe761c8b8 : 0x33c42e03);
        FngSendMessage(0x96dda9e6, package);
    }
}

// Original 0x4c25d0; the flow test precedes the message test.
void PauseIcon7954E8::React(const char *package, unsigned int message, int, int, int) {
    bool flow = FlowState6Value4();
    if (message == 0x0c407210) {
        FngShowDialog(package, "UI_InGameDialog.fng", 0x639, 0x541389e5, 0xe1a57d51, 0xb4623f67, 0xb4623f67, 1,
                      flow ? 0xaa50aa02 : 0x0972a44b);
        FngSendMessage(0x96dda9e6, package);
    }
}

// Original 0x4c26b0.
void PauseIcon7954F0::React(const char *package, unsigned int message, int, int, int) {
    if (message == 0x0c407210 && ReplayAvailable()) {
        FngShow(package, "ui_ReplayControl.fng", 0);
        replayPackage8363E4 = package;
    }
}

// Original 0x4c2700.
void PauseIcon795510::React(const char *package, unsigned int message, int, int, int) {
    if (message == 0x0c407210) {
        FngShow(package, "UI_PauseOptionsMain.fng", 0);
    }
}

// Original 0x4c2730.
void PauseIcon795518::React(const char *package, unsigned int message, int, int, int) {
    if (message == 0x0c407210) {
        FngShowDialog(package, "UI_InGameDialog.fng", 0x639, 0x541389e5, 0x43da9fd0, 0xb4623f67, 0xb4623f67, 1,
                      0xc9ba9e3e);
        FngSendMessage(0x96dda9e6, package);
    }
}

// Original 0x4c2780.
void PauseIcon795520::React(const char *package, unsigned int message, int, int, int) {
    if (message == 0x0c407210) {
        FngShowDialog(package, "UI_InGameDialog.fng", 0x639, 0x541389e5, 0x451e768e, 0xb4623f67, 0xb4623f67, 1,
                      FlowState6Value4() ? 0x07cd17b0 : 0x60b42afb);
        FngSendMessage(0x96dda9e6, package);
    }
}

// Original 0x4c2820.
void PauseIcon795528::React(const char *package, unsigned int message, int, int, int) {
    if (message == 0x0c407210) {
        unsigned int text;
        if (gameMode850078 == 8) {
            if (network89CF48.Value5F0120()) {
                text = value850098 ? 0x79a85513 : 0x555bd5b6;
            } else {
                text = value850098 ? 0xbc25c6b1 : 0xce50da14;
            }
        } else if (gameMode850078 == 0x200) {
            text = network89CF48.Value5F0120() ? 0x555bd5b6 : 0xd63b6982;
        } else if (FlowState6Value4()) {
            text = 0xe15b558a;
        } else {
            text = gameMode850078 != 1 ? 0x30ed2368 : 0x6f4796a6;
        }
        FngShowDialog(package, "UI_InGameDialog.fng", 0x639, 0x541389e5, 0x30ed2368, 0xb4623f67, 0xb4623f67, 1, text);
        FngSendMessage(0x96dda9e6, package);
    }
}
