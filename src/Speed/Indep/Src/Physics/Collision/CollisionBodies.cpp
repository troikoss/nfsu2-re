#include "Collision.h"

extern "C" double __cdecl sqrt(double);
#pragma intrinsic(sqrt)
extern "C" void *malloc(unsigned int size);
extern "C" void free(void *memory);
#include "../VehicleMover.h"
#include "../VehicleIntegrator.h"

// DynamicCollisionBody (parent of the car body), CarCollisionBody and
// TrackPolygonCollisionBody.

extern int collisionClock;
extern int collisionDynamicCount86B258;
extern int collisionDynamicPeak86B280;
extern int collisionCarCount86B260;
extern int collisionCarPeak86B288;
extern int collisionStaticCount86B254;
extern int collisionStaticPeak86B27C;
extern int collisionPolygonCount86B25C;
extern int collisionPolygonPeak86B284;
extern int collisionDisabled81CB88;
extern int trackPolygonScans86AF7C;
extern int trackPolygonBucketHits86AF80;
extern int trackPolygonDuplicates86AF84;

extern float collisionRewindMinimum;
extern float collisionGrowRadius;
extern float collisionBucketScale;
extern float collisionPolygonThickness;
extern float collisionShapeMargin;
extern float collisionShapeHalf;
extern float collisionInverseMassLimit;
extern float collisionRearInertiaScale;
extern float collisionRestitution;
extern float collisionWallRestitutionScale;
extern float collisionFriction;
extern float collisionSlopeFriction;
extern float collisionGroundedFrictionScale;
extern float collisionLandedFrictionScale;
extern float collisionWallNormalLimit;
extern float collisionFloorNormalLimit;
extern float collisionPolygonResponse;
extern float collisionPolygonDuration;
extern float collisionForceFeedbackLimit;
extern const char collisionCarNameFormat[];
extern const char collisionPolygonNameFormat[];

int VectorsNear(float *a, float *b, float tolerance);
int bSPrintf(char *buffer, const char *format, ...);
extern "C" char *nfs_strncpy(char *dest, const char *src, int maxlen);
const char *CollisionSectionName(int section);
unsigned int CarCollisionHullKey(PhysicsInfo *info);
unsigned int CarCollisionBaseHullKey(PhysicsInfo *info);
void NotifyCarContact(MoverModeOwner *car, MoverModeOwner *other);
int WorldObjectKind(CollisionBody *body);
void MoverVelocitiesAt(MoverBody *body, WheelVector *velocity, WheelVector *angular, int a, int b);
float MoverSlot68(MoverBody *body, int a, int b);
float CarSlot88(CarCollisionBody *body);

// Original 0x5ed400: the mover body's centre of mass, else the car record's centre.
WheelVector *CarCollisionCentre(MoverModeOwner *car) {
    if (car->mover34 && car->mover34->GetBody()) {
        return &car->mover34->GetBody()->centre1F0;
    }
    return reinterpret_cast<WheelVector *>(car->record20);
}

// Original 0x5ed1f0: the car's mover is in state 2.
static int CarMoverFrozen(MoverModeOwner *car) {
    if (car->moverType28 == 1) {
        return car->mover34->value60 == 2;
    }
    if (car->moverType28 != 3) {
        return 0;
    }
    return car->mover34->value60 == 2;
}

// ---------------------------------------------------------------------------
// DynamicCollisionBody

// Original 0x5a9ce0.
DynamicCollisionBody::DynamicCollisionBody(int id, char type, void *geometry) : CollisionBody(id, type) {
    geometry60 = geometry;
    ++collisionDynamicCount86B258;
    if (collisionDynamicPeak86B280 < collisionDynamicCount86B258) {
        collisionDynamicPeak86B280 = collisionDynamicCount86B258;
    }
    moverIntegrator64 = 0;
    body68 = 0;
    hasMover5C = 0;
    wallContacts120 = 0;
    carContacts124 = 0;
    dynamic12 = 1;
}

// Original 0x5a9dc0.
DynamicCollisionBody::~DynamicCollisionBody() {
    --collisionDynamicCount86B258;
}

// Original 0x5a9d40.
void DynamicCollisionBody::SetPosition(WheelVector *position) {
    *GetPosition() = *position;
}

// Original 0x5a9d60.
float DynamicCollisionBody::GetRadius() {
    return radius1C;
}

// Original 0x5a9d70.
float DynamicCollisionBody::GetStepDuration() {
    return duration6C;
}

