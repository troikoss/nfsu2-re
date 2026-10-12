#include "FEngPackage.h"
#include "PauseMenu.h"

extern FEngArchive fengArchive8637B0;

// Original 0x537950: initialise all four package input masks.
void FngSetControlMask(const char *name, unsigned int mask) {
    FEPackage *package = FngFindPackage(name);
    if (package) {
        package->inputMask1C = mask;
        package->inputMask20 = mask;
        package->inputMask24 = mask;
        package->inputMask28 = mask;
    }
}

// Original 0x50b8a0 / 0x50b8c0: persistent static FNG record selection.
int FngGetPackageValue10(const char *name) {
    FngRecord *record = FngFindRecord(FngHash(name));
    return record ? record->value10 : 0;
}
void FngSetPackageValue10(const char *name, int value) {
    FngRecord *record = FngFindRecord(FngHash(name));
    if (record) record->value10 = value;
}

// Original 0x52b3a0: enqueue object and owner focus notifications before changing focus.
void FEPackage::SetFocus(FEngQueue *queue, HudVisibilityObject *object, unsigned char notify) {
    if (notify) {
        if (focus8C) {
            queue->Post(0x55d1e635, 0, this, (int)focus8C, 0);
            queue->Post(0x55d1e635, focus8C, this, -5, 0);
        }
        if (object) {
            queue->Post(0xabc08912, 0, this, (int)object, 0);
            queue->Post(0xabc08912, object, this, -5, 0);
        }
    }
    focus8C = object;
}

// Original 0x52ce40: last matching hash in the focus array wins.
void FEngManager::SetFocusByHash(const char *name, unsigned int hash) {
    FEngPackageNode *node = fengArchive8637B0.FindNode(name);
    if (node && node->package18) {
        FEPackage *package = node->package18;
        HudVisibilityObject *object = 0;
        for (unsigned int i = 0; i < package->focusCount94; ++i) {
            if (package->focusObjects90[i]->hash10 == hash) object = package->focusObjects90[i];
        }
        if (object) package->SetFocus(queue8, object, 1);
    }
}
