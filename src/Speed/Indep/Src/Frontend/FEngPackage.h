#ifndef UG2_FENG_PACKAGE_H
#define UG2_FENG_PACKAGE_H

#include "../Hud/HudVisibility.h"

struct FEngQueue {
    void Post(unsigned int message, void *sender, struct FEPackage *package,
              int target, int value10); // 0x51aa20, thiscall
};

// Observed FEPackage prefix; unidentified fields retain their original offsets.
struct FEPackage {
    char unidentified0[0x1c];
    unsigned int inputMask1C;
    unsigned int inputMask20;
    unsigned int inputMask24;
    unsigned int inputMask28;
    char unidentified2C[0x60];
    HudVisibilityObject *focus8C;
    HudVisibilityObject **focusObjects90;
    unsigned int focusCount94;
    void SetFocus(FEngQueue *queue, HudVisibilityObject *object, unsigned char notify); // 0x52b3a0
};
UG2_CHECK_OFFSET(FEPackage, inputMask1C, 0x1c);
UG2_CHECK_OFFSET(FEPackage, inputMask28, 0x28);
UG2_CHECK_OFFSET(FEPackage, focus8C, 0x8c);
UG2_CHECK_OFFSET(FEPackage, focusObjects90, 0x90);
UG2_CHECK_OFFSET(FEPackage, focusCount94, 0x94);
UG2_CHECK_SIZE(FEPackage, 0x98);

struct FEngPackageNode {
    char unidentified0[0x18];
    FEPackage *package18;
};
struct FEngArchive {
    FEngPackageNode *FindNode(const char *name); // 0x51bb70
};
struct FEngManager {
    char unidentified0[8];
    FEngQueue *queue8;
    void SetFocusByHash(const char *name, unsigned int hash); // 0x52ce40
};
UG2_CHECK_OFFSET(FEngPackageNode, package18, 0x18);
UG2_CHECK_OFFSET(FEngManager, queue8, 8);

// Persisted selection belongs to the static FNG record, not the live FEPackage.
struct FngRecord {
    char unidentified0[0x10];
    int value10;
};
FEPackage *FngFindPackage(const char *name); // 0x52cef0
FngRecord *FngFindRecord(unsigned int hash); // 0x50b840

#endif
