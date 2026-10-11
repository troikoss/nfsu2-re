#include "TimedAverage.h"

extern "C" void * __cdecl malloc(unsigned int size);
extern "C" void __cdecl free(void *memory);
extern char sampleAverageVtable[];
extern char timedAverageVtable[];
extern char averageBaseVtable[];

// Original 0x5768e0, non-deleting destructor.
void SampleAverage::Destroy() {
    vtable0 = sampleAverageVtable;
    if (samples10 != inlineSamples14) free(samples10);
    vtable0 = averageBaseVtable;
}

// Original 0x576a60 includes the base destruction in its body.
void TimedAverage::DestroyTimed() {
    vtable0 = timedAverageVtable;
    free(times30);
    vtable0 = sampleAverageVtable;
    if (samples10 != inlineSamples14) free(samples10);
    vtable0 = averageBaseVtable;
}

// Original 0x579f20. The full integer capacity controls allocation/clearing;
// the original object separately stores its low byte at +5.
SampleAverage *SampleAverage::Construct(int capacity) {
    count6 = 0;
    next7 = 0;
    sum8 = 0.0f;
    averageC = 0.0f;
    value4 = 4;
    vtable0 = sampleAverageVtable;
    capacity5 = (unsigned char)capacity;
    samples10 = inlineSamples14;
    if (capacity > 5) samples10 = (float *)malloc(capacity * 4);
    for (int i = 0; i < capacity; ++i) samples10[i] = 0.0f;
    return this;
}

// Original 0x579f80. Allocation is the original malloc boundary; no invented
// capacity clamping or zero-capacity fallback is introduced.
TimedAverage *TimedAverage::ConstructTimed(float window, float frequency) {
    Construct((int)(window * frequency + 0.5f));
    window28 = window;
    vtable0 = timedAverageVtable;
    oldest2C = 0;
    times30 = (float *)malloc(capacity5 * 4);
    for (int i = 0; i < capacity5; ++i) times30[i] = 0.0f;
    return this;
}

// Original 0x576aa0. Zero time identifies an unused slot, independently of
// zero sample values. Expiry is strictly greater than the window.
void TimedAverage::Advance(float value, float time) {
    if (samples10[next7] == 0.0f && times30[next7] == 0.0f) {
        ++count6;
    } else {
        sum8 = sum8 - samples10[next7];
    }
    sum8 = sum8 + value;
    samples10[next7] = value;
    times30[next7] = time;
    while (time - times30[oldest2C] > window28) {
        if (times30[oldest2C] > 0.0f) {
            sum8 = sum8 - samples10[oldest2C];
            samples10[oldest2C] = 0.0f;
            times30[oldest2C] = 0.0f;
            --count6;
        }
        ++oldest2C;
        if (oldest2C >= capacity5) oldest2C = 0;
    }
    averageC = sum8 / count6;
    ++next7;
    if (next7 >= capacity5) next7 = 0;
}
