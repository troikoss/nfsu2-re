#include "HudPlayerMasks.h"
#include "../Frontend/PauseOptions.h"
#include "../Physics/ForceFeedback.h"

extern GameFlow8900D8 *gameFlow8900D8;
extern RaceStatus890118 *raceStatus890118;
extern PlayerRecord *players8900B4[2];
extern int playerCount8900BC;
extern int value82A3A4;
extern Object85EF50 object85EF50;
extern Object865480 object865480;
extern CareerState861E7C careerState861E7C;
extern HudFlagHolder hudHolder85B940;
extern GameSession89E7A0 gameSession89E7A0;
extern ReplayRecorder replayRecorder;
extern ProfileData83A9D0 profileData83A9D0;
extern PlayerSettings83AA64 playerSettings83AA64[2];
extern unsigned char mirror83AA18;
extern unsigned char raceMap83AA1B;
extern unsigned char exploreMap83AA1C;
extern int settings83AAB4;
extern int gameMode850078;
extern int value850088;
extern unsigned char value83A7A4;
extern int value89E818;
extern unsigned char cameraWarned89E82C;
extern int gameFlag890104;
extern int gameFlag890108;
extern int gameFlag89010C;
extern int frameTotal7FB720;
extern int drivetrainClock;
extern float drivetrainClockScale;  // 0x784268
extern float worldFramePeriod;      // 0x7875bc, 1/60
extern float hudZero7D7D3C;
extern float replayFinishDelay7A5700; // 3.0
extern float restartWindow7A5498;   // 3.0
extern float restartLimit7844F8;    // 4.0

// Original 0x401860 on the car state (+0x40): the +0x414 timer in seconds.
float DrivetrainAuxiliary::GetTimer414Seconds() {
    if (timer414 != 0 && timer414 != 0x7fffffff) return (float)timer414 * drivetrainClockScale;
    return hudZero7D7D3C;
}

// Original 0x401910.
bool GameSession89E7A0::Query401910() {
    return flag39 != 0 || gameFlag890104 != 0;
}

// Original 0x401930.
bool GameSession89E7A0::Query401930() {
    return flag43 != 0 || gameFlag89010C != 0;
}

// Original 0x5ed990.
bool GameSession89E7A0::Query5ED990() {
    if (raceType10 == 0) return true;
    if (raceType10 == 1 && value1C == 0) return true;
    return false;
}

// Original 0x5ea190.
int GameFlow8900D8::Query5EA190() {
    if (state0 == 6 && (value24 == 2 || value24 == 3)) return 1;
    return 0;
}

// Original 0x457ba0.
bool ProfileData83A9D0::Query457BA0() {
    if (value156A8 == 4 && valueE4 == 8) return true;
    return false;
}

// Original 0x5012c0.
bool Object85EF50::Flag5012C0() { return flag1C; }

// Original 0x5012d0.
bool Object85EF50::Flag5012D0() { return flag1D; }

// Original 0x510050.
bool Debug510050() { return value83A7A4; }

// Original 0x501690.
bool HudFlagHolder::IsSet() { return value8 != 0; }

// Original 0x501680.
bool HudFlagHolder::IsCashSuppressed() { return value11C == 1; }

// Original 0x502420.
bool CareerState861E7C::Query502420() {
    if (state0 == 0 || state0 == 9) return false;
    return true;
}

// Original 0x502440.
bool CareerState861E7C::Query502440() {
    if (state0 == 0 || state0 == 9) return false;
    return state0 == 2;
}

// Original 0x502460.
bool CareerState861E7C::Query502460() {
    if (state0 == 0 || state0 == 9) return false;
    if (state0 == 5 && flag8) return false;
    return state0 > 2;
}

// Original 0x502490.
bool CareerState861E7C::Query502490() {
    if (state0 == 0 || state0 == 9) return false;
    return state0 >= 6;
}

// Original 0x5024b0.
bool CareerState861E7C::Query5024B0() {
    if (state0 == 0 || state0 == 9) return false;
    return state0 > 8;
}

