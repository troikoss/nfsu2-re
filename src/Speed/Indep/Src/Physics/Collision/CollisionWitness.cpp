#include "Collision.h"

extern "C" double __cdecl sqrt(double);
#pragma intrinsic(sqrt)
extern "C" void *malloc(unsigned int size);
extern "C" void free(void *memory);

// Shape-pair distance (GJK) and the per-pair test that queues contacts.

extern int collisionClock;
extern float collisionClockScale;
extern float collisionTouchDistance;
extern float collisionIntersectDistance;
extern float collisionEventSettleTime;
extern int collisionFrame7FB724;
extern float collisionRewindMinimum;

int VectorsNear(float *a, float *b, float tolerance);

static void ClearVertex(CollisionVertex *vertex) {
    vertex->neighbourCount8 = 0;
    vertex->neighbourStartA = 0;
    vertex->indexC = 0;
    vertex->stampE = 0;
    vertex->point10[0] = 0.0f;
    vertex->point10[1] = 0.0f;
    vertex->point10[2] = 0.0f;
}

static CollisionVertex *Support(CollisionShape *shape, WheelVector *direction, CollisionVertex *start,
                                WheelMatrix *matrix, WheelMatrix *inverse, CollisionVertex *output) {
    switch (shape->type10) {
    case 1:
        return static_cast<CollisionHull *>(shape)->Support(direction, start, matrix, inverse, output);
    case 2:
        return static_cast<CollisionPolygon *>(shape)->Support(direction, start, matrix, inverse, output);
    case 3:
        return static_cast<CollisionBox *>(shape)->Support(direction, start, matrix, inverse, output);
    case 4:
        return static_cast<CollisionPoint *>(shape)->Support(direction, start, matrix, inverse, output);
    case 5:
        return static_cast<CollisionCylinder *>(shape)->Support(direction, start, matrix, inverse, output);
    }
    return output;
}

// Original 0x598620: store the shapes' poses; moving either position marks the pair dirty.
void CollisionWitness::SetPoses(WheelMatrix *matrixA, WheelVector *positionA, WheelMatrix *matrixB,
                                WheelVector *positionB) {
    if (!VectorsNear(&positionA->x, &positionA20.x, 0.001f) || !VectorsNear(&positionB->x, &positionB30.x, 0.001f)) {
        dirty220 = 1;
    }
    if (!dirty220) {
        return;
    }
    BodyMatrixCopy(&matrixA120, matrixA);
    WheelTranspose(&inverseA160, matrixA);
    matrixA120.elements[13] = positionA->y;
    matrixA120.elements[14] = positionA->z;
    matrixA120.elements[12] = positionA->x;
    matrixA120.elements[15] = 1.0f;
    BodyMatrixCopy(&matrixB1A0, matrixB);
    WheelTranspose(&inverseB1E0, matrixB);
    matrixB1A0.elements[12] = positionB->x;
    matrixB1A0.elements[13] = positionB->y;
    matrixB1A0.elements[14] = positionB->z;
    matrixB1A0.elements[15] = 1.0f;
    positionA20 = *positionA;
    positionB30 = *positionB;
}

static bool PolygonThick(CollisionShape *shape) {
    CollisionPolygon *polygon = static_cast<CollisionPolygon *>(shape);
    return polygon->thick1D0 && 0.0f < polygon->thickness1D4;
}

// Original 0x5b3d20: recompute the distance of a dirty pair.
float CollisionWitness::Update() {
    if (!dirty220) {
        return distance10;
    }
    CollisionShape *shape = shapeA224;
    unsigned char thick;
    if (shape->type10 == 2 && (unsigned char)PolygonThick(shape) != thick221) {
        thick = PolygonThick(shape);
        thick221 = thick;
        simplex40.startedD8 = 0;
    } else {
        shape = shapeB228;
        if (shape->type10 == 2 && (unsigned char)PolygonThick(shape) != thick221) {
            thick = PolygonThick(shape);
            thick221 = thick;
            simplex40.startedD8 = 0;
        }
    }
    float distance = ComputeDistance();
    distance10 = distance;
    near15 = distance <= collisionIntersectDistance;
    if (distance <= collisionTouchDistance) {
        touching14 = 1;
    } else {
        touching14 = 0;
    }
    dirty220 = 0;
    return distance;
}

