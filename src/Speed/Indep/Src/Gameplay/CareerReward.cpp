#include "CareerReward.h"

// Original PC v1.2 0x532e50. No additional payout or eligibility rule invented.
int CareerReward::GetBankWonFromRace() {
    if (activeGameRace && result.CheckRewardEligibility(0)) {
        CareerRaceReward *race = currentRace;
        if (race->rewardType == 1) {
            CareerSponsorReward *sponsor = stages[stageIndex].sponsor;
            if (sponsor) {
                return sponsor->amount;
            }
        }
        return race->bank;
    }
    return 0;
}
