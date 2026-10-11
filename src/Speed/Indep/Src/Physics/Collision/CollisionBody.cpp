#include "Collision.h"

extern "C" double __cdecl sqrt(double);
#pragma intrinsic(sqrt)
extern "C" void *malloc(unsigned int size);
extern "C" void free(void *memory);
#include "../VehicleMover.h"

// Collision-body bookkeeping: the body list sorted by maximum X, the pair (witness) list,
// contact events and the per-frame world step.

extern int collisionClock;
extern float collisionClockScale;
extern float collisionEventLifetime;
extern int collisionIterations802080;
extern int collisionWitnessPasses802084;
extern float collisionContactOrderTolerance;

extern int collisionBodyCount86B250;
extern int collisionBodyPeak86B278;
extern int collisionWitnessCount86B248;
extern int collisionWitnessPeak86B270;
extern int collisionEventCount86B24C;
extern int collisionEventPeak86B274;
extern CollisionBodyWitness *collisionDebugWitness86B2B8;

CollisionBody *PromoteWorldObject(CollisionBody *body);
int BreakWorldObject(CollisionBody *body, CollisionBody **output);
void RegisterCollisionEvent(CollisionEvent *event);
void UnregisterCollisionEvent(CollisionEvent *event);
void PostCarCollisionMessage(int message);

static CollisionBody *BodyFromLink(WCollisionLink *link) {
    return reinterpret_cast<CollisionBody *>(reinterpret_cast<char *>(link) - 4);
}

static CollisionBodyWitness *WitnessFromLink(WCollisionLink *link) {
    return reinterpret_cast<CollisionBodyWitness *>(reinterpret_cast<char *>(link) - 0x230);
}

// Original 0x592df0: mark an id used and advance the first-free hint past used ids.
void CollisionIdAllocator::Take(int id) {
    if (next8 == id) {
        int i = id + 1;
        while (i < count0 && (bits4[i >> 3] & (unsigned char)(1 << (i & 7)))) {
            ++i;
        }
        next8 = i;
    }
    bits4[id >> 3] |= (unsigned char)(1 << (id & 7));
}

// Original 0x5a04f0.
CollisionBody::CollisionBody(int id, char type) {
    id14 = (short)id;
    type11 = type;
    shapeC = 0;
    ownsShape10 = 0;
    if (id >= 0) {
        collisionIds86B2BC->Take((short)id);
        collisionBodyTable86B298[id14] = this;
    }
    ++collisionBodyCount86B250;
    if (collisionBodyPeak86B278 < collisionBodyCount86B250) {
        collisionBodyPeak86B278 = collisionBodyCount86B250;
    }
    minimum20[0] = -9999999.0f;
    minimum20[1] = -9999999.0f;
    maximum28[0] = -9999999.0f;
    maximum28[1] = -9999999.0f;
    name30[0] = 0;
    value58 = 0;
    WCollisionLink *first = collisionBodies86B4C8.next;
    collisionBodies86B4C8.next = &link4;
    first->previous = &link4;
    link4.previous = &collisionBodies86B4C8;
    link4.next = first;
    disabled17 = 0;
    events18 = 0;
    witnesses16 = 0;
    active19 = 0;
}

// Original 0x5a9c00.
CollisionBody::~CollisionBody() {
    --collisionBodyCount86B250;
    UpdateLargestWidth(0.0f);
    WCollisionLink *next = link4.next;
    WCollisionLink *previous = link4.previous;
    previous->next = next;
    next->previous = previous;
    if (witnesses16) {
        WCollisionLink *link = collisionWitnesses86B490.next;
        while (link != &collisionWitnesses86B490) {
            CollisionBodyWitness *witness = WitnessFromLink(link);
            link = link->next;
            if (witness->bodyA238 == this || witness->bodyB23C == this) {
                delete witness;
            }
        }
    }
    if (id14 >= 0) {
        collisionBodyTable86B298[id14] = 0;
        CollisionIdAllocator *ids = collisionIds86B2BC;
        int id = id14;
        ids->bits4[id >> 3] &= (unsigned char)~(1 << (id14 & 7));
        if (id < ids->next8) {
            ids->next8 = id;
        }
    }
}

// Base vtable 0x7a1fe0 slots.
void CollisionBody::Slot44(int) {
}

