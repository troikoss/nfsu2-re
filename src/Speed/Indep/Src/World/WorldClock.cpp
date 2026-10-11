// Original 0x57a0e0: per-frame world clock. Converts the frame duration into whole 60 Hz
// frames (keeping the remainder) and 4 kHz simulation ticks (0x86518c).

extern float frameDuration865154;
extern float frameRemainder865158;
extern int frameCount865150;
extern int frameTotal7FB720;
extern int frameTotalCopy7FB71C;
extern int worldTicked7FB728;
extern int drivetrainClock;
extern int collisionFrame7FB724;
extern float ambientTime7FB718;
extern float worldFrameRate;
extern float worldFramePeriod;
extern float worldTickRate;
extern float worldTickRounding;
extern float worldTickScale;

void AdvanceWorldClock() {
    worldTicked7FB728 = 1;
    if (frameDuration865154 == 0.0f) {
        return;
    }
    float frames = frameRemainder865158 + frameDuration865154;
    int whole = (int)(worldFrameRate * frames);
    frameTotal7FB720 = frameTotal7FB720 + whole;
    frameCount865150 = whole;
    frameTotalCopy7FB71C = frameTotal7FB720;
    frameRemainder865158 = frames - (float)whole * worldFramePeriod;
    drivetrainClock = drivetrainClock + (int)(frameDuration865154 * worldTickRate + worldTickRounding);
    ++collisionFrame7FB724;
    frameDuration865154 = 0.0f;
    ambientTime7FB718 = (float)drivetrainClock * worldTickScale;
}
