#include "PauseOptions.h"

extern unsigned char feedbackDevice87078C;

// Original 0x5b8240: whether a force-feedback device is present.
unsigned char Value5B8240() {
    return feedbackDevice87078C;
}

// Original 0x5026f0: the current mode's setting byte.
int Settings83AAB4::Value5026F0() {
    if (mode0 == 12) return value100;
    return modeRows10[mode0][0];
}

// Original 0x502fe0.
unsigned char MemoryCard83A9E0::Value502FE0() {
    return value20;
}
