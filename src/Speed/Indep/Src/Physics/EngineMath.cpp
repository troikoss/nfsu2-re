#include "Engine.h"

// Shared original helpers 0x401070 and 0x401090, not replacement CRT formulas.
float EngineMinimum(float a, float b) {
    return a < b ? a : b;
}

// Original 0x401050; equal and unordered operands return b.
float EngineMaximum(float a, float b) {
    return a > b ? a : b;
}

float EngineClamp(float value, float minimum, float maximum) {
    float result = value > minimum ? value : minimum;
    return maximum < result ? maximum : result;
}

// Original 0x43c2b0. Preserve angle quantization and polynomial evaluation.
float EngineSineUnits(unsigned int units) {
    float x = (units & 0xffffu) * engineAngleUnit;
    float sign = 1.0f;
    if (x >= engineThreeHalfPi) {
        x -= engineTwoPi;
    } else if (x >= engineHalfPi) {
        x -= enginePi;
        sign = -1.0f;
    }
    float x2 = x * x;
    float x3 = x2 * x;
    float x5 = x3 * x2;
    float x7 = x5 * x2;
    float x9 = x7 * x2;
    return (x - x3 * engineSineC3 + x5 * engineSineC5 - x7 * engineSineC7 + x9 * engineSineC9) * sign;
}

// Original 0x43c350.
float EngineSine(float angle) {
    return EngineSineUnits((int)(angle * engineAngleScale));
}
