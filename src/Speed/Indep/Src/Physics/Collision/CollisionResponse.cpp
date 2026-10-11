#include "Collision.h"

extern "C" double __cdecl sqrt(double);
#pragma intrinsic(sqrt)
extern "C" void *malloc(unsigned int size);
extern "C" void free(void *memory);
#include "../VehicleMover.h"

// Contact resolution: rewind both bodies to the impact time, apply the impulse, re-advance.

extern int collisionClock;
extern float collisionMinimumImpulseMass;
extern float collisionFrictionRampSpeed;
extern float collisionFrictionMinimumSpeed;
extern float collisionWallNormalLimit;
extern float collisionWallFrictionZ;
extern float collisionRestingScale;
extern float collisionUnbreakable;

// Original 0x5b2610.
void CollisionContact::Resolve() {
    bodyAC->OnContact(bodyB10);
    bodyB10->OnContact(bodyAC);
    bodyAC->Rewind(time8);
    bodyB10->Rewind(time8);
    WheelVector positionA = *bodyAC->GetPosition();
    WheelVector positionB = *bodyB10->GetPosition();
    CollisionBody *bodyA = bodyAC;
    CollisionBody *resting = 0;
    if (bodyA->type11 == 0 && bodyB10->type11 == 1
        && static_cast<DynamicCollisionBody *>(bodyB10)->wallContacts120 > 0) {
        resting = bodyA;
    } else if (bodyB10->type11 == 0 && bodyA->type11 == 1
               && static_cast<DynamicCollisionBody *>(bodyA)->wallContacts120 > 0) {
        resting = bodyB10;
    }
    if (bodyA->IsMovable()) {
        positionA.x = positionA.x - offset20.x;
        positionA.y = positionA.y - offset20.y;
        positionA.z = positionA.z - offset20.z;
        if (bodyAC == resting) {
            positionA.z = bodyAC->GetPosition()->z;
        }
    }
    if (bodyB10->IsMovable()) {
        positionB.x = positionB.x + offset20.x;
        positionB.y = offset20.y + positionB.y;
        positionB.z = offset20.z + positionB.z;
        if (bodyB10 == resting) {
            positionB.z = bodyB10->GetPosition()->z;
        }
    }
    bodyAC->SetPosition(&positionA);
    bodyB10->SetPosition(&positionB);
    bodyAC->SyncMover();
    bodyB10->SyncMover();
    ApplyImpulse();
    bodyAC->SyncMover();
    bodyB10->SyncMover();
    bodyAC->Advance(time8);
    bodyB10->Advance(time8);
}

// Original 0x5941c0 (and the restitution choice inside 0x5af0e0): a car against a track
// polygon uses the car's value, other pairs the larger one.
static float CombineFriction(CollisionBody *bodyA, CollisionBody *bodyB) {
    float a = bodyA->GetFriction(bodyB);
    float b = bodyB->GetFriction(bodyA);
    if (bodyA->type11 == 0 && bodyB->type11 == 3) {
        return a;
    }
    if (!(bodyB->type11 == 0 && bodyA->type11 == 3) && !(a <= b)) {
        return a;
    }
    return b;
}

static bool WallPolygon(CollisionBody *body) {
    if (body->type11 != 3) {
        return false;
    }
    float z = static_cast<TrackPolygonCollisionBody *>(body)->face60->normal70.z;
    return (z < 0.0f ? -z : z) < collisionWallNormalLimit;
}

// Removes the component along the car's up axis from a contact arm.
static void FlattenArm(CollisionBody *body, WheelVector *arm) {
    if (body->type11 != 0) {
        return;
    }
    MoverModeOwner *car = static_cast<CarCollisionBody *>(body)->car130;
    if (car->mode424 != 4) {
        return;
    }
    WheelMatrix *matrix = &car->State()->matrix50;
    float ux = matrix->elements[8];
    float uy = matrix->elements[9];
    float uz = matrix->elements[10];
    float along = ux * arm->x + uz * arm->z + uy * arm->y;
    float x = ux * along;
    float y = uy * along;
    float z = uz * along;
    arm->x = arm->x - x;
    arm->y = arm->y - y;
    arm->z = arm->z - z;
}

