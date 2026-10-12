#ifndef UG2_HUD_PLAYER_MASKS_H
#define UG2_HUD_PLAYER_MASKS_H

// The HUD feature masks a player record produces for the UI controller (0x4ca090).
// Low and high mask words travel in EAX:EDX.
#include "../Physics/VehicleMover.h"

// Camera mover base (constructor 0x44ab90). Kind +0xc 1 is the chase mover from 0x451660
// (vtable 0x787f28, slot +0x28 = 0x404800); 0x444040 sets +0x14 when the mover is attached.
class CameraMover {
public:
    virtual void Virtual00() = 0;
    virtual void Virtual04() = 0;
    virtual void Virtual08() = 0;
    virtual void Virtual0C() = 0;
    virtual void Virtual10() = 0;
    virtual void Virtual14() = 0;
    virtual void Virtual18() = 0;
    virtual void Virtual1C() = 0;
    virtual void SetType20(int type) = 0; // slot +0x20, 0x5f2ff7
    virtual void Virtual24() = 0;
    virtual bool Query28() = 0;
    char unidentified4[8];
    int kindC;
    int value10;
    int active14;
};
UG2_CHECK_OFFSET(CameraMover, kindC, 0xc);
UG2_CHECK_OFFSET(CameraMover, active14, 0x14);

// Player record published at 0x8900ac/0x8900b4; only fields read by the mask producer are named.
struct PlayerRecord {
    char unidentified0[4];
    MoverModeOwner *car4;
    int player8;
    char unidentifiedC[4];
    int index10;          // row of the player settings at 0x83aa64
    char unidentified14[0x14];
    signed char mode28;
    char unidentified29[0x27];
    int value50;          // frame of the last restart request
    char unidentified54[4];
    int state58;
    char unidentified5C[0x1d8];
    int value234;
    CameraMover *camera238;
    char unidentified23C[0xd];
    unsigned char flag249;
    unsigned char flag24A;
    char unidentified24B[5];

    unsigned __int64 GetHudMask();            // 0x600090
    unsigned __int64 GetScoreMask();          // 0x5ea8c0
    unsigned __int64 GetMapMask();            // 0x5ea8e0
    int GetCameraMask();                      // 0x5ea930
    unsigned __int64 GetGaugeMask();          // 0x5f32a0
    unsigned __int64 GetRaceMask();           // 0x5f33b0
    unsigned __int64 GetAmbientMask();        // 0x5f34c0
    bool QueryRestart();                      // 0x5fff50
};
UG2_CHECK_OFFSET(PlayerRecord, index10, 0x10);
UG2_CHECK_OFFSET(PlayerRecord, mode28, 0x28);
UG2_CHECK_OFFSET(PlayerRecord, value50, 0x50);
UG2_CHECK_OFFSET(PlayerRecord, state58, 0x58);
UG2_CHECK_OFFSET(PlayerRecord, value234, 0x234);
UG2_CHECK_OFFSET(PlayerRecord, camera238, 0x238);
UG2_CHECK_OFFSET(PlayerRecord, flag24A, 0x24a);
UG2_CHECK_SIZE(PlayerRecord, 0x250);

// Race status object at *0x890118 (constructed by 0x5fc730).
struct RaceStatus890118 {
    int state0;
    char unidentified4[0x20];
    int value24;
    int value28;
    int Query5F85B0();    // 0x5f85b0
    void Reset5FCA90();   // 0x5fca90
};
UG2_CHECK_OFFSET(RaceStatus890118, value24, 0x24);

// Object at 0x85ef50.
struct Object85EF50 {
    char unidentified0[0x1c];
    unsigned char flag1C;
    unsigned char flag1D;
    bool Flag5012C0();    // 0x5012c0
    bool Flag5012D0();    // 0x5012d0
};

// Career state at 0x861e7c.
struct CareerState861E7C {
    int state0;
    int value4;
    unsigned char flag8;
    bool Query502420();   // 0x502420
    bool Query502440();   // 0x502440
    bool Query502460();   // 0x502460
    bool Query502490();   // 0x502490
    bool Query5024B0();   // 0x5024b0
};

// Object at 0x85b940 (also read by the minimap through 0x501690).
struct HudFlagHolder {
    char unidentified0[8];
    int value8;
    char unidentifiedC[0x110];
    int value11C;
    bool IsSet();         // 0x501690
    bool IsCashSuppressed(); // 0x501680
};
UG2_CHECK_OFFSET(HudFlagHolder, value11C, 0x11c);

// Object at 0x865480; 0x571250 does not read it.
struct Object865480 {
    bool Query571250();   // 0x571250
};

bool Debug510050();       // 0x510050

#endif
