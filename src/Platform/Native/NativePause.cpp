// Platform scaffolding for the recovered pause menu and in-game options. Nothing in this file is
// original code: it supplies the remaining FEng, IconScroller, dialog and game-flow interfaces
// that Speed/Indep/Src/Frontend/PauseMenu.cpp and PauseOptions.cpp call, at the original call
// boundaries. Each seam names the retail routine it stands in for; behaviour taken from a read of
// that routine is marked as such and is not verified.
#include "NativePause.h"
#include "NativeAssets.h"
#include "NativeFrontend.h"
#include "NativeControls.h"
#include "NativeHud.h"
#include "../../Speed/Indep/Src/Frontend/FEngPackage.h"
#include "../../Speed/Indep/Src/Frontend/PauseOptions.h"
#include "../../Speed/Indep/Libs/bWare/Compression.h"

#include <map>
#include <new>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void NativeTrap(const char *what);

extern GameFlow8900D8 *gameFlow8900D8;
extern PCHelpBarFNGObject *pcHelpBar8384D8;
extern Replay8650B8 *replay8650B8;
extern int realClock8651AC;

namespace {

const unsigned int kAccept = 0x0c407210;      // accept, also the item React message
const unsigned int kPadAccept = 0x406415e3;   // PAD_ACCEPT (package-level accept in 0x54d4b0)
const unsigned int kPadBack = 0x911ab364;     // PAD_BACK
const unsigned int kPadLeft = 0x9120409e;     // PAD_LEFT
const unsigned int kPadRight = 0xb5971bf1;    // PAD_RIGHT
const unsigned int kPadUp = 0x72619778;       // PAD_UP
const unsigned int kPadStart = 0xb5af2461;   // PAD_START
const unsigned int kPadDown = 0x911c0a4b;     // PAD_DOWN
const unsigned int kExitComplete = 0xe1fde1d1; // EXIT_COMPLETE
const unsigned int kStopDim = 0x251bf83e;     // 0x543d40 -> 0x538ca0
const unsigned int kFrontendTick = 0xc98356ba; // sent to every FNG object each frame (0x5663b0)

// Message targets of the FEng queue (0x51aa20 / 0x541fc0).
const int kTargetGame = -1;     // the screen's FNG object handler (owner slot +0x18)
const int kTargetPackage = -3;  // package-level responses
const int kTargetInput = -5;    // owner slot +0x1c, not modelled
const int kTargetObject = 1;    // one object's responses

unsigned int Rd32(const unsigned char *p) { return p[0] | p[1] << 8 | p[2] << 16 | (unsigned int)p[3] << 24; }

bool Debug() { return getenv("UG2_DEBUG_PAUSE") != 0; }

typedef void *(*ScreenFactory)(void *data);
void *CreatePause(void *data) { return CreatePauseFNGObject(data); }
void *CreateOptionsMain(void *data) { return CreatePauseOptionsMainFNGObject(data); }
void *CreateOptions(void *data) { return CreatePauseOptionsFNGObject(data); }

struct NativeScreen {
    NativeScreen(const char *packageName, ScreenFactory create, bool list)
        : name(packageName), factory(create), optionsList(list), package(0), loaded(false), shown(false),
          object(0) {
        memset(&fePackage, 0, sizeof(fePackage));
        memset(&record, 0, sizeof(record));
        memset(&node, 0, sizeof(node));
        node.package18 = &fePackage;
    }
    // Every showing starts from the package's loaded state, as a newly pushed FEng package does.
    bool Reset(std::string *error) {
        delete package;
        package = new FengPackage;
        currentScript.clear();
        memset(&fePackage, 0, sizeof(fePackage));
        focusObjects.clear();
        if (!package->Load(&data[0], data.size(), 0, error)) return false;
        const std::vector<FengObject *> &objects = package->Objects();
        for (size_t i = 0; i < objects.size(); ++i) {
            if (objects[i]->flags1C & 0x10000000) focusObjects.push_back(objects[i]);
        }
        fePackage.focusObjects90 = focusObjects.empty() ? 0 : &focusObjects[0];
        fePackage.focusCount94 = focusObjects.size();
        return true;
    }
    void Deliver(unsigned int message, int value8, int valueC) {
        if (!object) return;
        if (!strcmp(name, "UI_Options_PC_Controller.fng")) return;
        if (optionsList) static_cast<OptionsScrollerFNGObject *>(object)->MessageHandler(message, value8, valueC, 0);
        else static_cast<IconScrollerFNGObject *>(object)->MessageHandler(message, value8, valueC, 0);
    }
    void DestroyObject() {
        if (!object) return;
        void *doomed = object;
        object = 0;
        if (!strcmp(name, "UI_Options_PC_Controller.fng")) DestroyNativeControls(doomed);
        else if (optionsList) delete static_cast<OptionsScrollerFNGObject *>(doomed);
        else delete static_cast<IconScrollerFNGObject *>(doomed);
    }
    const char *name;
    ScreenFactory factory;
    bool optionsList;
    std::vector<unsigned char> data;  // 0x30203 chunk
    FengPackage *package;
    bool loaded;
    bool shown;
    FEPackage fePackage;  // original prefix, native object loading remains a seam
    FngRecord record;      // static FNG selection survives package reloads
    FEngPackageNode node;
    std::vector<HudVisibilityObject *> focusObjects;
    std::map<FengObject *, unsigned int> currentScript;  // object +0x54 script hash
    void *object;          // FNG object created by the package's FNGINIT factory
};

struct RunningScript {
    NativeScreen *screen;
    FengObject *object;
    const FengScript *script;
    float milliseconds;
    size_t nextEvent;
};

struct PendingMessage {
    NativeScreen *screen;
    unsigned int message;
    FengObject *object;   // value8 delivered to the game, or the sender of a response command
    int target;
    FengObject *targetObject;
};

// The in-game dialog 0x558790 is not recovered. Button order, labels and result messages follow
// its arguments; reading value1C as the initially highlighted button is an interpretation. The
// "" and GenericDialog_SMALL.fng dialogs of the options screens use the same package.
struct NativeDialog {
    bool shown;
    NativeScreen *parent;
    unsigned int results[2];
    unsigned int backResult;
    int selected;
};

NativeScreen pauseScreen("UI_Pause.fng", CreatePause, false);
NativeScreen optionsMainScreen("UI_PauseOptionsMain.fng", CreateOptionsMain, false);
NativeScreen optionsScreen("UI_PauseOptions.fng", CreateOptions, true);
NativeScreen controlsScreen("UI_Options_PC_Controller.fng", CreateNativeControls, false);
NativeScreen dialogScreen("UI_InGameDialog.fng", 0, false);
NativeScreen *const screens[] = {&pauseScreen, &optionsMainScreen, &optionsScreen, &dialogScreen, &controlsScreen};
const size_t kScreenCount = sizeof(screens) / sizeof(screens[0]);
NativeDialog dialog;
NativeTextureCache *textures = 0;
NativeFont font;
std::vector<unsigned char> language;
std::vector<RunningScript> scripts;
std::vector<PendingMessage> messages;
// FngShow / FngRemove run after the current delivery: the caller keeps using its object.
NativeScreen *switchFrom = 0;
NativeScreen *switchTo = 0;
NativeScreen *removeScreen = 0;
NativePauseRequest request = kPauseRequestNone;
bool helpBarLoaded = false;
const char *returnPackage = 0;
float clockRemainder = 0.0f;
GameFlow8900D8 nativeFlow;
PCHelpBarFNGObject nativeHelpBar;
Replay8650B8 nativeReplay;
FEngQueue nativeFengQueue;
FEngManager nativeFengManager = {{0}, &nativeFengQueue};

NativeScreen *FindScreen(const char *name) {
    if (!name) return 0;
    unsigned int hash = FengHash(name);
    for (size_t i = 0; i < kScreenCount; ++i) {
        if (screens[i]->loaded && hash == FengHash(screens[i]->name)) return screens[i];
    }
    return 0;
}

NativeScreen *ScreenOfObject(FengObject *object) {
    for (size_t i = 0; i < kScreenCount; ++i) {
        NativeScreen *screen = screens[i];
        if (!screen->shown) continue;
        std::vector<FengObject *> &objects = screen->package->Objects();
        for (size_t k = 0; k < objects.size(); ++k) {
            if (objects[k] == object) return screen;
        }
    }
    return 0;
}

std::string Localised(unsigned int hash) {
    return LookupNativeString(language, hash);  // '^' line breaks are laid out by NativeFont (0x51c110)
}

// Package loading seam: UI_*.fng chunks 0x30210 hold the name hash and a JDLZ block that
// unpacks to the 'FEn\xe7' package body of a 0x30203 chunk.
bool LoadCompressedPackage(const std::vector<unsigned char> &bundle, NativeScreen *screen, std::string *error) {
    unsigned int wanted = FengHash(screen->name);
    for (size_t offset = 0; offset + 8 <= bundle.size();) {
        unsigned int id = Rd32(&bundle[offset]);
        unsigned int length = Rd32(&bundle[offset + 4]);
        if (length > bundle.size() - offset - 8) break;
        if (id == 0x30210 && length > 0x14 && Rd32(&bundle[offset + 8]) == wanted) {
            const unsigned char *block = &bundle[offset + 12];
            unsigned int size = Rd32(block + 8);
            std::vector<unsigned char> source(block, block + length - 4);
            source.resize(source.size() + 64);
            std::vector<unsigned char> package(8 + size + 64);
            if (bDecompress(reinterpret_cast<unsigned int *>(&source[0]), &package[8]) != (int)size) {
                *error = "cannot unpack " + std::string(screen->name);
                return false;
            }
            package[0] = 0x03; package[1] = 0x02; package[2] = 0x03; package[3] = 0;
            package[4] = size & 0xff; package[5] = size >> 8 & 0xff;
            package[6] = size >> 16 & 0xff; package[7] = size >> 24;
            package.resize(8 + size);
            screen->data.swap(package);
            if (!screen->Reset(error)) return false;
            screen->loaded = true;
            return true;
        }
        offset += 8 + length;
    }
    *error = std::string(screen->name) + " not found";
    return false;
}

// Plain 0x30203 packages: the PkHd name string sits at chunk +0x30.
bool LoadPlainPackage(const std::vector<unsigned char> &bundle, NativeScreen *screen, std::string *error) {
    size_t nameLength = strlen(screen->name) + 1;
    for (size_t offset = 0; offset + 8 <= bundle.size();) {
        unsigned int id = Rd32(&bundle[offset]);
        unsigned int length = Rd32(&bundle[offset + 4]);
        if (length > bundle.size() - offset - 8) break;
        if (id == 0x30203 && length > 0x30 + nameLength && !memcmp(&bundle[offset + 0x30], screen->name, nameLength)) {
            screen->data.assign(bundle.begin() + offset, bundle.begin() + offset + 8 + length);
            if (!screen->Reset(error)) return false;
            screen->loaded = true;
            return true;
        }
        offset += 8 + length;
    }
    *error = std::string(screen->name) + " not found";
    return false;
}

void Post(NativeScreen *screen, unsigned int message, FengObject *object, int target, FengObject *targetObject = 0) {
    PendingMessage pending = {screen, message, object, target, targetObject};
    messages.push_back(pending);
}

void RemoveScripts(NativeScreen *screen) {
    for (size_t i = 0; i < scripts.size();) {
        if (scripts[i].screen == screen) scripts.erase(scripts.begin() + i);
        else ++i;
    }
}

// 0x5178b0 with the object's script list: the object's current script, its tracks (FengPackage)
// and its package events (EXIT_COMPLETE).
void PlayScript(NativeScreen *screen, FengObject *object, unsigned int hash, bool restart) {
    if (!screen || !object) return;
    for (size_t i = 0; i < object->scripts.size(); ++i) {
        if (object->scripts[i].hash != hash) continue;
        for (size_t k = 0; k < scripts.size(); ++k) {
            if (scripts[k].object == object) {
                if (!restart && scripts[k].script == &object->scripts[i]) return;
                scripts.erase(scripts.begin() + k);
                break;
            }
        }
        screen->currentScript[object] = hash;
        FengPackage::PlayScriptHash(object, hash, restart);
        RunningScript running = {screen, object, &object->scripts[i], 0.0f, 0};
        scripts.push_back(running);
        return;
    }
}

// 0x504490: from the conditional at `index`, the command that closes it (or its else at depth 1).
size_t SkipBlock(const std::vector<FengResponseCommand> &commands, size_t index) {
    size_t count = commands.size();
    if (index == count - 1) return count;
    int depth = 1;
    do {
        ++index;
        unsigned int type = commands[index].type;
        if (type == 0x500) {
            if (depth == 1) depth = 0;
        } else if (type == 0x300 || type == 0x301) {
            ++depth;
        } else if (type == 0x501) {
            --depth;
        }
    } while (index < count - 1 && depth != 0);
    return index;
}

// 0x52c7c0, the commands the options and pause packages use.
void RunResponse(NativeScreen *screen, const FengResponse &response, FengObject *object) {
    const std::vector<FengResponseCommand> &commands = response.commands;
    for (size_t i = 0; i < commands.size(); ++i) {
        const FengResponseCommand &command = commands[i];
        unsigned int current = object && screen->currentScript.count(object) ? screen->currentScript[object] : 0;
        switch (command.type) {
        case 0:
            PlayScript(screen, object, command.parameter, true);
            break;
        case 1: {
            FengObject *target = screen->package->FindGuid(command.target);
            if (target) Post(screen, command.parameter, object, kTargetObject, target);
            break;
        }
        case 2:
            Post(screen, command.parameter, object, kTargetGame);
            break;
        case 3:
            Post(screen, command.parameter, object, kTargetInput);
            break;
        case 0x300:
            if (current != command.parameter) i = SkipBlock(commands, i);
            break;
        case 0x301:
            if (current == command.parameter) i = SkipBlock(commands, i);
            break;
        case 0x500:
            i = SkipBlock(commands, i);
            break;
        case 0x501:
            break;
        default:
            if (Debug()) fprintf(stderr, "pause: response command %x not modelled\n", command.type);
            break;
        }
    }
}

void Process(const PendingMessage &pending) {
    NativeScreen *screen = pending.screen;
    if (!screen->shown) return;
    if (pending.target == kTargetGame) {
        if (Debug()) fprintf(stderr, "pause: %s <- %08x (%p)\n", screen->name, pending.message, (void *)pending.object);
        screen->Deliver(pending.message, (int)(size_t)pending.object, 0);
    } else if (pending.target == kTargetPackage) {
        const FengResponse *response = FindFengResponse(screen->package->Responses(), pending.message);
        if (response) RunResponse(screen, *response, 0);
    } else if (pending.target == kTargetObject && pending.targetObject) {
        const FengResponse *response = FindFengResponse(pending.targetObject->responses, pending.message);
        if (response) RunResponse(screen, *response, pending.targetObject);
    }
}

void ShowScreen(NativeScreen *screen);
void CloseScreen(NativeScreen *screen);

void ApplyDeferred() {
    if (removeScreen) {
        NativeScreen *screen = removeScreen;
        removeScreen = 0;
        CloseScreen(screen);
    }
    if (switchFrom || switchTo) {
        NativeScreen *from = switchFrom, *to = switchTo;
        switchFrom = switchTo = 0;
        if (from) CloseScreen(from);
        if (to) ShowScreen(to);
    }
}

void Pump() {
    for (int guard = 0; guard < 256; ++guard) {
        ApplyDeferred();
        if (messages.empty()) break;
        PendingMessage pending = messages.front();
        messages.erase(messages.begin());
        Process(pending);
    }
    ApplyDeferred();
}

// 0x54d4b0: a pad message goes to the focus object when it responds to it, else to the
// package-level responses; nothing else is sent. Accept is 0x0c407210 on the focus object and
// PAD_ACCEPT on the package.
void Input(NativeScreen *screen, unsigned int message) {
    FengObject *focus = static_cast<FengObject *>(screen->fePackage.focus8C);
    if (focus && FindFengResponse(focus->responses, message)) {
        Post(screen, message, 0, kTargetObject, focus);
        Post(screen, message, focus, kTargetInput);
        return;
    }
    if (message == kAccept) message = kPadAccept;
    if (FindFengResponse(screen->package->Responses(), message)) {
        Post(screen, message, 0, kTargetPackage);
        Post(screen, message, 0, kTargetInput);
    }
}

FengObject *DialogObject(unsigned int hash) { return dialogScreen.package->Find(hash); }

void DialogHighlight() {
    // Button labels 0xda473d2f (left) and 0xda473d30 (right); highlight colour from 'No'.
    const unsigned int labels[2] = {0xda473d2f, 0xda473d30};
    for (int i = 0; i < 2; ++i) {
        FengObject *label = DialogObject(labels[i]);
        if (label) FengPackage::SetColour(label, i == dialog.selected ? 0xffccf48a : 0xffe1e1e1);
    }
}

void CloseDialog() {
    dialog.shown = false;
    dialogScreen.shown = false;
}

NativeScreen *ActiveScreen() {
    if (controlsScreen.shown) return &controlsScreen;
    for (size_t i = 0; i < kScreenCount; ++i) {
        if (screens[i] != &dialogScreen && screens[i]->shown) return screens[i];
    }
    return 0;
}

void ShowScreen(NativeScreen *screen) {
    if (!screen->loaded || screen->shown) return;
    std::string error;
    if (!screen->Reset(&error)) NativeTrap("FEng package reload");
    screen->shown = true;
    // FEng creation data: package name, three unidentified words, help-bar mask (+0x20).
    static unsigned int data[5];
    data[0] = (unsigned int)(size_t)screen->name;
    screen->object = screen->factory(data);
    if (Debug()) fprintf(stderr, "pause: showing %s\n", screen->name);
}

void CloseScreen(NativeScreen *screen) {
    if (!screen->shown) return;
    screen->DestroyObject();
    screen->shown = false;
    RemoveScripts(screen);
    for (size_t i = 0; i < messages.size();) {
        if (messages[i].screen == screen) messages.erase(messages.begin() + i);
        else ++i;
    }
    if (dialog.parent == screen) CloseDialog();
}

void CloseAll() {
    for (size_t i = 0; i < kScreenCount; ++i) CloseScreen(screens[i]);
    CloseDialog();
    scripts.clear();
    messages.clear();
    switchFrom = switchTo = removeScreen = 0;
}

// IconScroller embedded at IconScrollerFNGObject +0x4c (base vtable 0x795cbc, derived
// 0x79c0f4). Layout and selection follow reads of 0x543a90, 0x543c10, 0x52f8c0, 0x50d9c0,
// 0x538a30 and 0x51f9f0; the scroll animation (0x574e30) is not modelled.
class NativeIconScroller : public IconScroller4C {
public:
    struct Entry {
        IconScrollerIcon *icon;
        FengObject *object;
        float x;
        float width;
        float height;
    };

