// Platform scaffolding for the recovered collision-body world: seams for original routines
// reached from it that are not part of this recovery, and the unrecovered virtual slots
// (replay serialisation, cylinders, world objects), which stop the program if reached.
#include "NativeVehicle.h"
#include "../../Speed/Indep/Src/Physics/Collision/Collision.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

extern int drivetrainClock;
static int collisionClockTicks() {
    return drivetrainClock;
}

unsigned int nativeCollisionHullKey;

// bSPrintf (0x4400d0).
int bSPrintf(char *buffer, const char *format, ...) {
    va_list arguments;
    va_start(arguments, format);
    int written = vsprintf(buffer, format, arguments);
    va_end(arguments);
    return written;
}

// 0x6012b0 (car update): the PhysicsMover step (mover slot 4) is the part the demo runs;
// the rest of the car update (state machine, AI/replay hooks) is not recovered.
float UpdateCarForCollision(MoverModeOwner *car, float duration) {
    return car->mover34->Update(duration);
}

// 0x61c320: hash of the CarTypeInfo model name followed by "_CV". The native vehicle
// loader computes it for the stock part set (no body kit hulls).
unsigned int CarCollisionHullKey(PhysicsInfo *) {
    return nativeCollisionHullKey;
}

// 0x610310: alternative model key; the stock key is used again.
unsigned int CarCollisionBaseHullKey(PhysicsInfo *) {
    return nativeCollisionHullKey;
}

// 0x5f4960: tell the driver (AI/player participant) about a contact, or switch a car that
// has no physics mover to one. The demo car has neither case.
void NotifyCarContact(MoverModeOwner *car, MoverModeOwner *) {
    if (car->participant2C) {
        NativeTrap("participant contact slot 0x38 (0x5f4960)");
    }
    if (car->moverType28 != 1 && car->moverType28 != 3) {
        NativeTrap("mover switch 0x5f46a0");
    }
}

// Effects of 0x5a9620 after the impulse sums (sparks 0x59fb70, debris 0x5dc4c0, collision
// sounds 0x4814e0/0x4792d0, replay 0x56fb20, force feedback 0x5727b0): not recovered.
void CollisionEffects(CollisionEvent *event, WheelVector *point, WheelVector *impulse, float speed) {
    static int debug = -1;
    if (debug < 0) {
        debug = getenv("UG2_DEBUG_COLLISION") != 0;
    }
    if (!debug) {
        return;
    }
    CollisionBodyWitness *witness = event->witness64;
    WCollisionFace *face = 0;
    if (witness->bodyA238->type11 == 3) {
        face = static_cast<TrackPolygonCollisionBody *>(witness->bodyA238)->face60;
    } else if (witness->bodyB23C->type11 == 3) {
        face = static_cast<TrackPolygonCollisionBody *>(witness->bodyB23C)->face60;
    }
    fprintf(stderr, "collision t=%d %s/%s point=(%.2f %.2f %.2f) impulse=(%.1f %.1f %.1f) sliding=%.2f",
            collisionClockTicks(), witness->bodyA238->name30, witness->bodyB23C->name30, point->x, point->y,
            point->z, impulse->x, impulse->y, impulse->z, speed);
    if (face) {
        CollisionBody *car = witness->bodyA238->type11 == 3 ? witness->bodyB23C : witness->bodyA238;
        WheelVector *centre = car->GetPosition();
        float plane = face->normal70.x * centre->x + face->normal70.y * centre->y + face->normal70.z * centre->z
                    - face->distance7C;
        fprintf(stderr, " normal=(%.2f %.2f %.2f) surface=%d flags=%02x carPlane=%.2f", face->normal70.x,
                face->normal70.y, face->normal70.z, face->surfaceB, face->flagsA, plane);
    }
    fprintf(stderr, "\n");
}

// 0x637c70: force feedback for collisions; no device in the demo.
void CollisionForceFeedback(MoverModeOwner *, WheelVector *, float) {
}

// 0x483e20 / 0x483e40: collision sound emitter registration (only with audio, 0x7faaa8).
void RegisterCollisionEvent(CollisionEvent *) {
}

void UnregisterCollisionEvent(CollisionEvent *) {
}

// 0x5e0360: game message for cop/racer contacts; no message queue in the demo.
void PostCarCollisionMessage(int) {
}

int WorldObjectKind(CollisionBody *) {
    NativeTrap("world object collision body slot 0x8c");
    return 0;
}

CollisionBody *PromoteWorldObject(CollisionBody *) {
    NativeTrap("world object promotion 0x5b31f0");
    return 0;
}

int BreakWorldObject(CollisionBody *, CollisionBody **) {
    NativeTrap("world object break 0x5ae000");
    return 0;
}

void MoverVelocitiesAt(MoverBody *, WheelVector *, WheelVector *, int, int) {
    NativeTrap("mover body point velocity 0x59bec0");
}

float MoverSlot68(MoverBody *, int, int) {
    NativeTrap("mover body energy 0x59bfd0");
    return 0.0f;
}

float CarSlot88(CarCollisionBody *) {
    NativeTrap("car collision body 0x5a08b0");
    return 0.0f;
}

void CollisionBody::Serialise(void *) {
    NativeTrap("collision body serialisation 0x593cc0");
}

void DynamicCollisionBody::Serialise(void *) {
    NativeTrap("dynamic collision body serialisation 0x593120");
}

void CarCollisionBody::Serialise(void *) {
    NativeTrap("car collision body serialisation 0x5a9e80");
}

void TrackPolygonCollisionBody::Serialise(void *) {
    NativeTrap("track polygon collision body serialisation 0x593d10");
}

void CollisionCylinder::GetVertex(int, CollisionVertex *) {
    NativeTrap("cylinder shape vertex 0x5a2780");
}

CollisionVertex *CollisionCylinder::Support(WheelVector *, CollisionVertex *, WheelMatrix *, WheelMatrix *,
                                            CollisionVertex *output) {
    NativeTrap("cylinder shape support 0x5a23d0");
    return output;
}
