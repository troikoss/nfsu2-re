#ifndef UG2_COLLISION_H
#define UG2_COLLISION_H

#include "../WheelMath.h"
#include "../Ug2Layout.h"
#include "../../World/WCollision.h"

// Partial PC x86 layouts of UG2's collision-body world. Class names come from the
// original slot-pool and serialisation strings ("CarCollisionBody",
// "TrackPolygonCollisionBody", "CollisionBodyWitness", "DynamicCollisionBody").

struct MoverBody;
struct MoverModeOwner;
class CollisionBody;
class CollisionBodyWitness;

// Shape vertex and support-point record, 0x30 bytes. Hull vertices double as list
// nodes during the hill-climbing support search (0x5afba0).
struct CollisionVertex {
    CollisionVertex *next0;
    CollisionVertex *previous4;
    short neighbourCount8;
    short neighbourStartA;
    short indexC;
    short stampE;
    float point10[4];
    float dot20;
    char unidentified24[0xc];
};

// Common header of the convex shapes; type 1 hull, 2 polygon, 3 box, 4 point, 5 cylinder.
struct CollisionShape {
    CollisionShape *next0;
    CollisionShape *previous4;
    char unidentified8[8];
    int type10;
    int count14;
};

// Car hull read in place from chunk 0x39200 (loader 0x5a1750).
struct CollisionHull : CollisionShape {
    unsigned int key18;
    char unidentified1C[0x20];
    short neighbourCount3C;
    unsigned char offsetValid3E;
    char unidentified3F;
    WheelVector offset40;
    char unidentified4C[4];
    short *neighbours50;
    CollisionVertex *vertices54;
    void GetVertex(int index, CollisionVertex *output);
    void GetBounds(WheelVector *minimum, WheelVector *maximum);
    CollisionVertex *Support(WheelVector *direction, CollisionVertex *start, WheelMatrix *matrix,
                             WheelMatrix *inverse, CollisionVertex *output);
    void Release();
};

// Fallback box (0x598760), malloc'd 0x40 bytes.
struct CollisionBox : CollisionShape {
    char unidentified18[8];
    WheelVector centre20;
    char unidentified2C[4];
    WheelVector extent30;
    char unidentified3C[4];
    void Set(float minimumX, float minimumY, float minimumZ, float maximumX, float maximumY,
             float maximumZ);
    void GetVertex(unsigned short index, CollisionVertex *output);
    CollisionVertex *Support(WheelVector *direction, CollisionVertex *start, WheelMatrix *matrix,
                             WheelMatrix *inverse, CollisionVertex *output);
};

struct CollisionPoint : CollisionShape {
    char unidentified18[0x14];
    short index2C;
    char unidentified2E[2];
    WheelVector point30;
    CollisionVertex *Support(WheelVector *direction, CollisionVertex *start, WheelMatrix *matrix,
                             WheelMatrix *inverse, CollisionVertex *output);
};

struct CollisionCylinder : CollisionShape {
    float halfHeight18;
    float radius1C;
    WheelVector centre20;
    char unidentified2C[4];
    int axis30;
    void GetVertex(int index, CollisionVertex *output);
    CollisionVertex *Support(WheelVector *direction, CollisionVertex *start, WheelMatrix *matrix,
                             WheelMatrix *inverse, CollisionVertex *output);
};

// Track polygon shape embedded in TrackPolygonCollisionBody +0x70, 0x200 bytes.
struct CollisionPolygon : CollisionShape {
    char unidentified18[8];
    CollisionVertex vertices20[9];
    unsigned char thick1D0;
    char unidentified1D1[3];
    float thickness1D4;
    char unidentified1D8[8];
    WheelVector centre1E0;
    char unidentified1EC[4];
    WheelVector centre1F0;
    char unidentified1FC[4];
    void Build(WCollisionFace *face, WheelVector *origin, float thickness);
    void GetVertex(int index, CollisionVertex *output);
    CollisionVertex *Support(WheelVector *direction, CollisionVertex *start, WheelMatrix *matrix,
                             WheelMatrix *inverse, CollisionVertex *output);
};

