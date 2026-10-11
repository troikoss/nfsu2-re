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

// Original 0x500100. Match both descriptor keys; return the first match.
PhysicsUpgradeGroup *FindPhysicsUpgradeGroup(int value, int level) {
    int count = physicsUpgradeGroupCount8383FC;
    PhysicsUpgradeGroup *groups = physicsUpgradeGroups8383F8;
    for (int i = 0; i < count; ++i) {
        if (groups[i].value0 == value && groups[i].value4 == level) return &groups[i];
    }
    return 0;
}

// Original 0x5a61a0, cdecl. Each accumulation is stored as binary32. The
// descriptor's exact -1.0 bit pattern suppresses a component contribution.
void AccumulatePhysicsUpgradeWeights(float *output, PhysicsUpgradeSelection *selection,
                                     PhysicsUpgradeGroup *group, int count) {
    for (int component = 0; component < count; ++component) {
        for (unsigned int item = 0; item < group->count8; ++item) {
            PhysicsUpgradeItem *entry = &group->itemsC[item];
            if (selection->selected50[entry->value0]) {
                float *weight = reinterpret_cast<float *>(reinterpret_cast<char *>(entry) + 0x30 + component * 4);
                if (*reinterpret_cast<unsigned int *>(weight) != 0xbf800000u) {
                    output[component] = *weight + output[component];
                }
            }
        }
    }
}

// Original 0x5a64a0. Clearing happens even for a nonpositive grade. The binary
// iterates group items here directly, rather than calling SetGroup.
int PhysicsUpgradeSelection::SetGrade(int grade, int value) {
    int changed = 0;
    ClearGroups(value);
    int first = value == -1 ? 0 : value;
    int last = value == -1 ? 9 : value;
    for (int level = 1; level <= grade; ++level) {
        for (int kind = first; kind <= last; ++kind) {
            PhysicsUpgradeGroup *group = FindPhysicsUpgradeGroup(kind, level);
            if (group) {
                for (unsigned int i = 0; i < group->count8; ++i) changed += SetItem(&group->itemsC[i], 1, 0);
            }
        }
    }
    return changed;
}

extern float physicsUpgradeGradeScale;

// Original 0x5a6580. Return the unspilled x87 mean, while storing its clamped
// grade separately. The second averaging pass uses the binary32 reciprocal
// saved by the first pass, not a freshly computed division.
double PhysicsUpgradeSelection::GetFraction(int value, int count, float *output) {
    float local[4];
    if (!count || !output) {
        output = local;
        count = value == 1 || value == 5 ? 3 : (value == 9 ? 2 : 1);
    }
    for (int i = 0; i < count; ++i) output[i] = 0.0f;
    for (int level = 1; level <= 3; ++level) {
        PhysicsUpgradeGroup *group = FindPhysicsUpgradeGroup(value, level);
        if (group) AccumulatePhysicsUpgradeWeights(output, this, group, count);
    }
    double total = 0.0;
    for (int i = 0; i < count; ++i) total = total + (double)output[i];
    double reciprocal = 1.0 / (double)count;
    float savedReciprocal = (float)reciprocal;
    double mean = total * reciprocal;
    if (mean < 0.0 || mean > 1.0) {
        total = 0.0;
        for (int i = 0; i < count; ++i) {
            float weight = output[i];
            if (weight <= 0.0f) weight = 0.0f;
            else if (weight >= 1.0f) weight = 1.0f;
            output[i] = weight;
            total = total + (double)weight;
        }
        mean = total * (double)savedReciprocal;
    }
    double grade = mean;
    if (grade < 0.0) grade = 0.0;
    else if (grade > 1.0) grade = 1.0;
    values0[value] = (float)(grade * (double)physicsUpgradeGradeScale);
    return mean;
}