// Original 0x5b3390: GJK distance between the posed shapes, warm-started from the cached
// simplex vertex indices.
float CollisionWitness::ComputeDistance() {
    CollisionVertex supportA;
    CollisionVertex supportB;
    ClearVertex(&supportA);
    ClearVertex(&supportB);
    WheelVector direction;
    float squared;
    CollisionSimplex *simplex = &simplex40;
    if (!simplex->startedD8) {
        CollisionVertex *vertex = CollisionShapeVertex(shapeA224, 0, &supportA);
        WheelTransform(reinterpret_cast<WheelVector *>(supportA.point10), &matrixA120,
                       reinterpret_cast<WheelVector *>(vertex->point10));
        vertex = CollisionShapeVertex(shapeB228, 0, &supportB);
        WheelTransform(reinterpret_cast<WheelVector *>(supportB.point10), &matrixB1A0,
                       reinterpret_cast<WheelVector *>(vertex->point10));
        WheelVector toward;
        toward.x = supportB.point10[0] - supportA.point10[0];
        toward.y = supportB.point10[1] - supportA.point10[1];
        toward.z = supportB.point10[2] - supportA.point10[2];
        WheelVector away;
        away.x = -toward.x;
        away.y = -toward.y;
        away.z = -toward.z;
        Support(shapeA224, &toward, &supportA, &matrixA120, &inverseA160, &supportA);
        Support(shapeB228, &away, &supportB, &matrixB1A0, &inverseB1E0, &supportB);
        simplex->a0[0][0] = supportA.point10[0];
        simplex->a0[0][2] = supportA.point10[2];
        simplex->a0[0][1] = supportA.point10[1];
        simplex->b40[0][0] = supportB.point10[0];
        simplex->b40[0][2] = supportB.point10[2];
        simplex->b40[0][1] = supportB.point10[1];
        simplex->indicesB0[0] = supportA.indexC;
        simplex->indicesC4[0] = supportB.indexC;
        simplex->countD4 = 1;
        simplex->countC0 = 1;
        simplex->closestA80.x = simplex->a0[0][0];
        simplex->closestA80.y = simplex->a0[0][1];
        simplex->closestA80.z = simplex->a0[0][2];
        simplex->closestB90.x = simplex->b40[0][0];
        simplex->closestB90.y = simplex->b40[0][1];
        simplex->closestB90.z = simplex->b40[0][2];
        direction.x = simplex->closestB90.x - simplex->closestA80.x;
        direction.y = simplex->closestB90.y - simplex->closestA80.y;
        direction.z = simplex->closestB90.z - simplex->closestA80.z;
        squared = direction.y * direction.y + direction.z * direction.z + direction.x * direction.x;
        if (!(squared > collisionIntersectDistance)) {
            return 0.0f;
        }
        simplex->startedD8 = 1;
    } else {
        for (int i = 0; i < simplex->countC0; ++i) {
            CollisionShapeVertex(shapeA224, simplex->indicesB0[i], &supportA);
            if (shapeA224->type10 == 2) {
                simplex->a0[i][0] = supportA.point10[0];
                simplex->a0[i][1] = supportA.point10[1];
                simplex->a0[i][2] = supportA.point10[2];
            } else {
                WheelTransform(reinterpret_cast<WheelVector *>(simplex->a0[i]), &matrixA120,
                               reinterpret_cast<WheelVector *>(supportA.point10));
            }
        }
        for (int i = 0; i < simplex->countD4; ++i) {
            CollisionShapeVertex(shapeB228, simplex->indicesC4[i], &supportB);
            if (shapeB228->type10 == 2) {
                simplex->b40[i][0] = supportB.point10[0];
                simplex->b40[i][1] = supportB.point10[1];
                simplex->b40[i][2] = supportB.point10[2];
            } else {
                WheelTransform(reinterpret_cast<WheelVector *>(simplex->b40[i]), &matrixB1A0,
                               reinterpret_cast<WheelVector *>(supportB.point10));
            }
        }
        squared = simplex->Distance(&direction);
        if (!(squared > collisionIntersectDistance)) {
            return 0.0f;
        }
    }
    float previous = squared;
    CollisionSimplex saved;
    saved.CopyFrom(simplex);
    for (;;) {
        WheelVector away;
        away.x = -direction.x;
        away.y = -direction.y;
        away.z = -direction.z;
        Support(shapeA224, &direction, &supportA, &matrixA120, &inverseA160, &supportA);
        Support(shapeB228, &away, &supportB, &matrixB1A0, &inverseB1E0, &supportB);
        bool addedA = true;
        for (int i = 0; i < simplex->countC0; ++i) {
            if (simplex->indicesB0[i] == supportA.indexC) {
                addedA = false;
                break;
            }
        }
        if (addedA) {
            int count = simplex->countC0;
            simplex->indicesB0[count] = supportA.indexC;
            simplex->a0[count][1] = supportA.point10[1];
            simplex->a0[count][0] = supportA.point10[0];
            simplex->a0[count][2] = supportA.point10[2];
            simplex->countC0 = count + 1;
        }
        bool addedB = true;
        for (int i = 0; i < simplex->countD4; ++i) {
            if (simplex->indicesC4[i] == supportB.indexC) {
                addedB = false;
                break;
            }
        }
        if (addedB) {
            int count = simplex->countD4;
            simplex->indicesC4[count] = supportB.indexC;
            simplex->b40[count][1] = supportB.point10[1];
            simplex->b40[count][0] = supportB.point10[0];
            simplex->b40[count][2] = supportB.point10[2];
            simplex->countD4 = count + 1;
        }
        if (!addedA && !addedB) {
            break;
        }
        float next = simplex->Distance(&direction);
        squared = next;
        if (next == 0.0f) {
            return 0.0f;
        }
        if (!(next < previous)) {
            squared = previous;
            simplex->Assign(&saved);
            break;
        }
        previous = next;
        saved.Assign(simplex);
    }
    WheelNormalize(&simplex->normalA0, &simplex->normalA0);
    return (float)sqrt(squared);
}