// Original 0x5f0880.
int ReplayRecorder::QueryState5F0880() {
    if (state0 == 6) {
        int *state = state4;
        if ((*state == 4 || *state == 6 || *state == 0 || *state == 5) && *state != 6) {
            if (tick58 != 0 && tick58 != 0x7fffffff) {
                int elapsed = drivetrainClock - tick58;
                if ((float)elapsed * drivetrainClockScale > replayFinishDelay7A5700) return 1;
            }
            return 0;
        }
        return 1;
    }
    return state0 == 0;
}

// Original 0x571250; the receiver is not read.
bool Object865480::Query571250() {
    if (replayRecorder.QueryState5F00F0()) return false;
    if (gameFlow8900D8 && gameFlow8900D8->state0 == 6) return true;
    return false;
}

// Original 0x5f85b0: replay state, or whether every player has a restart frame.
int RaceStatus890118::Query5F85B0() {
    if (replayRecorder.QueryState5F00F0()) return replayRecorder.QueryState5F0880();
    for (int i = 0; i < playerCount8900BC; ++i) {
        if (players8900B4[i]->value50 == 0) return 0;
    }
    return 1;
}

// Original 0x5fca90: clears every player's restart frame and resets each car's race object.
void RaceStatus890118::Reset5FCA90() {
    int mode = gameMode850078;
    if (mode == 4) return;
    if ((mode == 0x200 || mode == 8) && value850088 == 8) return;
    for (int i = 0; i < playerCount8900BC; ++i) {
        PlayerRecord *player = players8900B4[i];
        player->value50 = 0;
        MoverModeOwner *car = player->car4;
        MoverOwnerObject *object = car ? car->object1C : 0;
        if (object) object->Reset5FC2A0(car);
    }
    value28 = 0;
}

// Original 0x5ea8c0.
unsigned __int64 PlayerRecord::GetScoreMask() {
    unsigned __int64 mask = 0;
    if (playerSettings83AA64[index10].value3) mask = 0x20000;
    return mask;
}

// Original 0x5ea8e0.
unsigned __int64 PlayerRecord::GetMapMask() {
    int raceType = gameSession89E7A0.raceType10;
    unsigned __int64 mask = 0;
    if (raceType ? raceMap83AA1B : exploreMap83AA1C) mask = 1;
    if (playerCount8900BC == 2 && index10 == 1) mask &= ~(unsigned __int64)1;
    if (raceType == 0) mask |= (unsigned __int64)2 << 32;
    return mask;
}

// Original 0x5ea930.
int PlayerRecord::GetCameraMask() {
    CameraMover *camera = camera238;
    if (camera && camera->active14) {
        int kind = camera->kindC;
        if (kind == 1 || state58 == 6 || state58 == 7 || kind == 0x12) return 0x40;
        if (!cameraWarned89E82C) cameraWarned89E82C = 1;
    }
    return 0;
}

// Original 0x5f32a0. The +0x414 timer reads are 0x401860 inlined.
unsigned __int64 PlayerRecord::GetGaugeMask() {
    unsigned int low = 0;
    unsigned int high = 0;
    if (playerSettings83AA64[index10].value0) {
        __int64 camera = GetCameraMask();
        low = (unsigned int)camera;
        high = (unsigned int)(camera >> 32);
        unsigned char drift = gameSession89E7A0.flag39;
        int driftOverride = gameFlag890104;
        if (!drift && !driftOverride && !gameSession89E7A0.flag43 && !gameFlag89010C) {
            if (car4->State()->GetTimer414Seconds() > hudZero7D7D3C) high |= 0x40000;
        }
        if (gameSession89E7A0.flag38 || gameFlag890108) {
            low |= 0x20000000;
        } else if (drift || driftOverride) {
            low |= 0x80000000;
        } else if (gameMode850078 == 4 && settings83AAB4 == 8) {
            low |= 0x400;
            high |= 0x40;
        }
        if (car4->State()->GetTimer414Seconds() > hudZero7D7D3C) high |= 0x40000;
    }
    return (unsigned __int64)high << 32 | low;
}