// Original 0x5938c0: point at the car's PhysicsMover integrator and body.
int DynamicCollisionBody::AttachMover() {
    moverIntegrator64 = 0;
    body68 = 0;
    if (!hasMover5C) {
        return 0;
    }
    MoverModeOwner *car = static_cast<CarCollisionBody *>(this)->car130;
    if (car->moverType28 == 1 || car->moverType28 == 3) {
        VehicleMover *mover = car->mover34;
        moverIntegrator64 = mover ? mover->integrator8 : 0;
        body68 = mover->body20;
        return reinterpret_cast<int>(body68);
    }
    return reinterpret_cast<int>(car);
}

// Original 0x593210: remember the pose at the start of a step.
void DynamicCollisionBody::Snapshot(float duration) {
    BodyMatrixCopy(&matrix70, GetMatrix());
    positionB0 = *GetPosition();
    velocityC0 = *GetVelocity();
    duration6C = duration;
}

// Original 0x593590: velocity `time` seconds before the end of the step.
void DynamicCollisionBody::GetRewoundPosition(WheelVector *velocity, float time) {
    if (duration6C < collisionRewindMinimum) {
        *velocity = *GetVelocity();
        return;
    }
    float blend = (duration6C - time) / duration6C;
    WheelVector *current = GetVelocity();
    WheelVector change;
    change.y = current->y - velocityC0.y;
    change.z = current->z - velocityC0.z;
    change.x = current->x - velocityC0.x;
    WheelScaleAdd(velocity, &velocityC0, &change, blend);
}

// Original 0x593280: pose `time` seconds before the end of the step.
void DynamicCollisionBody::GetPoseAt(WheelVector *position, WheelMatrix *matrix, float time) {
    WheelVector *current = GetPosition();
    WheelMatrix *rotation = GetMatrix();
    if (duration6C < collisionRewindMinimum) {
        *position = *current;
        BodyMatrixCopy(matrix, rotation);
        return;
    }
    float blend = (duration6C - time) / duration6C;
    WheelVector change;
    change.x = current->x - positionB0.x;
    change.y = current->y - positionB0.y;
    change.z = current->z - positionB0.z;
    WheelScaleAdd(position, &positionB0, &change, blend);
    for (int i = 0; i < 16; ++i) {
        matrix->elements[i] = (rotation->elements[i] - matrix70.elements[i]) * blend + matrix70.elements[i];
    }
}

// Original 0x593630: move the body back to its pose at `time`, keeping the rewound velocity.
void DynamicCollisionBody::Rewind(float time) {
    WheelVector position;
    WheelMatrix matrix;
    WheelVector velocity;
    GetPoseAt(&position, &matrix, time);
    GetRewoundPosition(&velocity, time);
    MoverBody *body = body68;
    if (body) {
        float mass = body->divisor164;
        velocity.x = velocity.x * mass;
        velocity.y = velocity.y * mass;
        velocity.z = velocity.z * mass;
        body->position20 = position;
        BodyMatrixCopy(&body68->matrix30, &matrix);
        body68->vector70 = velocity;
        return;
    }
    Slot78(time, &position, &matrix, &velocity);
}

// Original 0x593700: let the mover re-derive its state after the body moved.
void DynamicCollisionBody::SyncMover() {
    if (hasMover5C) {
        static_cast<VehicleIntegrator *>(moverIntegrator64)->AfterIntegrate();
    }
}

// Original 0x593710.
void DynamicCollisionBody::GetVelocities(WheelVector *velocity, WheelVector *angular) {
    if (hasMover5C) {
        *velocity = body68->vectorA0;
        *angular = body68->vectorC0;
        return;
    }
    *velocity = *GetVelocity();
    *angular = *GetAngularVelocity();
}

// Original 0x5a0990.
void DynamicCollisionBody::GetVelocitiesAt(WheelVector *velocity, WheelVector *angular, int a, int b) {
    if (hasMover5C) {
        MoverVelocitiesAt(body68, velocity, angular, a, b);
        return;
    }
    *velocity = *GetVelocity();
    *angular = *GetAngularVelocity();
}

// 0x4031c0 (slots 0x44 and 0x48): empty.
void DynamicCollisionBody::Slot44(int) {
}

void DynamicCollisionBody::OnContact(CollisionBody *) {
}

// Original 0x5a9db0.
void DynamicCollisionBody::ApplyImpulse(WheelVector *impulse, WheelVector *point) {
    body68->AddLimitedImpulse(impulse, point);
}

// Original 0x5931f0.
int DynamicCollisionBody::CanCollide(CollisionBody *) {
    if (!disabled17 && static_cast<WCollider *>(geometry60)->pack8) {
        return 1;
    }
    return 0;
}