CollisionVertex *CollisionShapeVertex(CollisionShape *shape, int index, CollisionVertex *output);
CollisionHull *FindCollisionHull(unsigned int key, WheelVector *offset);
WheelVector *CarCollisionCentre(MoverModeOwner *car);
int LoadCollisionHulls(int *chunk);

// GJK simplex state, 0xdc bytes, copied whole by 0x598130/0x598240.
struct CollisionSimplex {
    float a0[4][4];
    float b40[4][4];
    WheelVector closestA80;
    char unidentified8C[4];
    WheelVector closestB90;
    char unidentified9C[4];
    WheelVector normalA0;
    char unidentifiedAC[4];
    int indicesB0[4];
    int countC0;
    int indicesC4[4];
    int countD4;
    int startedD8;
    void CopyFrom(CollisionSimplex *source);
    void Assign(CollisionSimplex *source);
    float Distance(WheelVector *output);
};

// Distance witness between two shapes.
class CollisionWitness {
public:
    virtual ~CollisionWitness();
    char unidentified4[0xc];
    float distance10;
    unsigned char touching14;
    unsigned char near15;
    char unidentified16[0xa];
    WheelVector positionA20;
    char unidentified2C[4];
    WheelVector positionB30;
    char unidentified3C[4];
    CollisionSimplex simplex40;
    char unidentified11C[4];
    WheelMatrix matrixA120;
    WheelMatrix inverseA160;
    WheelMatrix matrixB1A0;
    WheelMatrix inverseB1E0;
    unsigned char dirty220;
    unsigned char thick221;
    char unidentified222[2];
    CollisionShape *shapeA224;
    CollisionShape *shapeB228;
    char unidentified22C[4];
    void SetShapes(CollisionShape *shapeA, CollisionShape *shapeB);
    void SetPoses(WheelMatrix *matrixA, WheelVector *positionA, WheelMatrix *matrixB,
                  WheelVector *positionB);
    float Update();
    float ComputeDistance();
};

struct CollisionEvent;

// Body pair, pool 0x86b0bc (0x250 x 0x80), vtable 0x7a1fd8.
class CollisionBodyWitness : public CollisionWitness {
public:
    CollisionBodyWitness(CollisionBody *bodyA, CollisionBody *bodyB);
    virtual ~CollisionBodyWitness();
    static void *operator new(unsigned int size);
    static void operator delete(void *memory);
    WCollisionLink link230;
    CollisionBody *bodyA238;
    CollisionBody *bodyB23C;
    int passes240;
    CollisionEvent *event244;
    int side248;
    char unidentified24C[4];
    bool IsStale();
    void Test(WCollisionLink *contacts);
    void MarkThickPolygon(char keep);
    void ClearThickPolygon();
    void CreateEvent(WheelVector *point);
    void ReleaseEvent();
};

// Pending contact, ContactSlotPool 0x86b2cc (0x40 x 0x30).
struct CollisionContact {
    WCollisionLink link0;
    float time8;
    CollisionBody *bodyAC;
    CollisionBody *bodyB10;
    CollisionBodyWitness *witness14;
    char unidentified18[8];
    WheelVector offset20;
    char unidentified2C[4];
    int value30;
    char unidentified34[0xc];
    CollisionContact *Construct(CollisionBody *bodyA, CollisionBody *bodyB, CollisionBodyWitness *witness);
    float FindImpactTime();
    void Separate(float time);
    void ContactFrame(WheelVector *point, WheelVector *normal);
    float SortKey(int *valid);
    void Resolve();
    void ApplyImpulse();
};

// Contact event, CollisionSlotPool 0x86b240 (0x80 x 0x20); the mover's contact bodies.
struct CollisionEvent {
    WCollisionLink link0;
    int tick8;
    int tickC;
    int value10;
    char unidentified14[0xc];
    WheelVector point20;
    char unidentified2C[4];
    WheelVector position30;
    char unidentified3C[4];
    float speed40;
    char unidentified44[0xc];
    WheelVector impulse50;
    char unidentified5C[4];
    float magnitude60;
    CollisionBodyWitness *witness64;
    unsigned char flag68;
    char unidentified69[7];
    WheelVector direction70;
    char unidentified7C[4];
    CollisionEvent *Construct(CollisionBodyWitness *witness, WheelVector *point);
    void Destroy();
    void Accumulate(WheelVector *point, WheelVector *impulse, float speed);
    bool IsTouching();
    bool IsWallContact();
};