// Original 0x592f00: polygons against world objects (type 1) use their extrusion only when
// the object's centre is in front of them.
void CollisionBodyWitness::MarkThickPolygon(char keep) {
    CollisionBody *polygon;
    CollisionBody *other;
    if (bodyA238->type11 == 3 && bodyB23C->type11 == 1) {
        polygon = bodyA238;
        other = bodyB23C;
    } else if (bodyB23C->type11 == 3 && bodyA238->type11 == 1) {
        polygon = bodyB23C;
        other = bodyA238;
    } else {
        return;
    }
    if (!polygon || !other) {
        return;
    }
    char thick = keep;
    if (!keep) {
        WCollisionFace *face = static_cast<TrackPolygonCollisionBody *>(polygon)->face60;
        WheelVector normal = face->normal70;
        WheelVector *centre = &static_cast<CollisionPolygon *>(polygon->GetShape(0))->centre1F0;
        if (static_cast<TrackPolygonCollisionBody *>(polygon)->face60->flagsA & 2) {
            centre->z = other->GetPosition()->z;
        }
        WheelVector *position = other->GetPosition();
        thick = 1;
        float side = (position->z - centre->z) * normal.z + (position->x - centre->x) * normal.x
                   + (position->y - centre->y) * normal.y;
        if (!(side > 0.0f)) {
            thick = 0;
        }
    }
    static_cast<CollisionPolygon *>(polygon->GetShape(0))->thick1D0 = thick;
}

// Original 0x593010.
void CollisionBodyWitness::ClearThickPolygon() {
    if (bodyA238->type11 == 3) {
        static_cast<CollisionPolygon *>(bodyA238->GetShape(0))->thick1D0 = 0;
        return;
    }
    if (bodyB23C->type11 == 3) {
        static_cast<CollisionPolygon *>(bodyB23C->GetShape(0))->thick1D0 = 0;
    }
}

// Original 0x592ed0: bodies that stop cars (track polygons only with face flag 2).
static int IsSolid(CollisionBody *body) {
    int type = body->type11;
    if (type < 0) {
        return 0;
    }
    if (type <= 2) {
        return 1;
    }
    if (type == 3) {
        return (static_cast<TrackPolygonCollisionBody *>(body)->face60->flagsA & 2) >> 1;
    }
    return 0;
}

