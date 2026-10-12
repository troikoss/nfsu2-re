#ifndef UG2_AUDIO_SYSTEM_H
#define UG2_AUDIO_SYSTEM_H

// The audio system object at *0x82b884. Only the members reached by recovered callers are named.
// 0x580130 clears the enable flag 0x7faaa8 when no sound device exists.
struct CollisionEvent;

struct CollisionSoundSet {
    void Add481CD0(CollisionEvent *event);    // not recovered
    void Remove481E50(CollisionEvent *event); // not recovered
};

struct Audio82B884 {
    char unidentified0[0x3c];
    CollisionSoundSet *collisions3C;
    void Apply(float *volumes);                    // 0x469050
    void RegisterCollision(CollisionEvent *event);   // 0x483e20
    void UnregisterCollision(CollisionEvent *event); // 0x483e40
};

#endif