int CollisionBody::Slot6C() {
    return 0;
}

void CollisionBody::Slot78(float, WheelVector *, WheelMatrix *, WheelVector *) {
}

// Original 0x59e590.
CollisionShape *CollisionBody::GetShape(CollisionBody *) {
    return shapeC;
}

// Original 0x5a0590.
void CollisionBody::AddEvent(CollisionEvent *) {
    ++events18;
}

// Original 0x5929c0.
void CollisionBody::RemoveEvent(CollisionEvent *) {
    --events18;
}

// Original 0x5a05a0: keep 0x86b29c at the largest body width along X.
void CollisionBody::UpdateLargestWidth(float width) {
    float current = maximum28[0] - minimum20[0];
    if (width == current) {
        return;
    }
    if (current < width && collisionLargestWidth86B29C < width) {
        collisionLargestWidth86B29C = width;
        return;
    }
    if (width < current && current == collisionLargestWidth86B29C) {
        collisionLargestWidth86B29C = 0.0f;
        for (WCollisionLink *link = collisionBodies86B4C8.next; link != &collisionBodies86B4C8;
             link = link->next) {
            CollisionBody *body = BodyFromLink(link);
            if (body != this) {
                float other = body->maximum28[0] - body->minimum20[0];
                if (collisionLargestWidth86B29C < other) {
                    collisionLargestWidth86B29C = other;
                }
            }
        }
    }
}

// Original 0x5a0680: store the XY bounds and keep the list sorted by (maximum X, id).
void CollisionBody::SetBounds(float *minimum, float *maximum) {
    UpdateLargestWidth(maximum[0] - minimum[0]);
    float previousMaximum = maximum28[0];
    minimum20[0] = minimum[0];
    minimum20[1] = minimum[1];
    maximum28[0] = maximum[0];
    maximum28[1] = maximum[1];
    if (previousMaximum == maximum[0]) {
        return;
    }
    for (;;) {
        WCollisionLink *before = link4.previous;
        WCollisionLink *after = link4.next;
        if (before != &collisionBodies86B4C8) {
            CollisionBody *body = BodyFromLink(before);
            if (!(body->maximum28[0] <= maximum28[0])
                || (!(body->maximum28[0] < maximum28[0]) && body->id14 > id14)) {
                // Move in front of the previous body.
                before->next = after;
                after->previous = before;
                WCollisionLink *earlier = before->previous;
                earlier->next = &link4;
                before->previous = &link4;
                link4.previous = earlier;
                link4.next = before;
                continue;
            }
        }
        if (after == &collisionBodies86B4C8) {
            return;
        }
        CollisionBody *body = BodyFromLink(after);
        if (maximum28[0] <= body->maximum28[0]) {
            if (maximum28[0] < body->maximum28[0]) {
                return;
            }
            if (id14 <= body->id14) {
                return;
            }
        }
        // Move behind the next body.
        before->next = after;
        after->previous = before;
        WCollisionLink *later = after->next;
        after->next = &link4;
        later->previous = &link4;
        link4.previous = after;
        link4.next = later;
    }
}

// Original 0x592960.
bool CollisionBody::Overlaps(CollisionBody *other, float margin) {
    return minimum20[0] <= margin + other->maximum28[0] && other->minimum20[0] - margin <= maximum28[0]
        && minimum20[1] <= margin + other->maximum28[1] && other->minimum20[1] - margin <= maximum28[1];
}