    explicit NativeIconScroller(IconScrollerFNGObject *owner)
        : owner_(owner), count_(1), runningX_(0.0f), selected_(-1), inputEnabled_(true), stopped_(false) {
        // 0x543a90: the OPTION_MASTER position anchors the icons; the master itself is hidden.
        master_ = static_cast<FengObject *>(FngFindObject(owner->package4, FngHash("OPTION_MASTER")));
        if (master_) FengPackage::Hide(master_);
    }
    ~NativeIconScroller() {
        for (size_t i = 0; i < entries_.size(); ++i) delete entries_[i].icon;
    }

    void UnknownSlot00() { NativeTrap("IconScroller scalar destructor 0x543bf0"); }
    void UnknownSlot04() { Layout(); }  // 0x538940
    void UnknownSlot08() { NativeTrap("IconScroller add slot 0x543c10 (reached through AddIcon)"); }
    void UnknownSlot0C() { NativeTrap("IconScroller react slot 0x50d980 (reached through MessageHandler)"); }
    void UnknownSlot10() { NativeTrap("IconScroller slot 0x52f7c0"); }
    void UnknownSlot14() { NativeTrap("IconScroller slot 0x52f680"); }
    void UnknownSlot18() { NativeTrap("IconScroller slot 0x538b80"); }
    void UnknownSlot1C() { NativeTrap("IconScroller slot 0x51f870"); }
    void UnknownSlot20(int index) {  // 0x538a30
        if (!entries_.empty()) selected_ = index > 0 && index <= (int)entries_.size() ? index - 1 : 0;
        Layout();
    }