// Original 0x5a9d80: inverse mass.
float DynamicCollisionBody::GetMass() {
    return body68->value168;
}

// Original 0x5a9d90.
WheelMatrix *DynamicCollisionBody::GetInverseInertia() {
    if (body68->flag158) {
        return &body68->matrix1B0;
    }
    return &body68->matrix170;
}

// Original 0x5a0a00.
float DynamicCollisionBody::Slot68(int a, int b) {
    MoverBody *body = body68;
    float linear = (body->vector70.x * body->vectorA0.x + body->vector70.y * body->vectorA0.y
                    + body->vector70.z * body->vectorA0.z) * collisionShapeHalf;
    float angular = (body->vector80.x * body->vectorC0.x + body->vector80.y * body->vectorC0.y
                     + body->vector80.z * body->vectorC0.z) * collisionShapeHalf;
    return MoverSlot68(body, a, b) - (angular + linear);
}

float DynamicCollisionBody::Slot88() {
    return CarSlot88(static_cast<CarCollisionBody *>(this));
}

// Original 0x5ae520: shape extents in body space.
void DynamicCollisionBody::MeasureShape() {
    float radius = 0.0f;
    float radiusXY = 0.0f;
    float highestZ = 0.0f;
    float lowestZ = 0.0f;
    float lowestX = 0.0f;
    float highestX = 0.0f;
    float lowestY = 0.0f;
    float highestY = 0.0f;
    float minimumZ = 0.0f;
    float maximumZ = 0.0f;
    CollisionVertex vertex;
    vertex.neighbourCount8 = 0;
    vertex.neighbourStartA = 0;
    vertex.indexC = 0;
    vertex.stampE = 0;
    vertex.point10[0] = 0.0f;
    vertex.point10[1] = 0.0f;
    vertex.point10[2] = 0.0f;
    for (int i = 0; i < shapeC->count14; ++i) {
        CollisionShapeVertex(shapeC, i, &vertex);
        float x = vertex.point10[0];
        float y = vertex.point10[1];
        float z = vertex.point10[2];
        float planar = x * x + y * y;
        float flat = (float)sqrt(planar);
        float full = (float)sqrt(z * z + planar);
        if (radius < full) {
            radius = full;
        }
        if (radiusXY < flat) {
            radiusXY = flat;
        }
        if (z <= highestZ) {
            if (z < lowestZ) {
                lowestZ = z;
            }
        } else {
            highestZ = z;
        }
        if (x <= highestX) {
            if (x < lowestX) {
                lowestX = x;
            }
        } else {
            highestX = x;
        }
        if (y <= highestY) {
            if (y < lowestY) {
                lowestY = y;
            }
        } else {
            highestY = y;
        }
        if (z <= maximumZ) {
            if (z < minimumZ) {
                minimumZ = z;
            }
        } else {
            maximumZ = z;
        }
    }
    radiusXY = collisionShapeMargin + radiusXY;
    radius1C = radius;
    radiusD8 = radiusXY;
    minimumE0.z = minimumZ;
    minimumE0.y = lowestY;
    upperD4 = collisionShapeMargin + highestZ;
    minimumE0.x = lowestX;
    maximumF0.z = maximumZ;
    maximumF0.y = highestY;
    lowerD0 = collisionShapeMargin - lowestZ;
    maximumF0.x = highestX;
    WheelVector low;
    WheelVector high;
    int type = shapeC->type10;
    if (type == 1) {
        static_cast<CollisionHull *>(shapeC)->GetBounds(&low, &high);
    } else if (type == 3) {
        CollisionBox *box = static_cast<CollisionBox *>(shapeC);
        low.x = box->centre20.x - box->extent30.x;
        low.y = box->centre20.y - box->extent30.y;
        low.z = box->centre20.z - box->extent30.z;
        high.x = box->extent30.x + box->centre20.x;
        high.y = box->extent30.y + box->centre20.y;
        high.z = box->extent30.z + box->centre20.z;
    } else if (type == 4) {
        CollisionPoint *point = static_cast<CollisionPoint *>(shapeC);
        low = point->point30;
        high = point->point30;
    }
    localCentre110.x = (high.x + low.x) * collisionShapeHalf;
    localCentre110.y = (high.y + low.y) * collisionShapeHalf;
    localCentre110.z = (high.z + low.z) * collisionShapeHalf;
}