// Original 0x5b23b0: spawn nearby track polygons, then pair this body with every body whose
// XY bounds overlap it (sweep over the list sorted by maximum X).
void CollisionBody::FindPairs() {
    if (!CanCollide(0)) {
        return;
    }
    static_cast<DynamicCollisionBody *>(this)->SpawnTrackPolygons();
    float lowest = minimum20[0];
    WCollisionLink *start = link4.previous;
    while (start != &collisionBodies86B4C8 && !(BodyFromLink(start)->maximum28[0] < lowest)) {
        start = start->previous;
    }
    float highest = collisionLargestWidth86B29C + maximum28[0];
    WCollisionLink *stop = link4.next;
    while (stop != &collisionBodies86B4C8 && BodyFromLink(stop)->maximum28[0] <= highest) {
        stop = stop->next;
    }
    WCollisionLink *link = start->next;
    while (link != stop) {
        CollisionBody *body = BodyFromLink(link);
        link = link->next;
        if (body == this) {
            continue;
        }
        if (!(minimum20[0] <= body->maximum28[0]) || maximum28[0] < body->minimum20[0]
            || !(minimum20[1] <= body->maximum28[1]) || maximum28[1] < body->minimum20[1]) {
            continue;
        }
        if (id14 >= 0 && body->id14 >= 0) {
            int bit = id14 * 0xd6 + body->id14;
            if ((1 << (bit & 7)) & collisionPairBits86B0B0[bit >> 3]) {
                continue;
            }
        }
        if (!body->CanCollide(this)) {
            continue;
        }
        if (body->type11 == 2) {
            body = PromoteWorldObject(body);
        }
        if (body && Overlaps(body, 0.0f)) {
            new CollisionBodyWitness(this, body);
        }
    }
}

// Original 0x5a0820: the index-th live contact event involving this body.
CollisionEvent *CollisionBody::GetEvent(int index) {
    if (index < events18) {
        int found = 0;
        for (WCollisionLink *link = collisionWitnesses86B490.next; link != &collisionWitnesses86B490;
             link = link->next) {
            CollisionBodyWitness *witness = WitnessFromLink(link);
            if (witness->event244 && (witness->bodyA238 == this || witness->bodyB23C == this)) {
                if (found == index) {
                    return witness->event244;
                }
                ++found;
            }
        }
    }
    return 0;
}

// Witness destructor 0x7a1d44 slot 0.
CollisionWitness::~CollisionWitness() {
}

// Original 0x598590.
void CollisionWitness::SetShapes(CollisionShape *shapeA, CollisionShape *shapeB) {
    if (shapeA224 == shapeA && shapeB228 == shapeB) {
        return;
    }
    touching14 = 0;
    near15 = 0;
    dirty220 = 1;
    shapeA224 = shapeA;
    shapeB228 = shapeB;
    float thickness;
    if (shapeA->type10 == 2) {
        CollisionPolygon *polygon = static_cast<CollisionPolygon *>(shapeA);
        if (!polygon->thick1D0) {
            thick221 = 0;
            return;
        }
        thickness = polygon->thickness1D4;
    } else {
        if (shapeB->type10 != 2) {
            return;
        }
        CollisionPolygon *polygon = static_cast<CollisionPolygon *>(shapeB);
        if (!polygon->thick1D0) {
            thick221 = 0;
            return;
        }
        thickness = polygon->thickness1D4;
    }
    thick221 = 0.0f < thickness;
}

void *CollisionBodyWitness::operator new(unsigned int) {
    return bOMalloc(collisionWitnessPool86B0BC);
}

// Scalar deleting destructor 0x5b3360.
void CollisionBodyWitness::operator delete(void *memory) {
    bFree(collisionWitnessPool86B0BC, memory);
}

// Original 0x5a0340.
CollisionBodyWitness::CollisionBodyWitness(CollisionBody *bodyA, CollisionBody *bodyB) {
    simplex40.startedD8 = 0;
    shapeA224 = 0;
    shapeB228 = 0;
    if (bodyA->id14 == 0 && bodyB->id14 == 0xb) {
        collisionDebugWitness86B2B8 = this;
    }
    CollisionShape *shapeB = bodyB->GetShape(bodyA);
    CollisionShape *shapeA = bodyA->GetShape(bodyB);
    SetShapes(shapeA, shapeB);
    ++collisionWitnessCount86B248;
    if (collisionWitnessPeak86B270 < collisionWitnessCount86B248) {
        collisionWitnessPeak86B270 = collisionWitnessCount86B248;
    }
    WCollisionLink *tail = collisionWitnesses86B490.previous;
    tail->next = &link230;
    link230.previous = tail;
    collisionWitnesses86B490.previous = &link230;
    link230.next = &collisionWitnesses86B490;
    bodyB23C = bodyB;
    bodyA238 = bodyA;
    event244 = 0;
    side248 = 0;
    int bit = bodyA->id14 * 0xd6 + bodyB->id14;
    collisionPairBits86B0B0[bit >> 3] |= (unsigned char)(1 << (bit & 7));
    bit = bodyB23C->id14 * 0xd6 + bodyA238->id14;
    collisionPairBits86B0B0[bit >> 3] |= (unsigned char)(1 << (bit & 7));
    ++bodyA238->witnesses16;
    ++bodyB23C->witnesses16;
}