// Original 0x5af0e0: impulse with restitution and friction at the contact point. The
// normal impulse is at least 0.3 / largest inverse mass; friction opposes the tangential
// relative velocity, ramped in below 1 m/s.
void CollisionContact::ApplyImpulse() {
    WheelVector point;
    WheelVector normal;
    ContactFrame(&point, &normal);
    CollisionBodyWitness *witness = witness14;
    if (!witness->event244) {
        witness->CreateEvent(&point);
    }
    CollisionEvent *event = witness->event244;
    WheelVector *position = bodyAC->GetPosition();
    WheelVector armA;
    armA.x = point.x - position->x;
    armA.y = point.y - position->y;
    armA.z = point.z - position->z;
    position = bodyB10->GetPosition();
    WheelVector armB;
    armB.x = point.x - position->x;
    armB.y = point.y - position->y;
    armB.z = point.z - position->z;
    for (;;) {
        WheelVector velocityA;
        WheelVector angularA;
        WheelVector velocityB;
        WheelVector angularB;
        bodyAC->GetVelocities(&velocityA, &angularA);
        bodyB10->GetVelocities(&velocityB, &angularB);
        WheelVector spin;
        WheelCross(&spin, &angularA, &armA);
        WheelVector pointA;
        pointA.x = spin.x + velocityA.x;
        pointA.y = spin.y + velocityA.y;
        pointA.z = spin.z + velocityA.z;
        WheelCross(&spin, &angularB, &armB);
        WheelVector pointB;
        pointB.x = spin.x + velocityB.x;
        pointB.y = spin.y + velocityB.y;
        pointB.z = spin.z + velocityB.z;
        WheelVector relative;
        relative.x = pointB.x - pointA.x;
        relative.y = pointB.y - pointA.y;
        relative.z = pointB.z - pointA.z;
        float approach = relative.x * normal.x + normal.z * relative.z + normal.y * relative.y;
        WheelVector tangent;
        WheelScaleAdd(&tangent, &relative, &normal, -approach);
        if (!(approach < 0.0f)) {
            return;
        }
        CollisionBody *bodyA = bodyAC;
        CollisionBody *bodyB = bodyB10;
        float restitutionA = bodyA->GetRestitution(bodyB);
        float restitutionB = bodyB->GetRestitution(bodyA);
        float restitution;
        if (bodyA->type11 == 0 && bodyB->type11 == 3) {
            restitution = restitutionA;
        } else if ((bodyB->type11 != 0 || bodyA->type11 != 3) && !(restitutionA <= restitutionB)) {
            restitution = restitutionA;
        } else {
            restitution = restitutionB;
        }
        float inverseMassA = bodyAC->GetMass();
        float inverseMassB = bodyB10->GetMass();
        float target = -((restitution + 1.0f) * approach);
        WheelMatrix *inertia = bodyAC->GetInverseInertia();
        WheelVector torque;
        WheelVector turn;
        WheelVector reach;
        WheelCross(&torque, &armA, &normal);
        WheelTransform(&turn, inertia, &torque);
        WheelCross(&reach, &turn, &armA);
        float denominator = ((inverseMassB + inverseMassA) + reach.x * normal.x) + reach.z * normal.z
                          + reach.y * normal.y;
        inertia = bodyB10->GetInverseInertia();
        WheelCross(&torque, &armB, &normal);
        WheelTransform(&turn, inertia, &torque);
        WheelCross(&reach, &turn, &armB);
        denominator = (reach.x * normal.x + reach.z * normal.z + reach.y * normal.y) + denominator;
        float largest = inverseMassA <= inverseMassB ? inverseMassB : inverseMassA;
        float minimum = collisionMinimumImpulseMass / largest;
        float magnitude = target / denominator;
        if (magnitude < minimum) {
            magnitude = minimum;
        }
        WheelVector impulse;
        impulse.x = normal.x * magnitude;
        impulse.y = normal.y * magnitude;
        impulse.z = normal.z * magnitude;
        float shareB = bodyB10->Slot4C(&impulse, &armB);
        impulse.x = -impulse.x;
        impulse.y = -impulse.y;
        impulse.z = -impulse.z;
        float shareA = bodyAC->Slot4C(&impulse, &armA);
        if (shareA == collisionUnbreakable || shareB == collisionUnbreakable) {
            return;
        }
        int single = 1;
        if (shareA < 0.0f) {
            shareA = -shareA;
        } else if (shareB < 0.0f) {
            shareB = -shareB;
        } else {
            single = 0;
        }
        float share = shareA < shareB ? shareA : shareB;
        float applied = share * magnitude;
        float friction = CombineFriction(bodyAC, bodyB10);
        float sliding = (float)sqrt(tangent.x * tangent.x + tangent.z * tangent.z + tangent.y * tangent.y);
        friction = friction * applied;
        if (sliding < collisionFrictionRampSpeed) {
            friction = friction / collisionFrictionRampSpeed * sliding;
        }
        WheelVector drag;
        if (!(sliding <= collisionFrictionMinimumSpeed)) {
            float scale = -1.0f / sliding * friction;
            drag.x = tangent.x * scale;
            drag.y = tangent.y * scale;
            drag.z = tangent.z * scale;
        } else {
            drag.y = 0.0f;
            drag.z = 0.0f;
            drag.x = 0.0f;
        }
        WheelVector push;
        push.x = normal.x * applied;
        push.y = normal.y * applied;
        push.z = normal.z * applied;
        CollisionBodyWitness *owner = event->witness64;
        if (WallPolygon(owner->bodyA238) || WallPolygon(owner->bodyB23C)) {
            drag.z = drag.z * collisionWallFrictionZ;
        }
        WheelVector total;
        total.x = drag.x + push.x;
        total.y = drag.y + push.y;
        total.z = push.z + drag.z;
        WheelVector opposite;
        opposite.x = -total.x;
        opposite.y = -total.y;
        opposite.z = -total.z;
        event->Accumulate(&point, &opposite, sliding);
        if (event->value10) {
            FlattenArm(bodyAC, &armA);
            FlattenArm(bodyB10, &armB);
        }
        CollisionBody *resting = 0;
        if (bodyAC->type11 == 0 && bodyB10->type11 == 1
            && static_cast<DynamicCollisionBody *>(bodyB10)->wallContacts120 > 0) {
            resting = bodyAC;
        } else if (bodyB10->type11 == 0 && bodyAC->type11 == 1
                   && static_cast<DynamicCollisionBody *>(bodyAC)->wallContacts120 > 0) {
            resting = bodyB10;
        }
        if (bodyAC == resting) {
            float z = opposite.z * collisionRestingScale;
            opposite.z = 0.0f <= z ? z : 0.0f;
        } else if (bodyB10 == resting) {
            float z = total.z * collisionRestingScale;
            total.z = 0.0f <= z ? z : 0.0f;
        }
        bodyB10->ApplyImpulse(&total, &armB);
        bodyAC->ApplyImpulse(&opposite, &armA);
        if (share == 1.0f || single) {
            return;
        }
    }
}

// Original 0x5a9620 (accumulation part): sum the impulses of the event; the effects that
// follow (sparks 0x59fb70, sounds, camera shake and force feedback) are a platform seam.
void CollisionEvent::Accumulate(WheelVector *point, WheelVector *impulse, float speed) {
    float y = impulse->y;
    float z = impulse->z;
    impulse50.x = impulse50.x + impulse->x;
    impulse50.y = y + impulse50.y;
    impulse50.z = z + impulse50.z;
    float magnitude = (float)sqrt(impulse->y * impulse->y + impulse->z * impulse->z + impulse->x * impulse->x);
    magnitude60 = magnitude + magnitude60;
    tickC = collisionClock;
    position30 = *point;
    speed40 = speed;
    CollisionEffects(this, point, impulse, speed);
}