// Original 0x5a9de0: XY bounds from the radius, and the shape centre in world space.
void DynamicCollisionBody::RefreshBounds() {
    WheelVector *position = GetPosition();
    float minimum[2];
    float maximum[2];
    minimum[0] = position->x - radius1C;
    minimum[1] = position->y - radius1C;
    maximum[0] = radius1C + position->x;
    maximum[1] = position->y + radius1C;
    SetBounds(minimum, maximum);
    WheelVector centre;
    WheelTransform(&centre, &matrix70, &localCentre110);
    centre100.x = centre.x + positionB0.x;
    centre100.y = positionB0.y + centre.y;
    centre100.z = positionB0.z + centre.z;
}

// Original 0x5937b0: can a polygon reach the body? Plane distance against the body's
// up-axis extent, then the planar distance to the outline.
bool DynamicCollisionBody::PolygonNear(WCollisionFace *face) {
    float lower = lowerD0;
    float radius = radius1C;
    WheelMatrix *matrix = GetMatrix();
    float up = matrix->elements[10] * face->normal70.z + matrix->elements[9] * face->normal70.y
             + matrix->elements[8] * face->normal70.x;
    if (!(up <= 1.0f)) {
        up = 1.0f;
    }
    float side = (float)sqrt(1.0f - up * up);
    WheelVector *position = GetPosition();
    float distance = ((position->z * face->normal70.z + position->y * face->normal70.y)
                      + position->x * face->normal70.x) - face->distance7C;
    float extent = up * distance < 0.0f ? upperD4 : lower;
    float reach = (up < 0.0f ? -up : up) * extent + side * radius;
    if (!((distance < 0.0f ? -distance : distance) < reach)) {
        return false;
    }
    float limit = radius1C + collisionGrowRadius;
    return face->PlanarDistance(&GetPosition()->x, limit) <= radius1C;
}

static int PolygonBucket(WCollisionFace *face) {
    return (int)((face->vertices30[1][1] + face->vertices30[0][0]) * collisionBucketScale) & 0xff;
}

// Original 0x5ae930: create TrackPolygonCollisionBodies for the collider-area faces the body
// can reach, skipping faces that already have one.
int DynamicCollisionBody::SpawnTrackPolygons() {
    WCollider *collider = static_cast<WCollider *>(geometry60);
    collider->UpdateArea(minimum20, maximum28, GetVelocity());
    int count = collider->areaCount2E0;
    int found = 0;
    WCollisionFace *faces[0x30];
    for (int i = 0; i < count; ++i) {
        WCollisionFace *face = collider->areaFaces3C[i];
        if (face->value10 != 0) {
            continue;
        }
        if (!(minimum20[0] <= face->maximumX1C) || maximum28[0] < face->minimumX14
            || !(minimum20[1] <= face->maximumY20) || maximum28[1] < face->minimumY18) {
            continue;
        }
        if (PolygonNear(face) && found < 0x30) {
            faces[found++] = face;
        }
    }
    if (!found) {
        return 0;
    }
    int swaps;
    do {
        swaps = 0;
        for (int i = 0; i < found - 1; ++i) {
            WCollisionFace *face = faces[i];
            if ((unsigned int)faces[i + 1]->source0C < (unsigned int)face->source0C) {
                faces[i] = faces[i + 1];
                faces[i + 1] = face;
                ++swaps;
            }
        }
    } while (swaps);
    for (int i = 0; i < found; ++i) {
        WCollisionFace *face = faces[i];
        ++trackPolygonScans86AF7C;
        if (!trackPolygonBuckets86AFA8[PolygonBucket(face)]) {
            continue;
        }
        ++trackPolygonBucketHits86AF80;
        for (WCollisionLink *link = collisionBodies86B4C8.next; link != &collisionBodies86B4C8; link = link->next) {
            CollisionBody *body = reinterpret_cast<CollisionBody *>(reinterpret_cast<char *>(link) - 4);
            if (body->type11 != 3) {
                continue;
            }
            WCollisionFace *other = static_cast<TrackPolygonCollisionBody *>(body)->face60;
            if (face->vertexCount9 != other->vertexCount9) {
                continue;
            }
            int matched = 0;
            while (matched < (signed char)face->vertexCount9
                   && VectorsNear(face->vertices30[matched], other->vertices30[matched], 0.001f)) {
                ++matched;
            }
            if (matched == (signed char)face->vertexCount9) {
                ++trackPolygonDuplicates86AF84;
                faces[i] = 0;
                break;
            }
        }
    }
    int created = 0;
    for (int i = 0; i < found; ++i) {
        if (!faces[i]) {
            continue;
        }
        if (bIsSlotPoolFull(trackPolygonBodyPool86AD68)) {
            return created;
        }
        WCollisionPolygon *polygon = faces[i]->source0C;
        new TrackPolygonCollisionBody((short)collisionIds86B2BC->next8, collider->pack8, polygon);
        ++created;
    }
    return created;
}

