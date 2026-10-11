// Platform presentation and SDL binding storage for recovered keyboard items.
// Original screen factory/constructor: 0x4f92e0 / 0x4ecd10; keyboard list: 0x4dba50.
// Full list ownership, device backends and screen handler 0x4f9340 remain seams.
#include "NativeControls.h"
#include "NativeFrontend.h"
#include "../../Speed/Indep/Src/Frontend/PCControls.h"
#include <stdio.h>
#include <stdlib.h>

ControlDevice874C40 controlDevice874C40;
namespace {
const char *packageName = "UI_Options_PC_Controller.fng";
int bindings[23][2];
bool initialized = false;
int captureAction = 0, captureSlot = 0;
struct Row { int action; unsigned int hash; };
const Row rows[] = {
    {3, 0x153e7337},
    {4, 0xbd7ba1aa},
    {1, 0x409607e2},
    {2, 0x82ebcc98},
    {7, 0x3eb318ce},
    {8, 0xb7d33921},
    {5, 0xb3d1ea35},
    {6, 0xdf5aef26},
    {10, 0x419ca63a},
    {9, 0x35b9f571},
    {15, 0x4a1341b3},
    {16, 0x1e7e3391},
    {17, 0x58baebba},
    {18, 0x735b2e6c},
    {14, 0x6fc04e6f},
    {22, 0xa081f0cc},
    {19, 0x64730aac},
    {12, 0x6f99f62a},
};
OptionButton *Binding(int row, int column, float *position, float *size) {
    switch (row * 2 + column) {
    case 0: return new PCControl7926A8(position, size);
    case 1: return new PCControl7926E8(position, size);
    case 2: return new PCControl792768(position, size);
    case 3: return new PCControl7927A8(position, size);
    case 4: return new PCControl792828(position, size);
    case 5: return new PCControl792868(position, size);
    case 6: return new PCControl7928E8(position, size);
    case 7: return new PCControl792928(position, size);
    case 8: return new PCControl7929A8(position, size);
    case 9: return new PCControl7929E8(position, size);
    case 10: return new PCControl792A28(position, size);
    case 11: return new PCControl792A68(position, size);
    case 12: return new PCControl792AA8(position, size);
    case 13: return new PCControl792AE8(position, size);
    case 14: return new PCControl792B28(position, size);
    case 15: return new PCControl792B68(position, size);
    case 16: return new PCControl792BA8(position, size);
    case 17: return new PCControl792BE8(position, size);
    case 18: return new PCControl792C28(position, size);
    case 19: return new PCControl792C68(position, size);
    case 20: return new PCControl792CA8(position, size);
    case 21: return new PCControl792CE8(position, size);
    case 22: return new PCControl792DA8(position, size);
    case 23: return new PCControl792DE8(position, size);
    case 24: return new PCControl792E28(position, size);
    case 25: return new PCControl792E68(position, size);
    case 26: return new PCControl792D28(position, size);
    case 27: return new PCControl792D68(position, size);
    case 28: return new PCControl792EA8(position, size);
    case 29: return new PCControl792EE8(position, size);
    case 30: return new PCControl792F28(position, size);
    case 31: return new PCControl792F68(position, size);
    case 32: return new PCControl792FA8(position, size);
    case 33: return new PCControl792FE8(position, size);
    case 34: return new PCControl793028(position, size);
    case 35: return new PCControl793068(position, size);
    }
    return 0;
}
FengObject *Named(FengPackage *p, const char *prefix, int index) {
    char name[64]; snprintf(name, sizeof(name), "%s%d", prefix, index); return p->Find(name);
}
void Script(FengObject *object, const char *name) { FengPackage::PlayScript(object, name, true); }
void Defaults() {
    if (initialized) return;
    initialized = true;
    bindings[1][0] = SDL_SCANCODE_UP; bindings[1][1] = SDL_SCANCODE_W;
    bindings[2][0] = SDL_SCANCODE_DOWN; bindings[2][1] = SDL_SCANCODE_S;
    bindings[3][0] = SDL_SCANCODE_LEFT; bindings[3][1] = SDL_SCANCODE_A;
    bindings[4][0] = SDL_SCANCODE_RIGHT; bindings[4][1] = SDL_SCANCODE_D;
    bindings[5][0] = SDL_SCANCODE_SPACE;
    bindings[9][0] = SDL_SCANCODE_V; bindings[10][0] = SDL_SCANCODE_C;
    bindings[12][0] = SDL_SCANCODE_ESCAPE; bindings[14][0] = SDL_SCANCODE_R;
}
struct Screen {
    Screen() : selected(0), column(0), top(0) {
        Defaults();
        float position[2] = {0,0}, size[2] = {175,24};
        for (int r = 0; r < 18; ++r) for (int c = 0; c < 2; ++c) items[r][c] = Binding(r,c,position,size);
        FengPackage *p = NativeFrontendPackage(packageName);
        const char *hide[] = {"TITLE_MASTER", "DATA_MASTER", "DATA_MASTER2", "scrollbar"};
        for (unsigned i = 0; i < sizeof(hide)/sizeof(hide[0]); ++i) FengPackage::Hide(p->Find(hide[i]));
        FngSetLabelString(p->Find(0x666593c4), 0x1b15f53c);
        FngSetLabelString(p->Find(0x59aba1e8), 0x37e740e0);
        FngSetLabelString(p->Find(0x42adb44c), 0x04dc6db5);
        FengPackage::SetText(p->Find(0x426c7b4d), "Keyboard");
        // Device switching needs the original gamepad/wheel list recovery; hide its arrows.
        FengPackage::Hide(p->Find(0xd1cda594)); FengPackage::Hide(p->Find(0x9345e4a7));
        Layout();
    }
    ~Screen() { for (int r=0;r<18;++r) for(int c=0;c<2;++c) delete items[r][c]; captureAction=0; }
    void Layout(bool refresh = true) {
        FengPackage *p = NativeFrontendPackage(packageName);
        if (selected >= 0) { if (selected < top) top=selected; if (selected >= top+7) top=selected-6; }
        FengObject *title=p->Find("TITLE_MASTER"), *data=p->Find("DATA_MASTER"), *data2=p->Find("DATA_MASTER2");
        if (!title || !data || !data2) return;
        for(int i=8;i<=18;++i) {
            FengPackage::Hide(Named(p,"OPTION_NAME_",i)); FengPackage::Hide(Named(p,"OPTION_DATA_",i)); FengPackage::Hide(Named(p,"OPTION_DATA_2_",i));
        }
        for(int i=0;i<7;++i) {
            int row=top+i;
            FengObject *name=Named(p,"OPTION_NAME_",i+1);
            FengObject *first=Named(p,"OPTION_DATA_",i+1), *second=Named(p,"OPTION_DATA_2_",i+1);
            if (!name || !first || !second) continue;
            FengPackage::Show(name); FengPackage::Show(first); FengPackage::Show(second);
            FngSetLabelString(name,rows[row].hash);
            FengPackage::SetPosition(name,title->position[0]-title->size[0]/2,title->position[1]-title->size[1]/2+i*40);
            FengPackage::SetPosition(first,data->position[0]-data->size[0]/2,data->position[1]-data->size[1]/2+i*40);
            FengPackage::SetPosition(second,data2->position[0]-data2->size[0]/2,data2->position[1]-data2->size[1]/2+i*40);
            items[row][0]->name2C=first; items[row][1]->name2C=second;
            items[row][0]->SetLabels(); items[row][1]->SetLabels();
            if (refresh) {
            Script(name, row==selected ? "Highlight" : "UnHighlight");
            Script(first,row==selected && column==0 ? "Highlight" : "UnHighlight");
            Script(second,row==selected && column==1 ? "Highlight" : "UnHighlight");
            }
        }
        if (refresh) Script(p->Find("DONE_BUTTON"),selected<0 ? "Highlight" : "UnHighlight");
        FengPackage::SetText(p->Find("Instructions_Label"), captureAction ? "Press a key. Esc cancels." : "Select a binding and press Enter / A. Esc / B returns.");
        if (refresh && getenv("UG2_DEBUG_PAUSE")) fprintf(stderr,"controls: row=%d column=%d top=%d capture=%d\n",selected+1,column+1,top,captureAction);
    }
    void Begin() { if(selected>=0) { items[selected][column]->React(packageName,0x406415e3); Layout(); } }
    int selected,column,top;
    OptionButton *items[18][2];
};
}
// 0x5c2100 boundary; session-only SDL storage replaces DirectInput/profile ownership.
void ControlDevice874C40::BeginBinding(int action,int slot) { captureAction=action; captureSlot=slot; }
// 0x4acf40 boundary; SDL key names replace the Windows key-name/wide-string backend.
void SetPCControlBindingLabel(void *object,int action,int slot,int) {
    Defaults();
    const char *name = captureAction==action && captureSlot==slot ? "..." :
        bindings[action][slot] ? SDL_GetScancodeName((SDL_Scancode)bindings[action][slot]) : "---";
    FengPackage::SetText(static_cast<FengObject *>(object),name);
}
void *CreateNativeControls(void *) { return new Screen; }
void DestroyNativeControls(void *screen) { delete static_cast<Screen *>(screen); }
void NativeControlsUpdate(void *s) { if (s) static_cast<Screen *>(s)->Layout(false); } // No retail binding backend is polled by the native adapter.
void NativeControlsKey(void *screen,NativePauseKey key) {
    Screen *s=static_cast<Screen *>(screen);
    if(captureAction) { if(key==kPauseKeyBack) { captureAction=0;s->Layout(); } return; }
    if(key==kPauseKeyUp) { if(s->selected>=0) --s->selected; }
    else if(key==kPauseKeyDown) { if(s->selected<17) ++s->selected; }
    else if(key==kPauseKeyLeft) s->column=0;
    else if(key==kPauseKeyRight) s->column=1;
    else if(key==kPauseKeyAccept) { if(s->selected<0) NativeFrontendCloseControls();else s->Begin();return; }
    else if(key==kPauseKeyBack || key==kPauseKeyStart) { NativeFrontendCloseControls();return; }
    s->Layout();
}
bool NativeControlsCapture(void *screen,SDL_Scancode key) {
    if(!captureAction) return false;
    if(key!=SDL_SCANCODE_ESCAPE && key!=SDL_SCANCODE_UNKNOWN) bindings[captureAction][captureSlot]=key;
    if(key!=SDL_SCANCODE_ESCAPE && key!=SDL_SCANCODE_UNKNOWN && getenv("UG2_DEBUG_PAUSE")) {
        Uint8 oldKeys[SDL_NUM_SCANCODES] = {}, newKeys[SDL_NUM_SCANCODES] = {};
        // Probe the actual gameplay reader, with only the newly assigned key held.
        newKeys[key]=1; oldKeys[SDL_SCANCODE_LEFT]=1;
        fprintf(stderr,"controls: bound action=%d slot=%d key=%s key-active=%d old-active=%d\n",captureAction,captureSlot,
                SDL_GetScancodeName(key),NativeControlPressed(captureAction,newKeys),NativeControlPressed(captureAction,oldKeys));
    }
    captureAction=0;static_cast<Screen *>(screen)->Layout();return true;
}
void NativeControlsPointer(void *screen,float x,float y,bool click) {
    Screen *s=static_cast<Screen *>(screen); if(captureAction) return;
    FengPackage *p=NativeFrontendPackage(packageName); FengObject *title=p->Find("TITLE_MASTER"), *data=p->Find("DATA_MASTER"), *second=p->Find("DATA_MASTER2");
    if(!title || !data || !second) return;
    float left=title->position[0]-title->size[0]/2, top=title->position[1]-title->size[1]/2;
    float dataX=data->position[0]-data->size[0]/2, secondX=second->position[0]-second->size[0]/2;
    if(x>=left && x<=secondX+150 && y>=top && y<top+7*40) {
        s->selected=s->top+(int)((y-top)/40);
        s->column=x<secondX ? 0 : 1;
        s->Layout();if(click && x>=dataX) s->Begin();
    } else {
        FengObject *done=p->Find("DONE_BUTTON");
        if(done && x>=done->position[0]-60 && x<=done->position[0]+100 && y>=done->position[1]-8 && y<=done->position[1]+24) {
            s->selected=-1;s->Layout();if(click) NativeFrontendCloseControls();
        }
    }
}
bool NativeControlKey(int action,SDL_Scancode key) {
    Defaults();return key!=SDL_SCANCODE_UNKNOWN && (bindings[action][0]==key || bindings[action][1]==key);
}
bool NativeControlPressed(int action,const Uint8 *keys) {
    Defaults();return (bindings[action][0] && keys[bindings[action][0]]) || (bindings[action][1] && keys[bindings[action][1]]);
}
