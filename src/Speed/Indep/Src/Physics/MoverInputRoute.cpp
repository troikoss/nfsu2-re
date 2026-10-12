#include "HumanDriverInput.h"

extern "C" double __cdecl fabs(double);
#pragma intrinsic(fabs)
extern "C" double __cdecl sqrt(double);
#pragma intrinsic(sqrt)

void Normalize2(float *output, float *input); // 0x43ce50
extern int drivetrainClock;
extern int *raceState890118;
extern float routeFastSpeed;         // 30
extern float routeFastCosine;        // 0x7837c4
extern float routeSlowCosine;        // 0x7837c8
extern float routeMinimumDistance2;  // 0x827a20
extern float routeMinimumAdvance;    // 0.001

// 0x43ce10: distance between two points, summed z, y, x; returned unspilled.
double bDistance(WheelVector *a, WheelVector *b) {
    double x = (double)a->x - b->x;
    double y = (double)a->y - b->y;
    double z = (double)a->z - b->z;
    return sqrt((z * z + y * y) + x * x);
}

typedef MoverInput::InputRoute InputRoute;

// 0x401890 / 0x4018a0: car position and velocity.
WheelVector *MoverModeOwner::GetPosition() { return &State()->position20; }
WheelVector *MoverModeOwner::GetVelocity() { return &State()->velocity30; }

// 0x405270: path length through the stored points.
void InputRoute::UpdateLength() {
    length1C4 = 0.0f;
    for (int i = 0; i < count0 - 1; ++i)
        length1C4 = (float)(bDistance(reinterpret_cast<WheelVector *>(points10[i]),
                                      reinterpret_cast<WheelVector *>(points10[i + 1])) + length1C4);
}

// 0x404ef0: first point at least sqrt(0x827a20) away within the heading cone
// (cosine 0x7837c4 above speed 30, else 0x7837c8); the last point otherwise.
// Cached per clock tick at +0x1b8.
int InputRoute::FindIndex(float speed, WheelVector *position, WheelVector *velocity) {
    if (drivetrainClock == tick1B8) return index1B4;
    if (count0 <= 0) {
        index1B4 = 0;
        tick1B8 = 0;
        return index1B4;
    }
    int found = count0 - 1;
    float cosine = speed > routeFastSpeed ? routeFastCosine : routeSlowCosine;
    float direction[2];
    Normalize2(direction, &velocity->x);
    for (int i = 0; i < count0; ++i) {
        float delta[2];
        delta[0] = (float)((double)points10[i][0] - position->x);
        delta[1] = (float)((double)points10[i][1] - position->y);
        float normal[2];
        Normalize2(normal, delta);
        if ((double)direction[1] * normal[1] + (double)normal[0] * direction[0] > cosine
                && (double)delta[0] * delta[0] + (double)delta[1] * delta[1] > routeMinimumDistance2) {
            found = i;
            break;
        }
    }
    index1B4 = (signed char)found;
    tick1B8 = drivetrainClock;
    return index1B4;
}

// 0x40f7a0 drops the first `count` points with their per-point values.
void InputRoute::RemoveFront(int count, bool recompute) {
    if (count0 <= count) return;
    for (int i = 0; i < count0 - count; ++i) {
        float y = points10[i + count][1];
        float z = points10[i + count][2];
        points10[i][0] = points10[i + count][0];
        points10[i][1] = y;
        points10[i][2] = z;
        values150[i] = values150[i + count];
        values178[i] = values178[i + count];
        values1A0[i] = values1A0[i + count];
    }
    count0 = (signed char)(count0 - count);
    tick1B8 = 0;
    value1BC = 0;
    if (recompute) UpdateLength();
    int index = index1B4 - count;
    index1B4 = (signed char)(index > 0 ? index : 0);
}