// ---------------------------------------------------------------------------
// CarCollisionBody

void *CarCollisionBody::operator new(unsigned int) {
    return bOMalloc(carCollisionBodyPool86AD60);
}

// Scalar deleting destructor 0x5b25b0.
void CarCollisionBody::operator delete(void *memory) {
    bFree(carCollisionBodyPool86AD60, memory);
}

// Original 0x5aec20.
CarCollisionBody::CarCollisionBody(MoverModeOwner *car, int id)
    : DynamicCollisionBody(id, 0, reinterpret_cast<char *>(car) + 0x5c4) {
    tick138 = 0;
    tick13C = 0;
    tick140 = 0;
    flag190 = 1;
    ++collisionCarCount86B260;
    if (collisionCarPeak86B288 < collisionCarCount86B260) {
        collisionCarPeak86B288 = collisionCarCount86B260;
    }
    car130 = car;
    moverIntegrator64 = 0;
    body68 = 0;
    active19 = 1;
    tick138 = collisionClock;
    tick13C = collisionClock;
    tick140 = collisionClock;
    char name[0x40];
    bSPrintf(name, collisionCarNameFormat, (int)*reinterpret_cast<short *>(reinterpret_cast<char *>(car) + 0xa));
    nfs_strncpy(name30, name, 0x28);
    name30[0x27] = 0;
    ownsShape10 = 0;
    carShape134 = 0;
    unsigned int key = CarCollisionHullKey(car->physicsInfo10);
    CollisionHull *hull = FindCollisionHull(key, CarCollisionCentre(car130));
    if (!hull) {
        key = CarCollisionBaseHullKey(car->physicsInfo10);
        hull = FindCollisionHull(key, CarCollisionCentre(car130));
        if (!hull) {
            CollisionBox *box = static_cast<CollisionBox *>(malloc(0x40));
            if (box) {
                box->type10 = 3;
                box->count14 = 8;
                box->Set(-2.217f, -0.971f, -0.2f, 2.165f, 0.971f, 0.624f);
                shapeC = box;
                ownsShape10 = 1;
            } else {
                shapeC = 0;
                ownsShape10 = 1;
            }
            MeasureShape();
            RefreshBounds();
            return;
        }
    }
    shapeC = hull;
    WheelVector *centre = reinterpret_cast<WheelVector *>(car->record20);
    hull->offset40 = *centre;
    hull->offsetValid3E = 1;
    WheelVector minimum;
    WheelVector maximum;
    hull->GetBounds(&minimum, &maximum);
    CollisionBox *box = static_cast<CollisionBox *>(malloc(0x40));
    if (box) {
        box->type10 = 3;
        box->count14 = 8;
        box->Set(minimum.x, minimum.y, minimum.z, maximum.x, maximum.y, maximum.z);
    }
    carShape134 = box;
    MeasureShape();
    RefreshBounds();
}

// Original 0x5aeee0.
CarCollisionBody::~CarCollisionBody() {
    --collisionCarCount86B260;
    if (ownsShape10) {
        if (shapeC->type10 == 1) {
            static_cast<CollisionHull *>(shapeC)->Release();
        }
        if (shapeC) {
            free(shapeC);
        }
    }
    if (carShape134) {
        free(carShape134);
    }
}

// Originals 0x593b40, 0x593b70, 0x593b90, 0x593bb0, 0x593bd0: the mover body's state while
// a PhysicsMover drives the car, the car state otherwise.
WheelVector *CarCollisionBody::GetPosition() {
    if (hasMover5C) {
        return &body68->position20;
    }
    return &car130->State()->position20;
}

WheelVector *CarCollisionBody::GetVelocity() {
    if (hasMover5C) {
        return &body68->vectorA0;
    }
    return &car130->State()->velocity30;
}

WheelVector *CarCollisionBody::GetSlot10() {
    if (hasMover5C) {
        return &body68->vectorB0;
    }
    return &car130->State()->value40;
}

WheelMatrix *CarCollisionBody::GetMatrix() {
    if (hasMover5C) {
        return &body68->matrix30;
    }
    return &car130->State()->matrix50;
}

WheelVector *CarCollisionBody::GetAngularVelocity() {
    if (hasMover5C) {
        return &body68->vectorC0;
    }
    return &car130->State()->angularD0;
}

// Original 0x593b60.
WheelVector *CarCollisionBody::GetShapeOffset() {
    return CarCollisionCentre(car130);
}

// Original 0x5a9f10: one step of the car (its update 0x6012b0 runs the mover).
void CarCollisionBody::Advance(float duration) {
    Snapshot(duration);
    UpdateCarForCollision(car130, duration);
    RefreshBounds();
}

