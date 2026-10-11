#include "PhysicsUpgrades.h"

extern int physicsUpgradeGroupCount8383FC;
extern PhysicsUpgradeGroup *physicsUpgradeGroups8383F8;
extern PhysicsUpgradeItem *physicsUpgradeItems837B90[];

int GetPhysicsUpgradeGroupCount() { return physicsUpgradeGroupCount8383FC; }
PhysicsUpgradeGroup *GetPhysicsUpgradeGroup(int index) { return physicsUpgradeGroups8383F8 + index; }
PhysicsUpgradeItem *GetPhysicsUpgradeItem(int index) { return physicsUpgradeItems837B90[index]; }

// Original 0x5a6320. The binary indexes these flags directly, without bounds
// checks. Retain prerequisite recursion and exclusion order, including the
// already-selected early return before inspecting prerequisites.
int PhysicsUpgradeSelection::SetItem(PhysicsUpgradeItem *item, char selected, char recursive) {
    int changed = 1;
    if (selected50[item->value0]) {
        if (!selected) {
            selected50[item->value0] = 0;
            return changed;
        }
        return 0;
    }
    if (!selected) return 0;
    for (int i = 0; i < 2; ++i) {
        int index = item->requirements54[i];
        if (index != -1 && !selected50[index]) {
            if (!recursive) return 0;
            changed += SetIndex(index, selected, recursive);
        }
    }
    for (int i = 0; i < 5; ++i) {
        if (item->exclusions40[i] != -1) {
            PhysicsUpgradeItem *other = GetPhysicsUpgradeItem(item->exclusions40[i]);
            if (other) SetItem(other, 0, 0);
        }
    }
    selected50[item->value0] = 1;
    return changed;
}

// Original 0x599680.
int PhysicsUpgradeSelection::SetIndex(int index, char selected, char recursive) {
    PhysicsUpgradeItem *item = GetPhysicsUpgradeItem(index);
    return item ? SetItem(item, selected, recursive) : 0;
}

// Original 0x5a6400; re-read the descriptor count after every call.
int PhysicsUpgradeSelection::SetGroup(PhysicsUpgradeGroup *group, char selected) {
    int changed = 0;
    for (unsigned int i = 0; i < group->count8; ++i) changed += SetItem(&group->itemsC[i], selected, 0);
    return changed;
}

// Original 0x5a6450; the count accessor is called again on each iteration.
int PhysicsUpgradeSelection::ClearGroups(int value) {
    int changed = 0;
    for (int i = 0; i < GetPhysicsUpgradeGroupCount(); ++i) {
        PhysicsUpgradeGroup *group = GetPhysicsUpgradeGroup(i);
        if (group && (value == -1 || value == group->value0)) changed += SetGroup(group, 0);
    }
    return changed;
}