    // 0x543c10: icons fill the package's option_1, option_2, ... objects left to right,
    // 40 units apart; an icon without an object is deleted.
    void Add(IconScrollerIcon *icon) {
        char name[32];
        snprintf(name, sizeof(name), "option_%d", count_);
        FengObject *object = static_cast<FengObject *>(FngFindObject(owner_->package4, FngHash(name)));
        if (!object) {
            delete icon;
            return;
        }
        ++count_;
        Entry entry = {icon, object, runningX_, object->size[0], object->size[1]};
        runningX_ += 40.0f + entry.width;
        // 0x50dab0: the object's texture becomes the icon's valueC.
        FengPackage::SetTexture(object, icon->valueC);
        entries_.push_back(entry);
        if (selected_ < 0) selected_ = 0;
    }

    void Layout() {
        if (!master_ || selected_ < 0) return;
        float masterX = master_->position[0], masterY = master_->position[1];
        float scroll = -entries_[selected_].x;
        for (size_t i = 0; i < entries_.size(); ++i) {
            Entry &e = entries_[i];
            float x = scroll + e.x + masterX;
            // 0x50d9c0: shown inside the 480-unit window (0x552330 argument) around the master.
            if (masterX - 240.0f <= x && x < masterX + 240.0f) FengPackage::Show(e.object);
            else FengPackage::Hide(e.object);
            float w = e.width, h = e.height, y = masterY;
            if ((int)i != selected_) {
                // 0x52f8c0, alignment 1: unselected icons at 0.8 (0x7850ec), bottom aligned.
                w *= 0.8f;
                y = (h - h * 0.8f) * 0.5f + masterY;
                h *= 0.8f;
            }
            e.object->size[0] = w;
            e.object->size[1] = h;
            FengPackage::SetPosition(e.object, x, y);
        }
        // 0x52f680 -> 0x52ce40: the selected icon's object takes the package focus.
        nativeFengManager.SetFocusByHash(owner_->package4, entries_[selected_].object->hash10);
    }