// Original 0x5b5050: distance test of a pair; touching pairs of movable bodies queue a
// contact (sorted by impact time).
void CollisionBodyWitness::Test(WCollisionLink *contacts) {
    char typeA = bodyA238->type11;
    if (typeA == 4 && bodyB23C->type11 != 0 && bodyB23C->type11 != 1) {
        return;
    }
    if (bodyB23C->type11 == 4 && typeA != 0 && typeA != 1) {
        return;
    }
    if (!bodyA238->CanCollide(bodyB23C)) {
        return;
    }
    if (!bodyB23C->CanCollide(bodyA238)) {
        return;
    }
    char keep = 0;
    if (event244 && (float)(collisionClock - event244->tick8) * collisionClockScale > collisionEventSettleTime) {
        keep = 1;
    }
    MarkThickPolygon(keep);
    WheelVector *offset = bodyA238->GetShapeOffset();
    if (shapeA224->type10 == 1) {
        CollisionHull *hull = static_cast<CollisionHull *>(shapeA224);
        hull->offset40 = *offset;
        hull->offsetValid3E = 1;
    }
    offset = bodyB23C->GetShapeOffset();
    if (shapeB228->type10 == 1) {
        CollisionHull *hull = static_cast<CollisionHull *>(shapeB228);
        hull->offset40 = *offset;
        hull->offsetValid3E = 1;
    }
    CollisionBody *bodyB = bodyB23C;
    CollisionBody *bodyA = bodyA238;
    WheelVector *positionB = bodyB->GetPosition();
    WheelMatrix *matrixB = bodyB->GetMatrix();
    WheelVector *positionA = bodyA->GetPosition();
    WheelMatrix *matrixA = bodyA->GetMatrix();
    SetPoses(matrixA, positionA, matrixB, positionB);
    Update();
    ClearThickPolygon();
    if (!touching14) {
        return;
    }
    if ((bodyA238->Slot6C() || bodyB23C->Slot6C()) && bodyA238->dynamic12 && bodyB23C->dynamic12) {
        if (bodyA238->type11 == 0) {
            static_cast<CarCollisionBody *>(bodyA238)->tick13C = collisionClock;
        }
        if (bodyB23C->type11 == 0) {
            static_cast<CarCollisionBody *>(bodyB23C)->tick13C = collisionClock;
        }
        return;
    }
    if (bodyA238->type11 == 0) {
        CarCollisionBody *car = static_cast<CarCollisionBody *>(bodyA238);
        if (IsSolid(bodyB23C)) {
            car->tick140 = collisionClock;
        }
        car->tick138 = collisionClock;
    }
    if (bodyB23C->type11 == 0) {
        CarCollisionBody *car = static_cast<CarCollisionBody *>(bodyB23C);
        if (IsSolid(bodyA238)) {
            car->tick140 = collisionClock;
        }
        car->tick138 = collisionClock;
    }
    void *memory = bOMalloc(collisionContactPool86B2CC);
    CollisionContact *contact = memory ? static_cast<CollisionContact *>(memory)->Construct(bodyA238, bodyB23C, this) : 0;
    InsertContactSorted(contacts, contact);
}

// Original 0x5b4f90.
CollisionContact *CollisionContact::Construct(CollisionBody *bodyA, CollisionBody *bodyB, CollisionBodyWitness *witness) {
    bodyB10 = bodyB;
    bodyAC = bodyA;
    witness14 = witness;
    time8 = FindImpactTime();
    value30 = collisionFrame7FB724;
    return this;
}

// Original 0x5b4be0: bisect (8 steps) for the latest time inside the step at which the
// bodies only touch; the poses are rewound by that time when the contact is resolved.
float CollisionContact::FindImpactTime() {
    WheelMatrix matrixB;
    WheelIdentity(&matrixB);
    offset20.x = 0.0f;
    offset20.y = 0.0f;
    offset20.z = 0.0f;
    WheelVector positionB;
    positionB.x = 0.0f;
    positionB.y = 0.0f;
    positionB.z = 0.0f;
    float upper = bodyAC->GetStepDuration();
    float other = bodyB10->GetStepDuration();
    if (other <= upper) {
        upper = other;
    }
    float lower = 0.0f;
    float earliest = -1.0f;
    if (!witness14->near15) {
        return 0.0f;
    }
    witness14->MarkThickPolygon(1);
    float middle;
    int iteration = 0;
    WheelVector positionA;
    WheelMatrix matrixA;
    for (;;) {
        middle = (upper - lower) * 0.5f + lower;
        bodyAC->GetPoseAt(&positionA, &matrixA, middle);
        bodyB10->GetPoseAt(&positionB, &matrixB, middle);
        witness14->SetPoses(&matrixA, &positionA, &matrixB, &positionB);
        witness14->Update();
        if (witness14->near15) {
            lower = middle;
        } else {
            if (witness14->touching14) {
                goto done;
            }
            upper = middle;
            if (earliest == -1.0f || middle < earliest) {
                earliest = middle;
            }
        }
        if (++iteration >= 8) {
            break;
        }
    }
    if (earliest <= 0.0f) {
        Separate(middle);
    } else {
        middle = earliest;
    }
done:
    CollisionBodyWitness *witness = witness14;
    if (witness->bodyA238->type11 == 3) {
        static_cast<CollisionPolygon *>(witness->bodyA238->GetShape(0))->thick1D0 = 0;
        return middle;
    }
    if (witness->bodyB23C->type11 == 3) {
        static_cast<CollisionPolygon *>(witness->bodyB23C->GetShape(0))->thick1D0 = 0;
    }
    return middle;
}

