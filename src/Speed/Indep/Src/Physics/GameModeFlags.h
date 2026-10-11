#ifndef UG2_GAME_MODE_FLAGS_H
#define UG2_GAME_MODE_FLAGS_H

// Global race-mode flags tested together throughout the vehicle update.
// Their gameplay names are not yet established; names keep original addresses.
extern unsigned char gameFlag89E7D8;
extern unsigned char gameFlag89E7D9;
extern unsigned char gameFlag89E7E2;
extern unsigned char gameFlag89E7E3;
extern int gameFlag890100;
extern int gameFlag890104;
extern int gameFlag890108;
extern int gameFlag89010C;
extern int gameFlag890110;
extern int gameFlag890114;

// 0x5b4fc0 selects car-record variant 2 for this group.
inline bool IsModeGroup2() {
    return gameFlag89E7D8 != 0 || gameFlag890108 != 0;
}

// 0x5b4fc0 selects car-record variant 3 for this group.
inline bool IsModeGroup3() {
    return gameFlag89E7D8 == 0 && gameFlag890108 == 0
        && (gameFlag890114 != 0 || gameFlag89E7D9 != 0 || gameFlag890104 != 0);
}

inline bool IsModeGroupE2() {
    return gameFlag89E7E2 != 0 || gameFlag890100 != 0 || gameFlag890110 != 0;
}

#endif