    // 0x51f9f0 (no wrap): steps over disabled icons (+0x20); stays put when none is left.
    bool Move(int direction) {
        for (int i = selected_ + direction; i >= 0 && i < (int)entries_.size(); i += direction) {
            if (entries_[i].icon->value20) continue;
            selected_ = i;
            Layout();
            return true;
        }
        return false;
    }

    // 0x50d980: React only when the message names the selected icon's object.
    bool Pointer(float x, float y) {
        FengPackage *package = NativeFrontendPackage(owner_->package4);
        for (size_t i = 0; package && i < entries_.size(); ++i) {
            if (!entries_[i].icon->value20 && package->Contains(entries_[i].object, x, y)) {
                if (selected_ != (int)i) { selected_ = (int)i; Layout(); owner_->SelectedIconChanged(); }
                return true;
            }
        }
        return false;
    }

    void React(unsigned int message, int value8, int valueC, int value10) {
        IconScrollerIcon *icon = Selected();
        if (icon && value8 == (int)(size_t)entries_[selected_].object)
            icon->React(owner_->package4, message, value8, valueC, value10);
    }

    IconScrollerIcon *Selected() { return selected_ >= 0 ? entries_[selected_].icon : 0; }
    FengObject *SelectedObject() { return selected_ >= 0 ? entries_[selected_].object : 0; }
    int SelectedIndex() const { return selected_ >= 0 ? selected_ + 1 : -1; }  // 0x51f820, 1-based
    bool Empty() const { return entries_.empty(); }

