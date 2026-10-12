#ifndef UG2_FORCE_FEEDBACK_H
#define UG2_FORCE_FEEDBACK_H

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
    char unidentified0[0x38];
    unsigned char flag38;
    bool Query4018F0(); // 0x4018f0
};

#endif