// 0x415590: index of the point at which the path from the car reaches
// `distance` (2D), starting from the cone point of 0x404ef0.
int InputRoute::FindPointAhead(MoverModeOwner *car, float distance) {
    WheelVector *position = &car->State()->position20;
    int index = FindIndex(car->speed42C, position, &car->State()->velocity30);
    float *point = points10[index];
    double x = (double)point[0] - position->x;
    double y = (double)point[1] - position->y;
    double length = sqrt(y * y + x * x);
    while (length < distance && index < count0) {
        double dx = (double)point[4] - point[0];
        ++index;
        double dy = (double)point[5] - point[1];
        point += 4;
        length = length + sqrt(dy * dy + dx * dx);
    }
    return index;
}

// 0x41d160 trims points the car has passed (relative to its velocity),
// keeps the points up to `distance` ahead, and resets the route when it is
// empty, the distance is not positive, or +0x1c9 is clear.
void InputRoute::Advance(MoverModeOwner *car, float distance) {
    if (count0 != 0) {
        if (fabs(distance) > routeMinimumAdvance && !(distance < 0.0f) && flag1C9) {
            int count = count0;
            int passed = 0;
            if (count > 0) {
                WheelVector *position = car->GetPosition();
                WheelVector *velocity = car->GetVelocity();
                float *point = points10[0];
                for (;;) {
                    double dx = (double)point[0] - position->x;
                    double dy = (double)point[1] - position->y;
                    if (dy * velocity->y + dx * velocity->x > 0.0) break;
                    ++passed;
                    point += 4;
                    if (passed >= count0) break;
                }
            }
            if (passed >= count) {
                count0 = 0;
                length1C4 = 0.0f;
            } else {
                if (passed > 0) RemoveFront(passed, false);
                int ahead = FindPointAhead(car, distance) + 1;
                if (ahead >= count0) ahead = count0;
                count0 = (signed char)ahead;
                UpdateLength();
            }
            flag1C8 = 0;
            flag1 = 0;
            tick1B8 = 0;
            value1BC = 0;
            return;
        }
        count0 = 0;
    }
    length1C4 = 0.0f;
    tick1C0 = drivetrainClock;
    flag1C9 = 0;
    flag1C8 = 0;
    flag1 = 0;
    tick1B8 = 0;
    value1BC = 0;
}

// 0x423330 without its vtable identity write (native C++ supplies the table).
MoverInput *MoverInput::ConstructBase() {
    route10.tick1B8 = 0;
    route10.value1BC = 0;
    route10.tick1C0 = 0;
    int *words = reinterpret_cast<int *>(&route10);
    for (int i = 0; i < 0x74; ++i) words[i] = 0;
    route10.Advance(0, 0.0f);
    steering1E0 = 0;
    gearRequest1E4 = 1;
    directGear1E8 = 1;
    throttle20C = 0.0f;
    handbrake210 = 0.0f;
    brake214 = 0.0f;
    route10.Advance(0, 0.0f);
    for (int i = 0; i < 4; ++i) {
        wheelTargets1FC[i] = 0.0f;
        wheelInputs1EC[i] = 0.0f;
    }
    return this;
}

// 0x425fc0 normal path without its vtable identity write and SEH frame.
HumanDriverInput *HumanDriverInput::ConstructHuman(MoverModeOwner *car) {
    ConstructBase();
    steering220.Construct(car);
    timer2CC.value0 = 0;
    wheelStamp300 = 0;
    car2A8 = car;
    device2AC = -1;
    *reinterpret_cast<int *>(&value2B0) = 0;
    ClearActions();
    flag2B4 = !(raceState890118 != 0 && *raceState890118 == 4);
    value2B8 = 0;
    *reinterpret_cast<int *>(&hold2BC) = 0;
    transmission2C0 = 0;
    gearRequest1E4 = 1;
    flag2F0 = 0;
    steeringEnabled2F8 = 1;
    wheelControl2F9 = 0;
    wheelRequest2FA = 0;
    wheelState2FC = 0;
    timer2CC.value0 = 0;
    clutch2F4 = 1.0f;
    return this;
}