// Original 0x5b22b0.
CollisionBodyWitness::~CollisionBodyWitness() {
    --collisionWitnessCount86B248;
    if (this == collisionDebugWitness86B2B8) {
        collisionDebugWitness86B2B8 = 0;
    }
    if (event244) {
        ReleaseEvent();
    }
    WCollisionLink *previous = link230.previous;
    WCollisionLink *next = link230.next;
    previous->next = next;
    next->previous = previous;
    --bodyA238->witnesses16;
    --bodyB23C->witnesses16;
    int bit = bodyA238->id14 * 0xd6 + bodyB23C->id14;
    collisionPairBits86B0B0[bit >> 3] &= (unsigned char)~(1 << (bit & 7));
    bit = bodyB23C->id14 * 0xd6 + bodyA238->id14;
    collisionPairBits86B0B0[bit >> 3] &= (unsigned char)~(1 << (bit & 7));
}

// Original 0x592e60: a pair without an event is dropped once its bodies stop overlapping,
// both become inactive, or either refuses the other.
bool CollisionBodyWitness::IsStale() {
    if (event244) {
        return false;
    }
    CollisionBody *bodyB = bodyB23C;
    CollisionBody *bodyA = bodyA238;
    if (bodyA->Overlaps(bodyB, 0.0f) && (bodyA->active19 || bodyB->active19) && bodyA->CanCollide(bodyB)
        && bodyB23C->CanCollide(bodyA238)) {
        return false;
    }
    return true;
}

// Original 0x5a9b10.
void CollisionBodyWitness::CreateEvent(WheelVector *point) {
    void *memory = bOMalloc(collisionEventPool86B240);
    CollisionEvent *event = memory ? static_cast<CollisionEvent *>(memory)->Construct(this, point) : 0;
    event244 = event;
    bodyA238->AddEvent(event);
    bodyB23C->AddEvent(event244);
    ++collisionEventCount86B24C;
    if (collisionEventPeak86B274 < collisionEventCount86B24C) {
        collisionEventPeak86B274 = collisionEventCount86B24C;
    }
}

// Original 0x5ae470.
void CollisionBodyWitness::ReleaseEvent() {
    bodyA238->RemoveEvent(event244);
    bodyB23C->RemoveEvent(event244);
    CollisionEvent *event = event244;
    if (event) {
        event->Destroy();
        bFree(collisionEventPool86B240, event);
    }
    event244 = 0;
    --collisionEventCount86B24C;
}

// Original 0x5a9430.
CollisionEvent *CollisionEvent::Construct(CollisionBodyWitness *witness, WheelVector *point) {
    tick8 = 0;
    tickC = 0;
    impulse50.x = 0.0f;
    impulse50.y = 0.0f;
    impulse50.z = 0.0f;
    witness64 = witness;
    magnitude60 = 0.0f;
    CollisionEventOwner *owner = collisionEventOwner890080;
    WCollisionLink *tail = owner->events8.previous;
    tail->next = &link0;
    owner->events8.previous = &link0;
    link0.next = &owner->events8;
    link0.previous = tail;
    if (!point) {
        point20.x = 0.0f;
        point20.y = 0.0f;
        point20.z = 0.0f;
    } else {
        point20 = *point;
        RegisterCollisionEvent(this);
    }
    CollisionBody *bodyA = witness64->bodyA238;
    if (bodyA->type11 == 0 && witness64->bodyB23C->type11 == 0) {
        int modeA = static_cast<CarCollisionBody *>(bodyA)->car130->context14->value4;
        int modeB = static_cast<CarCollisionBody *>(witness64->bodyB23C)->car130->context14->value4;
        if ((modeA == 1 && modeB == 4) || (modeA == 4 && modeB == 1)) {
            PostCarCollisionMessage(0x20004);
        }
    }
    int clock = collisionClock;
    tick8 = clock;
    tickC = clock;
    value10 = 0;
    flag68 = 0;
    direction70.x = 0.0f;
    direction70.y = 0.0f;
    direction70.z = 0.0f;
    if (!IsWallContact()) {
        CollisionBody *a = witness64->bodyA238;
        if (a->type11 == 0 && witness64->bodyB23C->type11 == 0) {
            ++static_cast<DynamicCollisionBody *>(a)->carContacts124;
            ++static_cast<DynamicCollisionBody *>(witness64->bodyB23C)->carContacts124;
        }
    } else {
        CollisionBody *body = witness64->bodyA238;
        if (body->type11 == 3) {
            body = witness64->bodyB23C;
        }
        if (body) {
            ++static_cast<DynamicCollisionBody *>(body)->wallContacts120;
        }
    }
    return this;
}