extern int collisionUseGroupSides7FBF18;
extern int collisionCacheSides7FBF14;

// Original 0x593df0: the polygon normal facing `point` (barrier-group polygons only; other
// polygons always push along their face normal).
static void PolygonNormal(CollisionContact *contact, WheelVector *output, CollisionBody *body,
                          CollisionBody *other, WheelVector *point) {
    TrackPolygonCollisionBody *polygon = static_cast<TrackPolygonCollisionBody *>(body);
    float ny = polygon->face60->normal70.y;
    float nx = polygon->face60->normal70.x;
    float nz = polygon->face60->normal70.z;
    if (!collisionUseGroupSides7FBF18 || polygon->grouped64) {
        CollisionPolygon *shape = static_cast<CollisionPolygon *>(polygon->GetShape(0));
        WheelVector *centre;
        if (!shape->thick1D0 || shape->thickness1D4 <= 0.0f) {
            centre = &shape->centre1E0;
        } else {
            centre = &shape->centre1F0;
        }
        if (!point || other->type11 == 0) {
            point = &static_cast<DynamicCollisionBody *>(other)->centre100;
        }
        if (collisionCacheSides7FBF14) {
            int side = other->type11 == 1 ? contact->witness14->side248 : 0;
            if (side == 0) {
                if ((point->x - centre->x) * nx + (point->y - centre->y) * ny + nz * (point->z - centre->z) < 0.0f) {
                    output->x = -nx;
                    output->y = -ny;
                    output->z = -nz;
                    contact->witness14->side248 = 2;
                    return;
                }
                output->x = nx;
                output->y = ny;
                output->z = nz;
                contact->witness14->side248 = 1;
                return;
            }
            if (side == 1) {
                output->x = nx;
                output->y = ny;
                output->z = nz;
                return;
            }
            if (side == 2) {
                output->x = -nx;
                output->y = -ny;
                output->z = -nz;
            }
            return;
        }
        if ((point->x - centre->x) * nx + (point->y - centre->y) * ny + nz * (point->z - centre->z) < 0.0f) {
            output->x = -nx;
            output->y = -ny;
            output->z = -nz;
            return;
        }
    }
    output->x = nx;
    output->y = ny;
    output->z = nz;
}

extern float collisionSeparationStepLimit;
extern float collisionSeparationStepGrowth;
extern float collisionSeparationClamp;
extern float collisionSeparationFastSpeed;

// Original 0x5b42f0: push the poses at `time` apart along the contact direction in growing
// steps (vertical part clamped to 0.15) until they no longer intersect; the push is stored
// at +0x20.
void CollisionContact::Separate(float time) {
    WheelVector positionA;
    WheelMatrix matrixA;
    WheelVector positionB;
    WheelMatrix matrixB;
    bodyAC->GetPoseAt(&positionA, &matrixA, time);
    bodyB10->GetPoseAt(&positionB, &matrixB, time);
    WheelVector direction;
    CollisionBody *bodyA = bodyAC;
    char typeA = bodyA->type11;
    if (typeA == 3) {
        PolygonNormal(this, &direction, bodyA, bodyB10, &positionB);
    } else {
        CollisionBody *bodyB = bodyB10;
        char typeB = bodyB->type11;
        if (typeB == 3) {
            PolygonNormal(this, &direction, bodyB, bodyA, &positionA);
            direction.x = -direction.x;
            direction.y = -direction.y;
            direction.z = -direction.z;
        } else {
            float speed;
            if (typeA == 2 || typeA == 1) {
                WheelVector *velocity = bodyB->GetVelocity();
                direction.x = -velocity->x;
                direction.y = -velocity->y;
                direction.z = -velocity->z;
                speed = (float)sqrt(direction.y * direction.y + direction.z * direction.z + direction.x * direction.x);
            } else if (typeB == 2 || typeB == 1) {
                direction = *bodyA->GetVelocity();
                speed = (float)sqrt(direction.x * direction.x + direction.y * direction.y + direction.z * direction.z);
            } else {
                speed = 0.0f;
            }
            if (typeA == 2 || typeA == 1 || typeB == 2 || typeB == 1) {
                if (speed < collisionSeparationFastSpeed) {
                    direction.x = positionB.x - positionA.x;
                    direction.y = positionB.y - positionA.y;
                    direction.z = positionB.z - positionA.z;
                }
            } else {
                direction.x = positionB.x - positionA.x;
                direction.y = positionB.y - positionA.y;
                direction.z = positionB.z - positionA.z;
            }
            WheelNormalize(&direction, &direction);
        }
    }
    float distance = 0.0f;
    float step = 0.005f;
    int iteration = 0;
    for (;;) {
        distance = step + distance;
        if (step < collisionSeparationStepLimit) {
            step = step + collisionSeparationStepGrowth;
        }
        WheelVector push;
        push.x = direction.x * distance;
        push.y = direction.y * distance;
        float z = direction.z * distance;
        float lowest = -collisionSeparationClamp;
        if (!(z > lowest)) {
            z = lowest;
        }
        if (collisionSeparationClamp < z) {
            z = collisionSeparationClamp;
        }
        push.z = z;
        WheelVector movedA = positionA;
        WheelVector movedB = positionB;
        if (bodyAC->IsMovable()) {
            movedA.x = movedA.x - push.x;
            movedA.y = movedA.y - push.y;
            movedA.z = movedA.z - push.z;
        }
        if (bodyB10->IsMovable()) {
            movedB.x = movedB.x + push.x;
            movedB.y = movedB.y + push.y;
            movedB.z = movedB.z + push.z;
        }
        witness14->SetPoses(&matrixA, &movedA, &matrixB, &movedB);
        witness14->Update();
        offset20 = push;
        if (!witness14->touching14 || !witness14->near15) {
            return;
        }
        if (++iteration >= 0x14) {
            return;
        }
    }
}