// Original 0x593940.
void CarCollisionBody::OnContact(CollisionBody *other) {
    MoverModeOwner *otherCar = (other && other->type11 == 0) ? static_cast<CarCollisionBody *>(other)->car130 : 0;
    NotifyCarContact(car130, otherCar);
    AttachMover();
    active19 = 1;
}

// Original 0x5aa060.
float CarCollisionBody::Slot4C(WheelVector *, WheelVector *) {
    return 1.0f;
}

// Original 0x5a0b00: the impulse is scaled down for light cars (inverse mass above 1/1.85)
// and the angular part is reduced to 0.4 unless the car is in mover state 1.
void CarCollisionBody::ApplyImpulse(WheelVector *impulse, WheelVector *point) {
    float limit = 1.0f / collisionInverseMassLimit;
    float inverseMass = body68->value168;
    float scale = 1.0f;
    if (limit < inverseMass) {
        scale = limit / inverseMass;
    }
    WheelVector scaled;
    scaled.x = scale * impulse->x;
    scaled.y = impulse->y * scale;
    scaled.z = impulse->z * scale;
    float magnitude = (float)sqrt(scaled.z * scaled.z + scaled.y * scaled.y + scaled.x * scaled.x);
    float angular;
    if (car130->state38 == 1) {
        angular = 1.0f;
    } else {
        angular = collisionRearInertiaScale;
    }
    body68->AddScaledImpulse(&scaled, point, angular);
    // Original 0x593bf0 (force feedback) and 0x593c50 (empty consumer 0x402350).
    if (car130->forceFeedback958) {
        WheelMatrix transposed;
        WheelVector local;
        WheelTranspose(&transposed, &body68->matrix30);
        WheelTransform(&local, &transposed, point);
        CollisionForceFeedback(car130, &local, collisionForceFeedbackLimit * magnitude);
    }
}

// Original 0x593980.
int CarCollisionBody::CanCollide(CollisionBody *other) {
    int result = 0;
    if (!collisionDisabled81CB88) {
        if (other && other->type11 == 1 && WorldObjectKind(other) == 1) {
            if (other->CanCollide(this)) {
                result = DynamicCollisionBody::CanCollide(other) ? 1 : 0;
            }
        } else if (!disabled17 && static_cast<WCollider *>(geometry60)->pack8) {
            result = 1;
        }
    }
    if (!flag190 && other && other->type11 == 0) {
        result = 0;
    }
    return result;
}

// Original 0x5aeed0.
int CarCollisionBody::GetMaterial() {
    return 0x1c;
}

// Original 0x593a00: inverse mass, at most 1/1.85.
float CarCollisionBody::GetMass() {
    if (!body68) {
        return 0.0f;
    }
    float limit = 1.0f / collisionInverseMassLimit;
    float inverseMass = body68->value168;
    if (limit < inverseMass) {
        return limit;
    }
    return inverseMass;
}

// Original 0x593a40: the mover body's inverse inertia with its first three rows scaled
// (1.0 in mover state 1, else 0.4).
WheelMatrix *CarCollisionBody::GetInverseInertia() {
    float scale = car130->state38 == 1 ? 1.0f : collisionRearInertiaScale;
    MoverBody *body = body68;
    WheelMatrix *source = body->flag158 ? &body->matrix1B0 : &body->matrix170;
    for (int i = 0; i < 12; ++i) {
        inertia150.elements[i] = scale * source->elements[i];
    }
    for (int i = 12; i < 16; ++i) {
        inertia150.elements[i] = source->elements[i];
    }
    return &inertia150;
}

// Original 0x5aee80.
int CarCollisionBody::IsMovable() {
    return CarMoverFrozen(car130) == 0;
}

// Original 0x5aeea0.
int CarCollisionBody::Slot6C() {
    if (car130->IsResetPending() || car130->effectsSuppressed998) {
        return 1;
    }
    return 0;
}

// Original 0x5a0c90: restitution 0.1, halved against walls.
float CarCollisionBody::GetRestitution(CollisionBody *other) {
    float restitution = collisionRestitution;
    if (other && other->type11 == 3) {
        float z = static_cast<TrackPolygonCollisionBody *>(other)->face60->normal70.z;
        if ((z < 0.0f ? -z : z) < collisionWallNormalLimit) {
            restitution = collisionWallRestitutionScale * collisionRestitution;
        }
    }
    return restitution;
}

