#ifndef UG2_FORCE_FEEDBACK_H
#define UG2_FORCE_FEEDBACK_H

#include "Ug2Layout.h"

// Force-feedback device manager at *0x870760. Its DirectInput effect methods
// are not recovered; names keep their original addresses and observed
// argument shapes. Natively the pointer is null and none of them is reached.
class ForceFeedbackManager {
public:
    bool HasDevice5BFD70(int device);
    bool IsEffectActive5BFD90(int device, int effect);
    void Stop5BFE00(int device);
    void Stop5BFE70(int device);
    void Play5C9150(int device, int effect, int magnitude, int duration);
    bool IsEffectActive5C9040(int device, int effect);
    void SetConstant5C9550(int device, int magnitude, int direction);
    void SetSpring5C9590(int device, int magnitude);
    void PlayJolt5C9760(int device, int magnitude);
    void SetRumble5C9D90(int device, int magnitude);
    void SetSurface5C9FD0(int device, int kind, int level, int parameter);
    void Stop5CD850(int device);
    void Stop5CD910(int device);
    void Stop5CD9D0(int device);
    void Start5CDA10(int device);
    void Stop5CDBB0(int device);
};

extern ForceFeedbackManager *forceFeedbackManager870760;

// Game session object at 0x89e7a0; only +0x38 is observed here.
struct GameSession89E7A0 {
    char unidentified0[0x10];
    int raceType10;       // 0x89e7b0, 0 in free roam
    int value14;
    char unidentified18[4];
    int value1C;
    int counts20[4];      // summed by 0x5f33b0
    char unidentified30[8];
    unsigned char flag38;
    unsigned char flag39;
    char unidentified3A[9];
    unsigned char flag43;
    char unidentified44[2];
    unsigned char flag46; // 0x89e7e6
    bool Query4018F0(); // 0x4018f0
    bool Query401910(); // 0x401910
    bool Query401930(); // 0x401930
    bool Query401950(); // 0x401950
    bool Query5ED990(); // 0x5ed990
};
UG2_CHECK_OFFSET(GameSession89E7A0, raceType10, 0x10);
UG2_CHECK_OFFSET(GameSession89E7A0, counts20, 0x20);
UG2_CHECK_OFFSET(GameSession89E7A0, flag38, 0x38);
UG2_CHECK_OFFSET(GameSession89E7A0, flag43, 0x43);
UG2_CHECK_OFFSET(GameSession89E7A0, flag46, 0x46);

#endif