// Base collision body, 0x5c bytes, vtable 0x7a1fe0. Slot order follows the original
// vtables; names describe the observed behaviour.
class CollisionBody {
public:
    CollisionBody(int id, char type);
    virtual ~CollisionBody();                                                  // 0x00
    virtual void Serialise(void *stream);                                      // 0x04
    virtual WheelVector *GetPosition() = 0;                                    // 0x08
    virtual WheelVector *GetVelocity() = 0;                                    // 0x0c
    virtual WheelVector *GetSlot10() = 0;                                      // 0x10
    virtual WheelMatrix *GetMatrix() = 0;                                      // 0x14
    virtual WheelVector *GetAngularVelocity() = 0;                             // 0x18
    virtual WheelVector *GetShapeOffset() = 0;                                 // 0x1c
    virtual void SetPosition(WheelVector *position) = 0;                       // 0x20
    virtual float GetRadius() = 0;                                             // 0x24
    virtual float GetStepDuration() = 0;                                       // 0x28
    virtual void Advance(float duration) = 0;                                  // 0x2c
    virtual void Rewind(float time) = 0;                                       // 0x30
    virtual void GetPoseAt(WheelVector *position, WheelMatrix *matrix, float time) = 0;  // 0x34
    virtual void SyncMover() = 0;                                              // 0x38
    virtual void GetVelocities(WheelVector *velocity, WheelVector *angular) = 0;        // 0x3c
    virtual void GetVelocitiesAt(WheelVector *velocity, WheelVector *angular, int a, int b) = 0;  // 0x40
    virtual void Slot44(int value);                                            // 0x44
    virtual void OnContact(CollisionBody *other) = 0;                          // 0x48
    virtual float Slot4C(WheelVector *impulse, WheelVector *point) = 0;        // 0x4c
    virtual void ApplyImpulse(WheelVector *impulse, WheelVector *point) = 0;   // 0x50
    virtual int CanCollide(CollisionBody *other) = 0;                          // 0x54
    virtual int GetMaterial() = 0;                                             // 0x58
    virtual float GetMass() = 0;                                               // 0x5c
    virtual WheelMatrix *GetInverseInertia() = 0;                              // 0x60
    virtual int IsMovable() = 0;                                               // 0x64
    virtual float Slot68(int a, int b) = 0;                                    // 0x68
    virtual int Slot6C();                                                      // 0x6c
    virtual float GetRestitution(CollisionBody *other) = 0;                    // 0x70
    virtual float GetFriction(CollisionBody *other) = 0;                       // 0x74
    virtual void Slot78(float time, WheelVector *position, WheelMatrix *matrix, WheelVector *velocity);  // 0x78
    virtual CollisionShape *GetShape(CollisionBody *other);                    // 0x7c
    virtual void AddEvent(CollisionEvent *event);                              // 0x80
    virtual void RemoveEvent(CollisionEvent *event);                           // 0x84

    WCollisionLink link4;
    CollisionShape *shapeC;
    unsigned char ownsShape10;
    signed char type11;
    unsigned char dynamic12;
    char unidentified13;
    short id14;
    unsigned char witnesses16;
    unsigned char disabled17;
    signed char events18;
    unsigned char active19;
    char unidentified1A[2];
    float radius1C;
    float minimum20[2];
    float maximum28[2];
    char name30[0x28];
    int value58;

    void SetBounds(float *minimum, float *maximum);
    void UpdateLargestWidth(float width);
    bool Overlaps(CollisionBody *other, float margin);
    void FindPairs();
    CollisionEvent *GetEvent(int index);
};