// Original 0x5a0be0: friction 0.3; against track polygons 0.09 on walls, 0.3 on slopes and
// up to 1.0 on floors depending on how many wheels are down.
float CarCollisionBody::GetFriction(CollisionBody *other) {
    float friction = collisionFriction;
    if (other && other->type11 == 3) {
        float z = static_cast<TrackPolygonCollisionBody *>(other)->face60->normal70.z;
        float steepness = z < 0.0f ? -z : z;
        if (steepness <= collisionFloorNormalLimit) {
            if (collisionWallNormalLimit <= steepness) {
                return friction;
            }
            friction = collisionSlopeFriction;
        } else {
            friction = (float)(4 - car130->value428) * collisionLandedFrictionScale
                     + (float)(4 - car130->mode424) * collisionGroundedFrictionScale + 1.0f;
        }
        friction = friction * collisionFriction;
        if (1.0f < friction) {
            friction = 1.0f;
        }
    }
    return friction;
}

// Original 0x5a0a80: with all four wheels down, track polygons (and other grounded cars)
// collide against the hull's bounding box.
CollisionShape *CarCollisionBody::GetShape(CollisionBody *other) {
    CollisionShape *shape = shapeC;
    if (car130->context14->value4 != 1 && carShape134 && other && car130->mode424 == 4) {
        if (other->type11 == 0) {
            MoverModeOwner *otherCar = static_cast<CarCollisionBody *>(other)->car130;
            if (otherCar->mode424 == 4 && otherCar->context14->value4 != 1) {
                shape = carShape134;
            }
        } else if (other->type11 == 3) {
            return carShape134;
        }
    }
    return shape;
}

// Original 0x59e530.
void CarCollisionBody::RemoveEvent(CollisionEvent *event) {
    CollisionBodyWitness *witness = event->witness64;
    MoverModeOwner *carA = witness->bodyA238->type11 == 0 ? static_cast<CarCollisionBody *>(witness->bodyA238)->car130 : 0;
    MoverModeOwner *carB = witness->bodyB23C->type11 == 0 ? static_cast<CarCollisionBody *>(witness->bodyB23C)->car130 : 0;
    if (carA || carB) {
        *reinterpret_cast<int *>(&inertia150.elements[4]) = collisionClock;
    }
    --events18;
}

// Original 0x5a0ae0.
float CarCollisionBody::Slot88() {
    if (car130->mode424 == 0) {
        return CarSlot88(this);
    }
    return 1.0f;
}

// ---------------------------------------------------------------------------
// TrackPolygonCollisionBody

void *TrackPolygonCollisionBody::operator new(unsigned int) {
    return bOMalloc(trackPolygonBodyPool86AD68);
}

// Scalar deleting destructor 0x5b25e0.
void TrackPolygonCollisionBody::operator delete(void *memory) {
    bFree(trackPolygonBodyPool86AD68, memory);
}

// Original 0x5aa0b0.
TrackPolygonCollisionBody::TrackPolygonCollisionBody(short id, WCollisionPack *pack, WCollisionPolygon *polygon)
    : CollisionBody(id, 3) {
    ++collisionStaticCount86B254;
    if (collisionStaticPeak86B27C < collisionStaticCount86B254) {
        collisionStaticPeak86B27C = collisionStaticCount86B254;
    }
    dynamic12 = 0;
    polygon70.type10 = 0;
    polygon70.count14 = 0;
    for (int i = 0; i < 9; ++i) {
        CollisionVertex *vertex = &polygon70.vertices20[i];
        vertex->neighbourCount8 = 0;
        vertex->neighbourStartA = 0;
        vertex->indexC = 0;
        vertex->stampE = 0;
        vertex->point10[0] = 0.0f;
        vertex->point10[1] = 0.0f;
        vertex->point10[2] = 0.0f;
    }
    polygon70.type10 = 2;
    polygon70.count14 = 0;
    polygon70.thickness1D4 = 0.0f;
    polygon70.thick1D0 = 0;
    ++collisionPolygonCount86B25C;
    if (collisionPolygonPeak86B284 < collisionPolygonCount86B25C) {
        collisionPolygonPeak86B284 = collisionPolygonCount86B25C;
    }
    WCollisionLink *node = bNewListNode();
    if (node) {
        *reinterpret_cast<CollisionBody **>(node + 1) = this;
    } else {
        node = 0;
    }
    WCollisionLink *tail = trackPolygonNodes86B5A0.previous;
    tail->next = node;
    trackPolygonNodes86B5A0.previous = node;
    node->previous = tail;
    node->next = &trackPolygonNodes86B5A0;
    node5C = node;
    shapeC = &polygon70;
    pack68 = pack;
    char name[0x18];
    bSPrintf(name, collisionPolygonNameFormat, (int)(polygon - pack->polygons8),
             CollisionSectionName(pack->section30));
    nfs_strncpy(name30, name, 0x28);
    name30[0x27] = 0;
    face60 = pack->GetFace(polygon);
    face60->value10 = reinterpret_cast<int>(this);
    grouped64 = polygon->group0 != 0;
    ++trackPolygonBuckets86AFA8[PolygonBucket(face60)];
    WheelVector origin;
    origin.x = 0.0f;
    origin.y = 0.0f;
    origin.z = 0.0f;
    polygon70.Build(face60, &origin, collisionPolygonThickness);
    SetBounds(&face60->minimumX14, &face60->maximumX1C);
    WheelMatrix unused;
    WheelIdentity(&unused);
}