// Original 0x5f33b0.
unsigned __int64 PlayerRecord::GetRaceMask() {
    unsigned int low = 0;
    unsigned int high = 0;
    int raceType = gameSession89E7A0.raceType10;
    if (raceType) {
        PlayerSettings83AA64 *settings = &playerSettings83AA64[index10];
        if (settings->value2) low = 0x508;
        unsigned char drift = gameSession89E7A0.flag39;
        int driftOverride = gameFlag890104;
        if (settings->value14 != 4 && !drift && !driftOverride) low |= 0x80;
        if (settings->value1) {
            if (gameSession89E7A0.counts20[0] + gameSession89E7A0.counts20[1] + gameSession89E7A0.counts20[2]
                    + gameSession89E7A0.counts20[3] == 1
                || (raceType == 1 && gameSession89E7A0.value1C == 0)) {
                if ((drift || driftOverride) && (raceType != 2 || gameSession89E7A0.value14 != 0)) low |= 0x200;
            } else {
                low |= 0x200;
            }
        }
        if (settings->value5) {
            if (value89E818 == 0 || value89E818 == 0x7fffffff) {
                if (raceType != 7 && !gameSession89E7A0.Query5ED990() && !gameSession89E7A0.Query4018F0())
                    low |= 0x800000;
            }
        }
        if (raceType == 7) high |= 0x40;
        if (raceType != 1 || gameSession89E7A0.value1C != 0) low |= 0x4000;
    }
    return (unsigned __int64)high << 32 | low;
}

// Original 0x5f34c0.
unsigned __int64 PlayerRecord::GetAmbientMask() {
    if (car4->object1C->value10 != 0 && gameSession89E7A0.raceType10 != 0
        && !gameSession89E7A0.flag38 && !gameFlag890108)
        return 0;
    int flow = gameFlow8900D8->state0;
    if (flow == 6) return 0;
    unsigned int low = 0;
    unsigned int high = 0;
    if (flow != 7 && value234 == 0) high = 0x20000;
    if (raceMap83AA1B) low |= 1;
    unsigned __int64 gauge = GetGaugeMask();
    low |= (unsigned int)gauge | 0x4800;
    high |= (unsigned int)(gauge >> 32);
    if (gameSession89E7A0.flag39 || gameFlag890104) low |= 0x20000;
    return (unsigned __int64)high << 32 | low;
}

// Original 0x5fff50: whether a restart request (+0x50) is still pending.
bool PlayerRecord::QueryRestart() {
    bool pending = false;
    if (value50 == 0) return pending;
    if (gameSession89E7A0.raceType10 == 0 && !object865480.Query571250()) {
        float elapsed = ((float)frameTotal7FB720 - (float)value50) * worldFramePeriod;
        float clamped = elapsed > hudZero7D7D3C ? elapsed : hudZero7D7D3C;
        if (restartWindow7A5498 < clamped) clamped = restartWindow7A5498;
        if (elapsed == clamped) return true;
        raceStatus890118->Reset5FCA90();
        return false;
    }
    if (gameSession89E7A0.raceType10 == 7 && replayRecorder.QueryState5F00F0()) {
        bool waiting;
        if (((float)frameTotal7FB720 - (float)value50) * worldFramePeriod < restartLimit7844F8) {
            waiting = true;
            if (raceStatus890118->Query5F85B0() == 0) return true;
        } else {
            waiting = false;
        }
        if (!waiting) flag24A = 1;
        return false;
    }
    pending = gameFlow8900D8->state0 == 4;
    if (gameSession89E7A0.flag46 && !gameSession89E7A0.flag38 && !gameFlag890108) pending = false;
    return pending;
}

