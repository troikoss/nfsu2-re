#include "WCollision.h"

// Records of collision faces near human cars, kept in an intrusive list at
// 0x88eadc and allocated from slot pool *0x883e5c. Only observed fields are named.
struct ColliderFaceRecord {
    ColliderFaceRecord *next0;
    ColliderFaceRecord *previous4;
    WCollisionPolygon *source8;
    int valueC;
    int time10;
    char unidentified14[0xc];
    float vertices20[5][4];
    ColliderFaceRecord *Initialise(WheelVector *position, WCollisionFace *face); // 0x5dc2a0
};

struct ColliderFaceRecordList {
    ColliderFaceRecord *first;
    ColliderFaceRecord *last;
    ColliderFaceRecord *End(); // 0x502fd0 (shared identical list helper)
};

extern ColliderFaceRecordList colliderFaceRecords88EADC;
extern void *colliderFaceRecordPool883E5C;
extern int colliderFaceRecordsEnabled883E60;
extern int frameTotal7FB720;
extern int colliderFaceSecondary802040;
extern float colliderFaceRaise;   // 8
#include "../../Libs/bWare/bSlotPool.h"

// 0x502fd0, identical to DriftRankList::End: the list object is its sentinel.
ColliderFaceRecord *ColliderFaceRecordList::End() {
    return reinterpret_cast<ColliderFaceRecord *>(this);
}

// 0x5dc2a0 copies the face's four vertices and normal (x as raw bits) and,
// for flag 4 faces, places the vertex heights 8 below/above the position.
ColliderFaceRecord *ColliderFaceRecord::Initialise(WheelVector *position, WCollisionFace *face) {
    time10 = frameTotal7FB720;
    source8 = face->source0C;
    valueC = 0;
    float *source = &face->vertices30[0][0];
    for (int i = 0; i < 5; ++i) {
        float y = source[i * 4 + 1];
        float z = source[i * 4 + 2];
        *reinterpret_cast<int *>(&vertices20[i][0]) = *reinterpret_cast<int *>(&source[i * 4]);
        vertices20[i][1] = y;
        vertices20[i][2] = z;
    }
    if (face->flagsA & 4) {
        vertices20[0][2] = (float)((double)position->z - colliderFaceRaise);
        vertices20[1][2] = (float)((double)position->z + colliderFaceRaise);
        vertices20[2][2] = (float)((double)position->z + colliderFaceRaise);
        vertices20[3][2] = (float)((double)position->z - colliderFaceRaise);
    }
    return this;
}

// 0x5dc360 refreshes a face's record or allocates and appends a new one.
// Faces with flag 2 and without flag 8 are recorded only while 0x802040 is
// set. An exhausted allocation still appends the null result, as the original.
void TrackColliderFace(WheelVector *position, WCollisionFace *face) {
    ColliderFaceRecord *record = colliderFaceRecords88EADC.first;
    ColliderFaceRecord *end = colliderFaceRecords88EADC.End();
    for (; record != end; record = record->next0) {
        if (record->source8 && face->source0C == record->source8) {
            record->time10 = frameTotal7FB720;
            return;
        }
    }
    if (bIsSlotPoolFull(colliderFaceRecordPool883E5C)) return;
    unsigned char flags = face->flagsA;
    if (!((flags & 2) && colliderFaceSecondary802040) && !(flags & 8) && (flags & 2)) return;
    ColliderFaceRecord *added = static_cast<ColliderFaceRecord *>(bSlotPoolAllocate(colliderFaceRecordPool883E5C));
    if (added) added = added->Initialise(position, face);
    else added = 0;
    ColliderFaceRecord *last = colliderFaceRecords88EADC.last;
    last->next0 = added;
    colliderFaceRecords88EADC.last = added;
    added->previous4 = last;
    added->next0 = reinterpret_cast<ColliderFaceRecord *>(&colliderFaceRecords88EADC);
}

// 0x5dc410 records every face in the collider's area when records are enabled.
void UpdateColliderFaceRecords(WCollider *collider, WheelVector *position) {
    if (colliderFaceRecordPool883E5C == 0 || colliderFaceRecordsEnabled883E60 == 0) return;
    for (int i = 0; i < collider->areaCount2E0; ++i) TrackColliderFace(position, collider->areaFaces3C[i]);
}