// Original 0x5a9570.
void CollisionEvent::Destroy() {
    WCollisionLink *previous = link0.previous;
    WCollisionLink *next = link0.next;
    previous->next = next;
    next->previous = previous;
    UnregisterCollisionEvent(this);
    if (!IsWallContact()) {
        CollisionBody *a = witness64->bodyA238;
        if (a->type11 == 0 && witness64->bodyB23C->type11 == 0) {
            DynamicCollisionBody *dynamicA = static_cast<DynamicCollisionBody *>(a);
            int count = dynamicA->carContacts124 - 1;
            dynamicA->carContacts124 = count < 0 ? 0 : count;
            DynamicCollisionBody *dynamicB = static_cast<DynamicCollisionBody *>(witness64->bodyB23C);
            count = dynamicB->carContacts124 - 1;
            dynamicB->carContacts124 = count < 0 ? 0 : count;
        }
    } else {
        CollisionBody *body = witness64->bodyA238;
        if (body->type11 == 3) {
            body = witness64->bodyB23C;
        }
        if (body) {
            DynamicCollisionBody *dynamic = static_cast<DynamicCollisionBody *>(body);
            int count = dynamic->wallContacts120 - 1;
            dynamic->wallContacts120 = count < 0 ? 0 : count;
        }
    }
}

extern float collisionWallNormalLimit;
extern float collisionFloorNormalLimit;

static bool PolygonNormalBelow(CollisionBody *body, float limit) {
    if (body->type11 != 3) {
        return false;
    }
    float z = static_cast<TrackPolygonCollisionBody *>(body)->face60->normal70.z;
    return (z < 0.0f ? -z : z) < limit;
}

static bool PolygonNormalAbove(CollisionBody *body, float limit) {
    if (body->type11 != 3) {
        return false;
    }
    float z = static_cast<TrackPolygonCollisionBody *>(body)->face60->normal70.z;
    return (z < 0.0f ? -z : z) > limit;
}

// Original 0x5a0160: the event is against a near-vertical track polygon (|normal z| < 0.1).
bool CollisionEvent::IsWallContact() {
    return PolygonNormalBelow(witness64->bodyA238, collisionWallNormalLimit)
        || PolygonNormalBelow(witness64->bodyB23C, collisionWallNormalLimit);
}

// Original 0x5a01b0: the event is against a floor-like track polygon (|normal z| > 0.7).
bool CollisionEvent::IsTouching() {
    return PolygonNormalAbove(witness64->bodyA238, collisionFloorNormalLimit)
        || PolygonNormalAbove(witness64->bodyB23C, collisionFloorNormalLimit);
}

// Original 0x592bd0: events stay alive for 0.3 s after their last update.
static bool EventIsRecent(CollisionEvent *event) {
    return (float)(collisionClock - event->tickC) * collisionClockScale < collisionEventLifetime;
}

// Original 0x5a0eb0: contact order, latest impact time first.
static int ContactOrder(CollisionContact *added, CollisionContact *existing) {
    float difference = added->time8 - existing->time8;
    if ((difference < 0.0f ? -difference : difference) < collisionContactOrderTolerance) {
        int addedValid;
        int existingValid;
        float addedKey = added->SortKey(&addedValid);
        float existingKey = existing->SortKey(&existingValid);
        if (!addedValid || !existingValid) {
            return 1;
        }
        return addedKey <= existingKey;
    }
    return added->time8 <= existing->time8;
}