    IconScrollerFNGObject *owner_;
    FengObject *master_;
    std::vector<Entry> entries_;
    int count_;
    float runningX_;
    int selected_;
    bool inputEnabled_;  // +0x7b
    bool stopped_;       // +0x14c
};

NativeIconScroller *ScrollerOf(IconScrollerFNGObject *object) {
    return static_cast<NativeIconScroller *>(object->Scroller());
}

}  // namespace

// ---- Shared frontend seams (NativeFrontend.h) ------------------------------------------------

FengPackage *NativeFrontendPackage(const char *package) {
    NativeScreen *screen = FindScreen(package);
    return screen && screen->shown ? screen->package : 0;
}

void NativeFrontendSetFocus(const char *package, FengObject *object) {
    NativeScreen *screen = FindScreen(package);
    if (screen) screen->fePackage.SetFocus(&nativeFengQueue, object, 1);
}

void NativeFrontendPlayScript(FengObject *object, unsigned int hash, bool restart) {
    PlayScript(ScreenOfObject(object), object, hash, restart);
}

std::string NativeFrontendString(unsigned int hash) { return Localised(hash); }

// Package lookup, static-record lookup and queue storage remain native seams.
FEPackage *FngFindPackage(const char *name) {
    NativeScreen *screen = FindScreen(name);
    return screen && screen->shown ? &screen->fePackage : 0;
}
FngRecord *FngFindRecord(unsigned int hash) {
    for (size_t i = 0; i < kScreenCount; ++i) {
        if (screens[i]->loaded && FengHash(screens[i]->name) == hash) return &screens[i]->record;
    }
    return 0;
}
FEngPackageNode *FEngArchive::FindNode(const char *name) {
    NativeScreen *screen = FindScreen(name);
    return screen && screen->shown ? &screen->node : 0;
}
void FEngQueue::Post(unsigned int message, void *sender, FEPackage *package, int target, int) {
    for (size_t i = 0; i < kScreenCount; ++i) {
        if (&screens[i]->fePackage != package) continue;
        ::Post(screens[i], message, static_cast<FengObject *>(sender),
               target == -5 ? kTargetInput : kTargetObject,
               target == -5 ? 0 : reinterpret_cast<FengObject *>(target));
        return;
    }
}

// ---- IconScroller base seams --------------------------------------------------------------

// 0x51f670, as read.
IconScrollerIcon::IconScrollerIcon(unsigned int value0C, unsigned int value2C_, unsigned int value30_) {
    memset(unidentified4, 0, sizeof(unidentified4));
    memset(unidentified10, 0, sizeof(unidentified10));
    memset(unidentified21, 0, sizeof(unidentified21));
    memset(unidentified34, 0, sizeof(unidentified34));
    memset(unidentified46, 0, sizeof(unidentified46));
    valueC = value0C;
    value2C = value2C_;
    value30 = value30_;
    value20 = 0;
    unidentified21[0] = 1;  // +0x21
    float one = 1.0f;
    for (int i = 0; i < 4; ++i) memcpy(unidentified34 + i * 4, &one, 4);  // +0x34..+0x40
    unidentified34[0x10] = 1;  // +0x44
    value45 = 0;
}

IconScrollerIcon::~IconScrollerIcon() {}  // 0x527770

// 0x552330 / 0x551de0: package name from data[0], help-bar mask +0x20 from data[3].
IconScrollerFNGObject::IconScrollerFNGObject(void *data) {
    const unsigned int *words = static_cast<const unsigned int *>(data);
    package4 = reinterpret_cast<const char *>(words[0]);
    memset(unidentified8, 0, sizeof(unidentified8));
    value20 = (int)words[3];
    memset(unidentified24, 0, sizeof(unidentified24));
    value13C = 0;
    memset(unidentified140, 0, sizeof(unidentified140));
    typedef char scroller_fits[sizeof(NativeIconScroller) <= sizeof(scroller4C) ? 1 : -1];
    (void)sizeof(scroller_fits);
    new (scroller4C) NativeIconScroller(this);
}

IconScrollerFNGObject::~IconScrollerFNGObject() {  // 0x4defb0
    ScrollerOf(this)->~NativeIconScroller();
}

int IconScrollerFNGObject::UnknownSlot04(int) {
    NativeTrap("IconScrollerFNGObject slot 0x40d080");
    return 0;
}

int IconScrollerFNGObject::UnknownSlot0C(int, int, int, int, int) {
    NativeTrap("IconScrollerFNGObject slot 0x570a70");
    return 0;
}

