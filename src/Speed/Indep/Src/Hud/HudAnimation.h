#ifndef UG2_HUD_ANIMATION_H
#define UG2_HUD_ANIMATION_H

// Original FEng interpolation primitives used by free-roam message tracks.
void HudLerpVector3(const float *a, const float *b, float fraction, const float *base, float *out); // 0x505150
void HudLerpColour(const int *a, const int *b, float fraction, const int *base, int *out); // 0x5051a0

#endif