// Original 0x5aefa0.
TrackPolygonCollisionBody::~TrackPolygonCollisionBody() {
    WCollisionLink *node = node5C;
    WCollisionLink *next = node->next;
    WCollisionLink *previous = node->previous;
    previous->next = next;
    next->previous = previous;
    bDeleteListNode(node);
    --trackPolygonBuckets86AFA8[PolygonBucket(face60)];
    face60->value10 = 0;
    face60->references8 = face60->references8 - 1;
    --collisionPolygonCount86B25C;
    --collisionStaticCount86B254;
}

// Polygon slots: a fixed world-space body (0x593d80, 0x5a9f40, 0x5a9f50, 0x5a9f60, 0x5a9fa0,
// 0x5a9ff0, 0x5aa040, 0x5aa050, 0x5aa060, 0x5aa070, 0x5aa080, 0x5b7680, 0x593d90).
WheelVector *TrackPolygonCollisionBody::GetPosition() {
    return &collisionZeroVector86B390;
}

WheelVector *TrackPolygonCollisionBody::GetVelocity() {
    return &collisionZeroVector86B390;
}

WheelVector *TrackPolygonCollisionBody::GetSlot10() {
    return &collisionZeroVector86B390;
}

WheelMatrix *TrackPolygonCollisionBody::GetMatrix() {
    return &collisionIdentity86B520;
}

WheelVector *TrackPolygonCollisionBody::GetAngularVelocity() {
    return &collisionZeroVector86B390;
}

WheelVector *TrackPolygonCollisionBody::GetShapeOffset() {
    return &collisionZeroVector86B390;
}

void TrackPolygonCollisionBody::SetPosition(WheelVector *) {
}

float TrackPolygonCollisionBody::GetRadius() {
    return radius1C;
}

float TrackPolygonCollisionBody::GetStepDuration() {
    return collisionPolygonDuration;
}

void TrackPolygonCollisionBody::Advance(float) {
}

void TrackPolygonCollisionBody::Rewind(float) {
}

void TrackPolygonCollisionBody::GetPoseAt(WheelVector *position, WheelMatrix *matrix, float) {
    BodyMatrixCopy(matrix, &collisionIdentity86B520);
    *position = collisionZeroVector86B390;
}

void TrackPolygonCollisionBody::SyncMover() {
}

void TrackPolygonCollisionBody::GetVelocities(WheelVector *velocity, WheelVector *angular) {
    *velocity = collisionZeroVector86B390;
    *angular = collisionZeroVector86B390;
}

void TrackPolygonCollisionBody::GetVelocitiesAt(WheelVector *velocity, WheelVector *angular, int, int) {
    *velocity = collisionZeroVector86B390;
    *angular = collisionZeroVector86B390;
}

void TrackPolygonCollisionBody::OnContact(CollisionBody *) {
}

float TrackPolygonCollisionBody::Slot4C(WheelVector *, WheelVector *) {
    return 1.0f;
}

void TrackPolygonCollisionBody::ApplyImpulse(WheelVector *, WheelVector *) {
}

int TrackPolygonCollisionBody::CanCollide(CollisionBody *) {
    return disabled17 == 0;
}

int TrackPolygonCollisionBody::GetMaterial() {
    return face60->surfaceB;
}

float TrackPolygonCollisionBody::GetMass() {
    return 0.0f;
}

WheelMatrix *TrackPolygonCollisionBody::GetInverseInertia() {
    return &collisionZeroMatrix86B450;
}

int TrackPolygonCollisionBody::IsMovable() {
    return 0;
}

float TrackPolygonCollisionBody::Slot68(int, int) {
    return 0.0f;
}

float TrackPolygonCollisionBody::GetRestitution(CollisionBody *) {
    return collisionPolygonResponse;
}

float TrackPolygonCollisionBody::GetFriction(CollisionBody *) {
    return collisionPolygonResponse;
}