// 0x543d40: the messages the native demo produces. Sounds, Dim/UNDIM scripts and the
// vertical/other cases are not modelled.
void IconScrollerFNGObject::MessageHandler(unsigned int message, int value8, int valueC, int value10) {
    NativeIconScroller *scroller = ScrollerOf(this);
    switch (message) {
    case kPadLeft:
    case kPadRight:
        if (scroller->Empty() || !scroller->inputEnabled_) return;
        if (scroller->Move(message == kPadLeft ? -1 : 1)) {
            SelectedIconChanged();
            FngSetPackageValue10(package4, scroller->SelectedIndex());
        }
        break;
    case kAccept: {
        IconScrollerIcon *icon = scroller->Selected();
        if (scroller->Empty() || !scroller->inputEnabled_ || !icon || icon->value20) return;
        if (!icon->value45) {
            StopInput();
        } else {
            scroller->React(kAccept, value8, valueC, value10);
        }
        Value50DA20(kAccept, value8, valueC, value10);
        break;
    }
    case kPadBack:
        Value50DA20(kPadBack, value8, valueC, value10);
        StopInput();
        break;
    case kExitComplete: {
        IconScrollerIcon *icon = scroller->Selected();
        if (scroller->Empty() || !icon || icon->value45) return;
        int stored[3];
        memcpy(stored, unidentified140, sizeof(stored));
        scroller->React(value13C, stored[0], stored[1], stored[2]);
        break;
    }
    case kStopDim:
        if (scroller->stopped_) {
            scroller->inputEnabled_ = true;
            scroller->stopped_ = false;
        }
        break;
    }
}

// 0x50da50: ICON_TITLE shows the selected icon's value2C; the second label (name "") is absent.
void IconScrollerFNGObject::SelectedIconChanged() {
    IconScrollerIcon *icon = ScrollerOf(this)->Selected();
    FngSetLabelString(FngFindObject(package4, FngHash("ICON_TITLE")), icon ? icon->value2C : 0);
}

void IconScrollerFNGObject::AddIcon(IconScrollerIcon *icon) { ScrollerOf(this)->Add(icon); }  // 0x50dab0

void IconScrollerFNGObject::StopInput() {  // 0x52fbf0
    ScrollerOf(this)->inputEnabled_ = false;
    ScrollerOf(this)->stopped_ = true;
}

void IconScrollerFNGObject::Value50DA20(unsigned int message, int value8, int valueC, int value10) {  // 0x50da20
    value13C = message;
    int stored[3] = {value8, valueC, value10};
    memcpy(unidentified140, stored, sizeof(stored));
}

// ---- FEng seams (cdecl interfaces declared in PauseMenu.h / PauseOptions.h) -----------------

unsigned int FngHash(const char *name) { return FengHash(name); }  // 0x505450

void *FngFindObject(const char *package, unsigned int hash) {  // 0x537a10
    NativeScreen *screen = FindScreen(package);
    return screen ? screen->package->Find(hash) : 0;
}

void *FngFindObject5379C0(const char *package, unsigned int hash) {  // 0x5379c0
    return FngFindObject(package, hash);
}

void FngSetLabelString(void *object, unsigned int hash) {  // 0x50c900
    if (object) FengPackage::SetText(static_cast<FengObject *>(object), hash ? Localised(hash).c_str() : "");
}

// 0x537be0: formats into the string object (the options use it with literal strings only).
void FngPrintf(void *object, const char *format, ...) {
    if (object) FengPackage::SetText(static_cast<FengObject *>(object), format);
}

void FngHideObject(void *object) { FengPackage::Hide(static_cast<FengObject *>(object)); }  // 0x50ca00
void FngShowObject(void *object) { FengPackage::Show(static_cast<FengObject *>(object)); }  // 0x50ca50

// 0x51cf70: object script by name (Highlight, UnHighlight, Disable, Disabled, Blink).
void FngSetObjectScript(void *object, const char *script, bool restart) {
    NativeFrontendPlayScript(static_cast<FengObject *>(object), FengHash(script), restart);
}

// 0x50d150: menu sound from table 0x837fe0; there is no frontend audio in the demo.
void FngPlayMenuSound(const char *, int, int) {}

// The PC help bar (UI_PC_Help_Bar.fng) is tracked but not drawn: PCHelpBarFNGObject
// 0x54e6a0/0x54e6e0 is not recovered.
const char *FngHelpBarName() { return "UI_PC_Help_Bar.fng"; }  // 0x50cea0

bool FngIsLoaded(const char *package) {  // 0x52cf60
    if (FengHash(package) == FengHash(FngHelpBarName())) return helpBarLoaded;
    NativeScreen *screen = FindScreen(package);
    return screen && screen->shown;
}

// 0x5379a0: removing a screen package closes it after the current message.
void FngRemove(const char *package) {
    if (FengHash(package) == FengHash(FngHelpBarName())) {
        helpBarLoaded = false;
        return;
    }
    NativeScreen *screen = FindScreen(package);
    if (screen && screen->shown) removeScreen = screen;
}

void FngAdd(const char *package, const char *, int, int) {  // 0x555f10
    if (FengHash(package) == FengHash(FngHelpBarName())) helpBarLoaded = true;
}

// 0x537980: replaces the calling package with `name`. Screens without recovered code (replay,
// PC customize options, performance dyno) return to the game.
void FngShow(const char *from, const char *name, int) {
    NativeScreen *target = FindScreen(name);
    if (!target || !target->factory) {
        fprintf(stderr, "ug2demo: %s is not available in the native demo; resuming\n", name);
        request = kPauseRequestResume;
        return;
    }
    switchFrom = FindScreen(from);
    switchTo = target;
}

bool FngScriptActive(const char *package, unsigned int object, const char *script) {  // 0x537cc0
    return FngScriptRunning(package, object, FengHash(script));
}

// 0x537d40: the object's current script is `script` and has not finished.
bool FngScriptRunning(const char *package, unsigned int object, unsigned int script) {
    NativeScreen *screen = FindScreen(package);
    for (size_t i = 0; i < scripts.size(); ++i) {
        if (scripts[i].screen == screen && scripts[i].object->hash10 == object && scripts[i].script->hash == script)
            return true;
    }
    return false;
}