// DynamicCollisionBody, 0x128 bytes, vtable 0x7a2168 (constructor 0x5a9ce0).
class DynamicCollisionBody : public CollisionBody {
public:
    DynamicCollisionBody(int id, char type, void *geometry);
    virtual ~DynamicCollisionBody();
    virtual void Serialise(void *stream);
    virtual void SetPosition(WheelVector *position);
    virtual float GetRadius();
    virtual float GetStepDuration();
    virtual void Rewind(float time);
    virtual void GetPoseAt(WheelVector *position, WheelMatrix *matrix, float time);
    virtual void SyncMover();
    virtual void GetVelocities(WheelVector *velocity, WheelVector *angular);
    virtual void GetVelocitiesAt(WheelVector *velocity, WheelVector *angular, int a, int b);
    virtual void Slot44(int value);
    virtual void OnContact(CollisionBody *other);
    virtual void ApplyImpulse(WheelVector *impulse, WheelVector *point);
    virtual int CanCollide(CollisionBody *other);
    virtual float GetMass();
    virtual WheelMatrix *GetInverseInertia();
    virtual float Slot68(int a, int b);
    virtual float Slot88();                                                    // 0x88

    int hasMover5C;
    void *geometry60;
    void *moverIntegrator64;
    MoverBody *body68;
    float duration6C;
    WheelMatrix matrix70;
    WheelVector positionB0;
    char unidentifiedBC[4];
    WheelVector velocityC0;
    char unidentifiedCC[4];
    float lowerD0;
    float upperD4;
    float radiusD8;
    char unidentifiedDC[4];
    WheelVector minimumE0;
    char unidentifiedEC[4];
    WheelVector maximumF0;
    char unidentifiedFC[4];
    WheelVector centre100;
    char unidentified10C[4];
    WheelVector localCentre110;
    char unidentified11C[4];
    int wallContacts120;
    int carContacts124;

    int AttachMover();
    void Snapshot(float duration);
    void GetRewoundPosition(WheelVector *velocity, float time);
    void MeasureShape();
    void RefreshBounds();
    void GetShapeBounds(WheelVector *minimum, WheelVector *maximum);
    int HasTrackGeometry();
    int SpawnTrackPolygons();
    bool PolygonNear(WCollisionFace *face);
};

// CarCollisionBody, 0x1a0 bytes, pool 0x86ad60, vtable 0x7a2470 (constructor 0x5aec20).
class CarCollisionBody : public DynamicCollisionBody {
public:
    CarCollisionBody(MoverModeOwner *car, int id);
    virtual ~CarCollisionBody();
    static void *operator new(unsigned int size);
    static void operator delete(void *memory);
    virtual void Serialise(void *stream);
    virtual WheelVector *GetPosition();
    virtual WheelVector *GetVelocity();
    virtual WheelVector *GetSlot10();
    virtual WheelMatrix *GetMatrix();
    virtual WheelVector *GetAngularVelocity();
    virtual WheelVector *GetShapeOffset();
    virtual void Advance(float duration);
    virtual void OnContact(CollisionBody *other);
    virtual float Slot4C(WheelVector *impulse, WheelVector *point);
    virtual void ApplyImpulse(WheelVector *impulse, WheelVector *point);
    virtual int CanCollide(CollisionBody *other);
    virtual int GetMaterial();
    virtual float GetMass();
    virtual WheelMatrix *GetInverseInertia();
    virtual int IsMovable();
    virtual int Slot6C();
    virtual float GetRestitution(CollisionBody *other);
    virtual float GetFriction(CollisionBody *other);
    virtual CollisionShape *GetShape(CollisionBody *other);
    virtual void RemoveEvent(CollisionEvent *event);
    virtual float Slot88();

    char unidentified128[8];
    MoverModeOwner *car130;
    CollisionShape *carShape134;
    int tick138;
    int tick13C;
    int tick140;
    char unidentified144[0xc];
    WheelMatrix inertia150;
    unsigned char flag190;
    char unidentified191[0xf];
};

