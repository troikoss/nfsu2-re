#ifndef UG2_TIMED_AVERAGE_H
#define UG2_TIMED_AVERAGE_H

#include "../../Src/Physics/Ug2Layout.h"

// Original explicit layouts. Vtable identities are retained as data; native
// ownership/freeing belongs to the platform backend, not these recovered calls.
struct SampleAverage {
    void *vtable0;
    unsigned char value4;
    unsigned char capacity5;
    unsigned char count6;
    unsigned char next7;
    float sum8;
    float averageC;
    float *samples10;
    float inlineSamples14[5];

    SampleAverage *Construct(int capacity); // 0x579f20
    void Destroy(); // 0x5768e0
};

struct TimedAverage : SampleAverage {
    float window28;
    int oldest2C;
    float *times30;

    TimedAverage *ConstructTimed(float window, float frequency); // 0x579f80
    void Advance(float value, float time); // 0x576aa0
    void DestroyTimed(); // 0x576a60
};

UG2_CHECK_SIZE(SampleAverage, 0x28);
UG2_CHECK_OFFSET(SampleAverage, samples10, 0x10);
UG2_CHECK_SIZE(TimedAverage, 0x34);
UG2_CHECK_OFFSET(TimedAverage, times30, 0x30);

#endif