void FngSetScript(const char *package, unsigned int object, const char *script, int restart) {  // 0x537c00
    FngSetScriptHash(package, object, FengHash(script), restart);
}

void FngSetScriptHash(const char *package, unsigned int objectHash, unsigned int script, int restart) {  // 0x537c60
    NativeScreen *screen = FindScreen(package);
    FengObject *object = screen ? screen->package->Find(objectHash) : 0;
    PlayScript(screen, object, script, restart != 0);
}

// 0x55dda0 -> 0x55b5c0: straight to the package's FNG object.
void FngSendMessage(unsigned int message, const char *package) {
    NativeScreen *screen = FindScreen(package);
    if (screen) Post(screen, message, 0, kTargetGame);
}

void FngShowDialog(const char *package, const char *, int value8, unsigned int valueC, unsigned int value10,
                   unsigned int value14, unsigned int value18, int value1C, unsigned int value20) {  // 0x558790
    if (!dialogScreen.loaded) {
        NativeTrap("UI_InGameDialog.fng is not loaded");
        return;
    }
    std::string error;
    if (!dialogScreen.Reset(&error)) NativeTrap("UI_InGameDialog.fng reload");
    if (Debug()) fprintf(stderr, "pause: dialog prompt %08x\n", value20);
    dialog.shown = true;
    dialog.parent = FindScreen(package);
    dialog.results[0] = value10;
    dialog.results[1] = value14;
    dialog.backResult = value18;
    dialog.selected = value1C == 0 ? 0 : 1;
    dialogScreen.shown = true;
    // Two-button form: the centre button and the memory-card header are not used here.
    const unsigned int hidden[] = {FengHash("Button3"), 0xda473d31, FengHash("Header_text_02"), 0xc26a8d5c};
    for (size_t i = 0; i < sizeof(hidden) / sizeof(hidden[0]); ++i) {
        FengObject *object = DialogObject(hidden[i]);
        if (object) FengPackage::Hide(object);
    }
    FngSetLabelString(DialogObject(FengHash("message_blurb")), value20);
    FngSetLabelString(DialogObject(0xda473d2f), (unsigned int)value8);
    FngSetLabelString(DialogObject(0xda473d30), valueC);
    DialogHighlight();
}

void FngSetReturnPackage(const char *package) { returnPackage = package; }  // 0x50c8d0

int ControllerAssigned(int) { return 1; }  // 0x571a60: the keyboard is always assigned

int FngFilterSlot0C(int, int) {  // 0x50d1d0
    NativeTrap("FEng slot filter 0x50d1d0");
    return 0;
}

// ---- Game flow and other seams ------------------------------------------------------------

// 0x5f4240. Message meanings are not established; the demo maps the ones the menus queue.
void GameFlow8900D8::Queue8(int value) {
    if (Debug()) fprintf(stderr, "pause: game flow 0x5f4240(%d)\n", value);
    switch (value) {
    case 1:  // PAD_BACK/START after EXIT_COMPLETE, "Resume Race", options closed with START
    case 2:  // "Return to Free Run"
        request = kPauseRequestResume;
        break;
    case 3:  // "Restart" dialog result
        request = kPauseRequestRestart;
        break;
    case 6:  // QuitToProfile / QuitToCrib
        request = kPauseRequestQuit;
        break;
    default:
        fprintf(stderr, "ug2demo: game-flow message %d is not modelled; resuming\n", value);
        request = kPauseRequestResume;
        break;
    }
}

void GameFlow8900D8::Queue(int type, int value) {  // 0x5f41e0
    fprintf(stderr, "ug2demo: game-flow message %d/%d is not modelled; resuming\n", type, value);
    request = kPauseRequestResume;
}

void PCHelpBarFNGObject::Show() {}                    // 0x54e6a0, not drawn
void PCHelpBarFNGObject::SyncByMask(int, const char *) {}  // 0x54e6e0, not drawn

void Career85AD40::Value513B30() { NativeTrap("career 0x513b30"); }
void Career85AD40::Value5139C0() { NativeTrap("career 0x5139c0"); }
// 0x502ff0: the front-end return package is not modelled (the demo has no front end).
bool Network89CF48::Value5F0120() {
    NativeTrap("network 0x5f0120");
    return false;
}

// ---- NativePause ----------------------------------------------------------------------------

NativePause::NativePause() {
    memset(&dialog, 0, sizeof(dialog));
    memset(&nativeFlow, 0, sizeof(nativeFlow));
    memset(&nativeReplay, 0, sizeof(nativeReplay));
}

NativePause::~NativePause() { CloseAll(); }

bool NativePause::Load(const char *gameDirectory, NativeTextureCache *cache, const char *fontPath, char *error,
                       int errorSize) {
    textures = cache;
    std::string base = gameDirectory;
    std::vector<unsigned char> bundle, global;
    if (!ReadNativeFile(base + "/GLOBAL/InGameCommon.bun", &bundle)) {
        snprintf(error, errorSize, "cannot read InGameCommon.bun");
        return false;
    }
    NativeImageMap images;
    LoadNativeTextures(&bundle[0], bundle.size(), &images);
    if (ReadNativeFile(base + "/GLOBAL/GLOBALB.BUN", &global)) LoadNativeTextures(&global[0], global.size(), &images);
    cache->Add(images);
    std::string reason;
    if (!LoadCompressedPackage(bundle, &pauseScreen, &reason) || !LoadCompressedPackage(bundle, &dialogScreen, &reason)) {
        snprintf(error, errorSize, "%s", reason.c_str());
        return false;
    }
    if (!LoadPlainPackage(bundle, &optionsMainScreen, &reason) || !LoadPlainPackage(bundle, &optionsScreen, &reason))
        fprintf(stderr, "ug2demo: %s; pause options are not available\n", reason.c_str());
    std::vector<unsigned char> frontend;
    if (ReadNativeFile(base + "/FRONTEND/FrontB.lzc", &frontend))
        LoadPlainPackage(frontend, &controlsScreen, &reason);
    if (!controlsScreen.loaded) fprintf(stderr, "ug2demo: %s; controls screen unavailable\n", reason.c_str());
    ReadNativeFile(base + "/LANGUAGES/English.bin", &language);
    if (!font.Load(gameDirectory, cache, fontPath)) fprintf(stderr, "ug2demo: no font found; pause menu text is not drawn\n");
    // Objects behind the .bss pointers the recovered code dereferences.
    gameFlow8900D8 = &nativeFlow;
    pcHelpBar8384D8 = &nativeHelpBar;
    replay8650B8 = &nativeReplay;
    NativePauseOptionsLoad(gameDirectory);
    return true;
}