// Original 0x594000: contact point midway between the closest points, and the normal
// (towards B): the polygon normal against track polygons, else the closest-point direction.
void CollisionContact::ContactFrame(WheelVector *point, WheelVector *normal) {
    CollisionSimplex *simplex = &witness14->simplex40;
    WheelVector closestA = simplex->closestA80;
    WheelVector closestB = simplex->closestB90;
    point->x = (closestB.x + closestA.x) * 0.5f;
    point->y = (closestB.y + closestA.y) * 0.5f;
    point->z = (closestB.z + closestA.z) * 0.5f;
    if (!bodyAC->dynamic12) {
        PolygonNormal(this, normal, bodyAC, bodyB10, &closestB);
        return;
    }
    if (!bodyB10->dynamic12) {
        PolygonNormal(this, normal, bodyB10, bodyAC, &closestA);
        normal->z = -normal->z;
        normal->x = -normal->x;
        normal->y = -normal->y;
        return;
    }
    float x = closestB.x - closestA.x;
    float y = closestB.y - closestA.y;
    float z = closestB.z - closestA.z;
    normal->x = x;
    normal->y = y;
    normal->z = z;
    float length = (float)sqrt(x * x + y * y + z * z);
    if (length < collisionRewindMinimum) {
        WheelVector *positionA = bodyAC->GetPosition();
        WheelVector *positionB = bodyB10->GetPosition();
        normal->x = positionB->x - positionA->x;
        normal->z = positionB->z - positionA->z;
        normal->y = positionB->y - positionA->y;
        WheelNormalize(normal, normal);
        return;
    }
    float inverse = 1.0f / length;
    normal->x = x * inverse;
    normal->y = y * inverse;
    normal->z = z * inverse;
}

// Original 0x5a0dc0: key for contacts with (nearly) equal impact times: the dynamic body's
// height along the polygon normal.
float CollisionContact::SortKey(int *valid) {
    CollisionBody *bodyA = bodyAC;
    CollisionBody *bodyB = bodyB10;
    WheelVector normal;
    if (bodyA->type11 == 3) {
        if (bodyB->dynamic12) {
            PolygonNormal(this, &normal, bodyA, bodyB, 0);
            *valid = 1;
            WheelVector *position = bodyB10->GetPosition();
            return normal.x * position->x + normal.z * position->z + normal.y * position->y;
        }
    } else if (bodyB->type11 == 3 && bodyA->dynamic12) {
        PolygonNormal(this, &normal, bodyB, bodyA, 0);
        normal.x = -normal.x;
        *valid = 1;
        normal.y = -normal.y;
        normal.z = -normal.z;
        WheelVector *position = bodyAC->GetPosition();
        return normal.x * position->x + normal.y * position->y + normal.z * position->z;
    }
    *valid = 0;
    return 0.0f;
}