// TrackPolygonCollisionBody, 0x270 bytes, pool 0x86ad68, vtables 0x7a2208/0x7a22a8.
class TrackPolygonCollisionBody : public CollisionBody {
public:
    TrackPolygonCollisionBody(short id, WCollisionPack *pack, WCollisionPolygon *polygon);
    virtual ~TrackPolygonCollisionBody();
    static void *operator new(unsigned int size);
    static void operator delete(void *memory);
    virtual void Serialise(void *stream);
    virtual WheelVector *GetPosition();
    virtual WheelVector *GetVelocity();
    virtual WheelVector *GetSlot10();
    virtual WheelMatrix *GetMatrix();
    virtual WheelVector *GetAngularVelocity();
    virtual WheelVector *GetShapeOffset();
    virtual void SetPosition(WheelVector *position);
    virtual float GetRadius();
    virtual float GetStepDuration();
    virtual void Advance(float duration);
    virtual void Rewind(float time);
    virtual void GetPoseAt(WheelVector *position, WheelMatrix *matrix, float time);
    virtual void SyncMover();
    virtual void GetVelocities(WheelVector *velocity, WheelVector *angular);
    virtual void GetVelocitiesAt(WheelVector *velocity, WheelVector *angular, int a, int b);
    virtual void OnContact(CollisionBody *other);
    virtual float Slot4C(WheelVector *impulse, WheelVector *point);
    virtual void ApplyImpulse(WheelVector *impulse, WheelVector *point);
    virtual int CanCollide(CollisionBody *other);
    virtual int GetMaterial();
    virtual float GetMass();
    virtual WheelMatrix *GetInverseInertia();
    virtual int IsMovable();
    virtual float Slot68(int a, int b);
    virtual float GetRestitution(CollisionBody *other);
    virtual float GetFriction(CollisionBody *other);

    WCollisionLink *node5C;
    WCollisionFace *face60;
    unsigned char grouped64;
    char unidentified65[3];
    WCollisionPack *pack68;
    char unidentified6C[4];
    CollisionPolygon polygon70;
};

// Object at 0x890080 whose +8 list holds the live contact events.
struct CollisionEventOwner {
    WCollisionLink unidentified0;
    WCollisionLink events8;
};

UG2_CHECK_SIZE(CollisionVertex, 0x30);
UG2_CHECK_OFFSET(CollisionHull, offset40, 0x40);
UG2_CHECK_OFFSET(CollisionHull, vertices54, 0x54);
UG2_CHECK_SIZE(CollisionBox, 0x40);
UG2_CHECK_OFFSET(CollisionCylinder, axis30, 0x30);
UG2_CHECK_OFFSET(CollisionPolygon, thick1D0, 0x1d0);
UG2_CHECK_OFFSET(CollisionPolygon, centre1F0, 0x1f0);
UG2_CHECK_SIZE(CollisionPolygon, 0x200);
UG2_CHECK_OFFSET(CollisionSimplex, indicesB0, 0xb0);
UG2_CHECK_OFFSET(CollisionSimplex, countD4, 0xd4);
UG2_CHECK_SIZE(CollisionSimplex, 0xdc);
UG2_CHECK_OFFSET(CollisionWitness, distance10, 0x10);
UG2_CHECK_OFFSET(CollisionWitness, simplex40, 0x40);
UG2_CHECK_OFFSET(CollisionWitness, matrixA120, 0x120);
UG2_CHECK_OFFSET(CollisionWitness, inverseB1E0, 0x1e0);
UG2_CHECK_OFFSET(CollisionWitness, shapeA224, 0x224);
UG2_CHECK_OFFSET(CollisionBodyWitness, link230, 0x230);
UG2_CHECK_OFFSET(CollisionBodyWitness, event244, 0x244);
UG2_CHECK_SIZE(CollisionBodyWitness, 0x250);
UG2_CHECK_OFFSET(CollisionContact, offset20, 0x20);
UG2_CHECK_SIZE(CollisionContact, 0x40);
UG2_CHECK_OFFSET(CollisionEvent, impulse50, 0x50);
UG2_CHECK_OFFSET(CollisionEvent, witness64, 0x64);
UG2_CHECK_SIZE(CollisionEvent, 0x80);
UG2_CHECK_OFFSET(CollisionBody, shapeC, 0xc);
UG2_CHECK_OFFSET(CollisionBody, id14, 0x14);
UG2_CHECK_OFFSET(CollisionBody, radius1C, 0x1c);
UG2_CHECK_OFFSET(CollisionBody, name30, 0x30);
UG2_CHECK_SIZE(CollisionBody, 0x5c);
UG2_CHECK_OFFSET(DynamicCollisionBody, body68, 0x68);
UG2_CHECK_OFFSET(DynamicCollisionBody, matrix70, 0x70);
UG2_CHECK_OFFSET(DynamicCollisionBody, lowerD0, 0xd0);
UG2_CHECK_OFFSET(DynamicCollisionBody, minimumE0, 0xe0);
UG2_CHECK_OFFSET(DynamicCollisionBody, localCentre110, 0x110);
UG2_CHECK_SIZE(DynamicCollisionBody, 0x128);
UG2_CHECK_OFFSET(CarCollisionBody, car130, 0x130);
UG2_CHECK_OFFSET(CarCollisionBody, inertia150, 0x150);
UG2_CHECK_OFFSET(CarCollisionBody, flag190, 0x190);
UG2_CHECK_SIZE(CarCollisionBody, 0x1a0);
UG2_CHECK_OFFSET(TrackPolygonCollisionBody, face60, 0x60);
UG2_CHECK_OFFSET(TrackPolygonCollisionBody, polygon70, 0x70);
UG2_CHECK_SIZE(TrackPolygonCollisionBody, 0x270);