// Original 0x43bee0 (bTList::AddSorted) with comparator 0x5a0eb0.
int InsertContactSorted(WCollisionLink *list, CollisionContact *contact) {
    WCollisionLink *link = list->next;
    while (link != list) {
        if (!ContactOrder(contact, reinterpret_cast<CollisionContact *>(link))) {
            break;
        }
        link = link->next;
    }
    WCollisionLink *previous = link->previous;
    previous->next = &contact->link0;
    link->previous = &contact->link0;
    contact->link0.next = link;
    contact->link0.previous = previous;
    return 1;
}

// Original 0x5af090: drop the pending contacts that involve a body.
void RemoveBodyContacts(WCollisionLink *list, CollisionBody *body) {
    WCollisionLink *link = list->next;
    while (link != list) {
        WCollisionLink *next = link->next;
        CollisionContact *contact = reinterpret_cast<CollisionContact *>(link);
        if (contact->bodyAC == body || contact->bodyB10 == body) {
            WCollisionLink *previous = link->previous;
            previous->next = next;
            next->previous = previous;
            bFree(collisionContactPool86B2CC, contact);
        }
        link = next;
    }
}

// Original 0x5a0cd0: delete track polygon bodies that no pair references any more.
void DeleteOrphanTrackPolygons() {
    WCollisionLink *link = trackPolygonNodes86B5A0.next;
    while (link != &trackPolygonNodes86B5A0) {
        CollisionBody *body = *reinterpret_cast<CollisionBody **>(link + 1);
        link = link->next;
        if (!body->witnesses16) {
            delete body;
        }
    }
}

static void FreeContacts(WCollisionLink *list) {
    while (list->next != list) {
        WCollisionLink *link = list->next;
        WCollisionLink *next = link->next;
        WCollisionLink *previous = link->previous;
        previous->next = next;
        next->previous = previous;
        bFree(collisionContactPool86B2CC, link);
    }
}

// Original 0x5f0f00: advance every dynamic body by the frame time.
void AdvanceCollisionBodies(float duration) {
    CollisionBody *bodies[182];
    int count = 0;
    for (WCollisionLink *link = collisionBodies86B4C8.next; link != &collisionBodies86B4C8; link = link->next) {
        CollisionBody *body = BodyFromLink(link);
        if (body->dynamic12) {
            bodies[count++] = body;
        }
    }
    for (int i = 0; i < count; ++i) {
        bodies[i]->Advance(duration);
    }
}

// Original 0x5ffa50: the collision step run after all bodies have advanced.
void UpdateCollisionWorld(CollisionEventOwner *owner, float duration) {
    (void)duration;
    WCollisionLink *link = collisionWitnesses86B490.next;
    while (link != &collisionWitnesses86B490) {
        CollisionBodyWitness *witness = WitnessFromLink(link);
        link = link->next;
        if (witness->IsStale()) {
            delete witness;
        }
    }
    for (link = collisionBodies86B4C8.next; link != &collisionBodies86B4C8; link = link->next) {
        CollisionBody *body = BodyFromLink(link);
        if (body->dynamic12 && body->active19) {
            body->FindPairs();
        }
    }
    WCollisionLink contacts;
    contacts.next = &contacts;
    contacts.previous = &contacts;
    for (link = collisionWitnesses86B490.next; link != &collisionWitnesses86B490; link = link->next) {
        CollisionBodyWitness *witness = WitnessFromLink(link);
        witness->passes240 = 0;
        witness->Test(&contacts);
    }
    bGetTicker();
    int iteration = 0;
    while (contacts.next != &contacts) {
        if (iteration >= collisionIterations802080) {
            break;
        }
        CollisionContact *contact = reinterpret_cast<CollisionContact *>(contacts.next);
        contact->Resolve();
        ++contact->witness14->passes240;
        CollisionBody *bodyA = contact->bodyAC;
        CollisionBody *bodyB = contact->bodyB10;
        if (bodyA->dynamic12) {
            RemoveBodyContacts(&contacts, bodyA);
        }
        if (bodyB->dynamic12) {
            RemoveBodyContacts(&contacts, bodyB);
        }
        CollisionBody *moved[9];
        int count = 0;
        if (bodyA->type11 == 1 && *reinterpret_cast<float *>(reinterpret_cast<char *>(bodyA) + 0x154) == -1.0f) {
            count = BreakWorldObject(bodyA, moved);
        } else if (bodyA->dynamic12 && bodyA->active19) {
            moved[0] = bodyA;
            count = 1;
        }
        if (bodyB->type11 == 1 && *reinterpret_cast<float *>(reinterpret_cast<char *>(bodyB) + 0x154) == -1.0f) {
            count += BreakWorldObject(bodyB, &moved[count]);
        } else if (bodyB->dynamic12 && bodyB->active19) {
            moved[count++] = bodyB;
        }
        for (int i = 0; i < count; ++i) {
            moved[i]->FindPairs();
        }
        for (link = collisionWitnesses86B490.next; link != &collisionWitnesses86B490; link = link->next) {
            CollisionBodyWitness *witness = WitnessFromLink(link);
            for (int i = 0; i < count; ++i) {
                if ((witness->bodyA238 == moved[i] || witness->bodyB23C == moved[i])
                    && witness->passes240 < collisionWitnessPasses802084) {
                    witness->Test(&contacts);
                }
            }
        }
        ++iteration;
    }
    if (iteration == collisionIterations802080) {
        FreeContacts(&contacts);
    }
    link = owner->events8.next;
    while (link != &owner->events8) {
        CollisionEvent *event = reinterpret_cast<CollisionEvent *>(link);
        link = link->next;
        if (!EventIsRecent(event)) {
            event->witness64->ReleaseEvent();
        }
    }
    DeleteOrphanTrackPolygons();
    FreeContacts(&contacts);
}

