#ifndef UG2_CAREER_REWARD_H
#define UG2_CAREER_REWARD_H

// Selected PC v1.2 layout recovered from consumers at 0x532e50/0x5287a0.
// Padding preserves unidentified fields; it is not a claim of complete types.
struct CareerRaceReward {
    char unidentifiedBeforeType[0xf];
    unsigned char raceType;
    char unidentifiedAfterType[0x20];
    int bank;
    unsigned char rewardType;
};

struct CareerSponsorReward {
    char unidentified[2];
    unsigned short amount;
};

struct CareerStageReward {
    char unidentifiedBefore[0xe8];
    CareerSponsorReward *sponsor;
    char unidentifiedAfter[0x18];
};

struct CareerResultParticipant {
    int unidentified0;
    int selector;
    int classification;
    char unidentified[0x218];
};

struct CareerResultDetail {
    int unidentified0;
    int value4;
};

struct CareerResultParticipants {
    CareerResultParticipant participants[6];
    CareerResultDetail *detail;
    char unidentifiedBeforeCount[0x14];
    int count;

    CareerResultParticipant *FindZeroSelector() {
        for (int i = 0; i < count; ++i) {
            if (participants[i].selector == 0) {
                return &participants[i];
            }
        }
        return 0;
    }
};

struct CareerResultEntry {
    char unidentifiedBeforeStatus[0x14];
    int status;
    char unidentifiedBeforeParticipants[0x18];
    CareerResultParticipants participants;
};

struct CareerResultTableEntry {
    void *unidentified0;
    int value4;
    int unidentified8;
};

struct CareerRaceResult {
    unsigned char overrideEligibility;
    char unidentifiedFlags[7];
    CareerResultTableEntry table[6];
    int tableCount;
    CareerResultEntry entries[4];
    CareerResultEntry *selected;
    int entryCount;

    CareerResultEntry *GetEntry(int index) {
        return index >= 0 && index < entryCount && index < 4 ? &entries[index] : 0;
    }
    int GetOverallResult();
    bool CheckRewardEligibility(bool mode);
};

struct CareerReward {
    char unidentifiedBeforeResult[0xd24];
    CareerRaceResult result;
    char unidentifiedBeforeStages[0x30c8];
    CareerStageReward stages[8];
    int stageIndex;
    char unidentifiedBeforeRace[0xc34];
    CareerRaceReward *currentRace;
    int GetBankWonFromRace();
};

extern CareerRaceReward *activeGameRace;

typedef char VerifyParticipantSize[sizeof(CareerResultParticipant) == 0x224 ? 1 : -1];
typedef char VerifyParticipantsSize[sizeof(CareerResultParticipants) == 0xcf4 ? 1 : -1];
typedef char VerifyEntrySize[sizeof(CareerResultEntry) == 0xd24 ? 1 : -1];
typedef char VerifyResultSize[sizeof(CareerRaceResult) == 0x34ec ? 1 : -1];
typedef char VerifyStageSize[sizeof(CareerStageReward) == 0x104 ? 1 : -1];

#endif