// Globals (OriginalSymbols.ld binds them to their original addresses).
extern WCollisionLink collisionBodies86B4C8;
extern WCollisionLink collisionWitnesses86B490;
extern WCollisionLink trackPolygonNodes86B5A0;
extern WCollisionLink collisionHulls86B598;
extern float collisionLargestWidth86B29C;
extern unsigned char *collisionPairBits86B0B0;
extern CollisionBody **collisionBodyTable86B298;
struct CollisionIdAllocator {
    int count0;
    unsigned char *bits4;
    int next8;
    void Take(int id);
};
extern CollisionIdAllocator *collisionIds86B2BC;
extern unsigned char trackPolygonBuckets86AFA8[0x100];
extern CollisionEventOwner *collisionEventOwner890080;
extern WheelVector collisionZeroVector86B390;
extern WheelMatrix collisionIdentity86B520;
extern WheelMatrix collisionZeroMatrix86B450;
extern void *carCollisionBodyPool86AD60;
extern void *trackPolygonBodyPool86AD68;
extern void *collisionWitnessPool86B0BC;
extern void *collisionContactPool86B2CC;
extern void *collisionEventPool86B240;
extern void *worldObjectBodyPool86AD6C;

void InitialiseCollisionPools();
void InitialiseContactPool();
void InitialiseCollisionEventPool();
void RunCollisionStaticInitializers();
void AdvanceCollisionBodies(float duration);
void UpdateCollisionWorld(CollisionEventOwner *owner, float duration);
int InsertContactSorted(WCollisionLink *list, CollisionContact *contact);
void RemoveBodyContacts(WCollisionLink *list, CollisionBody *body);
void DeleteOrphanTrackPolygons();

// Platform seams: original routines reached from the collision code that are not part of
// this recovery (car update 0x6012b0, effects/sound 0x59fb70.., force feedback 0x637c70).
float UpdateCarForCollision(MoverModeOwner *car, float duration);
void CollisionEffects(CollisionEvent *event, WheelVector *point, WheelVector *impulse, float speed);
void CollisionForceFeedback(MoverModeOwner *car, WheelVector *localImpulse, float magnitude);
void *bOMalloc(void *pool);
void bFree(void *pool, void *memory);
int bIsSlotPoolFull(void *pool);
void *bNewSlotPool(int size, int count, const char *name, int flags);
WCollisionLink *bNewListNode();
void bDeleteListNode(WCollisionLink *node);
unsigned int bGetTicker();
unsigned int bRandom(unsigned int range);

#endif