// Original 0x5a0200 (pools, id allocator, body table, pair bits), 0x593db0 and 0x592b00.
void InitialiseCollisionPools() {
    worldObjectBodyPool86AD6C = bNewSlotPool(0x450, 0x20, "WorldObjectCollisionBodySlotPool", 0);
    carCollisionBodyPool86AD60 = bNewSlotPool(0x1a0, 0x16, "CarCollisionBodySlotPool", 0);
    trackPolygonBodyPool86AD68 = bNewSlotPool(0x270, 0xa0, "TrackPolygonCollisionBodySlotPool", 0);
    collisionWitnessPool86B0BC = bNewSlotPool(0x250, 0x80, "CollisionBodyWitnessSlotPool", 0);
    CollisionIdAllocator *ids = static_cast<CollisionIdAllocator *>(malloc(0xc));
    if (ids) {
        ids->bits4 = 0;
        ids->next8 = 0;
        ids->count0 = 0xd6;
        ids->bits4 = static_cast<unsigned char *>(malloc(0x1a));
        for (int i = 0; i < 0x1a; ++i) {
            ids->bits4[i] = 0;
        }
        ids->next8 = 0;
    }
    collisionIds86B2BC = ids;
    collisionBodyTable86B298 = static_cast<CollisionBody **>(malloc(0x358));
    for (int i = 0; i < 0xd6; ++i) {
        collisionBodyTable86B298[i] = 0;
    }
    WheelIdentity(&collisionIdentity86B520);
    for (int i = 0; i < 16; ++i) {
        collisionZeroMatrix86B450.elements[i] = 0.0f;
    }
    collisionPairBits86B0B0 = static_cast<unsigned char *>(malloc(0x165c));
    for (int i = 0; i < 0x165c; ++i) {
        collisionPairBits86B0B0[i] = 0;
    }
}

void InitialiseContactPool() {
    collisionContactPool86B2CC = bNewSlotPool(0x40, 0x30, "ContactSlotPool", 0);
}

void InitialiseCollisionEventPool() {
    collisionEventPool86B240 = bNewSlotPool(0x80, 0x20, "CollisionSlotPool", 0);
}

// List heads constructed by static initializers in the original.
void RunCollisionStaticInitializers() {
    collisionBodies86B4C8.next = &collisionBodies86B4C8;
    collisionBodies86B4C8.previous = &collisionBodies86B4C8;
    collisionWitnesses86B490.next = &collisionWitnesses86B490;
    collisionWitnesses86B490.previous = &collisionWitnesses86B490;
    trackPolygonNodes86B5A0.next = &trackPolygonNodes86B5A0;
    trackPolygonNodes86B5A0.previous = &trackPolygonNodes86B5A0;
    collisionHulls86B598.next = &collisionHulls86B598;
    collisionHulls86B598.previous = &collisionHulls86B598;
}
