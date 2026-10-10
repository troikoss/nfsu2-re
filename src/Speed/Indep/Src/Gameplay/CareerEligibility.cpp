#include "CareerReward.h"
#include "DriftRanking.h"

// Recovered control flow of PC v1.2 0x5287a0. Unidentified fields stay unnamed.
bool CareerRaceResult::CheckRewardEligibility(bool mode) {
    if (overrideEligibility) {
        return true;
    }
    if (mode) {
        for (int i = 0; i < entryCount; ++i) {
            CareerResultEntry *entry = GetEntry(i);
            if (entry->status != 4) {
                return false;
            }
            int classification = entry->participants.FindZeroSelector()->classification;
            if (classification != 1 && classification != 11) {
                return false;
            }
        }
    } else {
        for (int i = 0; i < entryCount; ++i) {
            CareerResultEntry *entry = GetEntry(i);
            if (entry->status != 4 && entry->status != 3) {
                return false;
            }
            int classification = entry->participants.FindZeroSelector()->classification;
            if (classification != 1 && classification != 11) {
                return false;
            }
        }
    }
    if (entryCount > 1) {
        return !GetOverallResult();
    }
    if (activeGameRace->raceType == 5) {
        return FindDriftListPosition(0) == 1;
    }
    for (int i = 0; i < entryCount; ++i) {
        CareerResultEntry *entry = GetEntry(i);
        if (entry->participants.count <= 0 || !entry->participants.detail || entry->participants.detail->value4 != 0) {
            return false;
        }
    }
    return true;
}