bool NativePause::Loaded() const { return pauseScreen.loaded; }

bool NativePause::Active() const { return ActiveScreen() != 0; }

void NativePause::Open() {
    if (!pauseScreen.loaded || ActiveScreen()) return;
    request = kPauseRequestNone;
    ShowScreen(&pauseScreen);
    Pump();
}

void NativeFrontendOpenControls() {
    if (!dialog.shown && ActiveScreen() && !controlsScreen.shown) { ShowScreen(&controlsScreen); Pump(); }
}
void NativeFrontendCloseControls() { CloseScreen(&controlsScreen); }
void NativePause::OpenControls() { NativeFrontendOpenControls(); }
bool NativePause::CaptureKey(int scancode) {
    return controlsScreen.shown && NativeControlsCapture(controlsScreen.object, (SDL_Scancode)scancode);
}

void NativePause::Key(NativePauseKey key) {
    NativeScreen *screen = ActiveScreen();
    if (!screen) return;
    if (screen == &controlsScreen) { NativeControlsKey(screen->object, key); return; }
    if (key == kPauseKeyDevice) return;
    if (dialog.shown) {
        if (key == kPauseKeyLeft || key == kPauseKeyRight) {
            dialog.selected = key == kPauseKeyLeft ? 0 : 1;
            DialogHighlight();
        } else if (key == kPauseKeyAccept || key == kPauseKeyBack) {
            unsigned int result = key == kPauseKeyAccept ? dialog.results[dialog.selected] : dialog.backResult;
            NativeScreen *parent = dialog.parent;
            CloseDialog();
            if (parent) Post(parent, result, 0, kTargetGame);
        }
    } else {
        static const unsigned int messagesForKey[] = {kPadLeft, kPadRight, kAccept, kPadBack, kPadUp, kPadDown, kPadStart};
        Input(screen, messagesForKey[key]);
    }
    Pump();
}

void NativePause::Pointer(float x, float y, bool click, bool drag) {
    NativeScreen *screen = ActiveScreen();
    if (!screen) return;
    if (screen == &controlsScreen) { NativeControlsPointer(screen->object, x, y, click); return; }
    if (dialog.shown) {
        // Native text hit regions; dialog labels have glyph scales rather than image bounds.
        const unsigned int labels[2] = {0xda473d2f, 0xda473d30};
        for (int i = 0; i < 2; ++i) {
            FengObject *label = DialogObject(labels[i]);
            if (label && x >= label->position[0] - 55 && x <= label->position[0] + 55 &&
                y >= label->position[1] - 12 && y <= label->position[1] + 18) {
                dialog.selected = i; DialogHighlight();
                if (click) Key(kPauseKeyAccept);
                return;
            }
        }
    } else if (screen->optionsList) {
        NativePauseOptionsPointer(screen->object, x, y, click, drag);
        Pump();
    } else if (click && screen->object && ScrollerOf(static_cast<IconScrollerFNGObject *>(screen->object))->Pointer(x, y)) {
        if (click) Key(kPauseKeyAccept);
    }
}

void NativePause::Update(float seconds) {
    // Real-time clock seam 0x8651ac (4 kHz units assumed); AcceptsInput (0x4a7bc0) waits for
    // it to change after the screen opens.
    clockRemainder += seconds * 4000.0f;
    int ticks = (int)clockRemainder;
    if (ticks < 1) ticks = 1;
    clockRemainder -= (float)ticks;
    if (clockRemainder < 0.0f) clockRemainder = 0.0f;
    realClock8651AC += ticks;
    for (size_t i = 0; i < scripts.size();) {
        RunningScript &running = scripts[i];
        running.milliseconds += seconds * 1000.0f;
        const std::vector<FengEvent> &events = running.script->events;
        while (running.nextEvent < events.size() && events[running.nextEvent].time <= running.milliseconds) {
            if (events[running.nextEvent].target == 0xffffffff)
                Post(running.screen, events[running.nextEvent].message, 0, kTargetGame);
            ++running.nextEvent;
        }
        if (running.nextEvent >= events.size() && running.milliseconds >= running.script->duration)
            scripts.erase(scripts.begin() + i);
        else
            ++i;
    }
    for (size_t i = 0; i < kScreenCount; ++i) {
        if (!screens[i]->shown) continue;
        screens[i]->package->AdvanceScripts(seconds);
        if (screens[i] != &dialogScreen) Post(screens[i], kFrontendTick, 0, kTargetGame);  // 0x5663b0
    }
    Pump();
    NativePauseOptionsUpdate();
    if (controlsScreen.shown) NativeControlsUpdate(controlsScreen.object);
}

void NativePause::Draw(int width, int height) {
    if (controlsScreen.shown) { controlsScreen.package->Draw(textures, &font, width, height); return; }
    for (size_t i = 0; i < kScreenCount; ++i) {
        if (screens[i]->shown) screens[i]->package->Draw(textures, &font, width, height);
    }
}

NativePauseRequest NativePause::TakeRequest() {
    NativePauseRequest pending = request;
    request = kPauseRequestNone;
    if (pending != kPauseRequestNone) CloseAll();
    return pending;
}