// Original 0x600090: the HUD feature masks for this player (low word in EAX, high in EDX).
unsigned __int64 PlayerRecord::GetHudMask() {
    if (car4 == 0) return 0;
    if (camera238) {
        switch (camera238->kindC) {
        case 2:
        case 3:
            return 1;
        case 4:
        case 0x12:
            return 0;
        }
    }
    if (state58 == 1 || (state58 > 5 && state58 <= 7)) return 0;
    if (value234 && gameFlow8900D8->state0 != 4) return 0;
    if (value82A3A4 > 0) return 0;
    if (object85EF50.Flag5012C0()) return 0;
    if (object85EF50.Flag5012D0()) return 0;
    if (gameSession89E7A0.flag46 && Debug510050()) return 0;
    unsigned __int64 mask = 0x40000;
    if (state58 == 0 || state58 == 6 || state58 == 7) {
        int flow = gameFlow8900D8->state0;
        if (flow == 5 || flow == 1 || flow == 7) {
            mask = 0;
        } else {
            int race = raceStatus890118->state0;
            if (race == 1 || race == 2) {
                mask = 0x2000000;
            } else if (race == 3 && playerCount8900BC == 1) {
                if (gameSession89E7A0.flag46) return GetAmbientMask();
                if (flow != 6) {
                    mask = GetScoreMask() | 0x40000;
                    if (playerSettings83AA64[index10].value0) {
                        mask |= (__int64)GetCameraMask();
                        if (!gameSession89E7A0.Query401910() && !gameSession89E7A0.Query401930()
                            && car4->State()->GetTimer414Seconds() > hudZero7D7D3C)
                            mask |= (unsigned __int64)0x40000 << 32;
                        if (gameSession89E7A0.Query4018F0()) mask |= 0x20000000;
                    }
                }
            } else if (QueryRestart()) {
                mask = 0x40004;
                if (playerCount8900BC == 2 && index10 == 0
                    && raceStatus890118->value24 != raceStatus890118->value28 && raceMap83AA1B)
                    mask = 0x40005;
            } else if (flag24A) {
                mask = 0x2000000;
            } else if (gameFlow8900D8->state0 == 6) {
                if (gameFlow8900D8->Query5EA190()) mask = 0x2040000;
                if (replayRecorder.QueryState5F00F0()) mask |= 0x4000;
            } else if (gameMode850078 == 1 && careerState861E7C.Query502420()) {
                if (careerState861E7C.Query502440()) mask = (unsigned __int64)2 << 32 | 0x40000;
                if (careerState861E7C.Query502460()) mask |= (unsigned __int64)2 << 32;
                if (careerState861E7C.Query502490()) mask |= 1;
                if (careerState861E7C.Query5024B0()) mask |= 0x60;
            } else if (state58 == 1 || state58 == 2) {
                mask = 0x42000;
            } else {
                bool checkPlayers = true;
                switch (mode28) {
                case 0: {
                    if (gameSession89E7A0.flag46) return GetAmbientMask();
                    unsigned __int64 base = (unsigned __int64)0x80 << 32 | 0x140000;
                    if (camera238 && camera238->Query28()) base = (unsigned __int64)0x80 << 32 | 0x40000;
                    mask = base | (GetMapMask() | GetRaceMask() | GetScoreMask());
                    mask |= GetGaugeMask();
                    if (mirror83AA18 && !flag249) {
                        int camera = playerSettings83AA64[index10].value10;
                        if (camera == 0 || camera == 1) mask |= 0x1000;
                    }
                    if (hudHolder85B940.IsSet() && hudHolder85B940.IsCashSuppressed())
                        mask |= (unsigned __int64)4 << 32;
                    break;
                }
                case 1:
                    if (playerCount8900BC == 1) {
                        if (gameSession89E7A0.raceType10 ? raceMap83AA1B : exploreMap83AA1C) mask = 0x40001;
                        checkPlayers = false;
                    }
                    break;
                case 3:
                    mask = (__int64)(GetCameraMask() | 0x40000);
                    break;
                }
                if (checkPlayers && playerCount8900BC == 2) {
                    if (mask & 0x4000000) {
                        mask &= ~(unsigned __int64)0x4000000;
                        mask |= 0x8000;
                    }
                    mask &= ~(unsigned __int64)0x100000;
                }
                if (gameMode850078 == 1 && gameSession89E7A0.raceType10 == 0)
                    mask |= (unsigned __int64)0x80000 << 32;
            }
        }
    }
    int flow = gameFlow8900D8->state0;
    if (flow != 7 && (gameSession89E7A0.raceType10 != 0 || profileData83A9D0.Query457BA0()) && flow != 6
        && value234 == 0)
        mask |= (unsigned __int64)0x20000 << 32;
    return mask;
}
