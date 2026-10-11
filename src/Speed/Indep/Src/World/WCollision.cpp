#include "WCollision.h"

extern "C" double __cdecl sqrt(double);
#pragma intrinsic(sqrt)

extern float collisionCoordinateScale;
extern float collisionCellScale;
extern float collisionCellSize;
extern float collisionSubcellScale;
extern float collisionFixedScale;
extern float collisionStepLimit;
extern float collisionNegativeOne;
extern float collisionAreaRatio;
extern float collisionAreaMargin;
extern float sectionNearLimit;
extern int collisionRelocation864F60;
extern int topologyStreaming864F7C;

void *bOMalloc(void *pool);
void *bNewSlotPool(int size, int count, const char *name, int flags);
void FillColliderArea(WCollider *collider, float *minimum, float *maximum);

static void Unlink(WCollisionLink *link) {
    WCollisionLink *next = link->next;
    WCollisionLink *previous = link->previous;
    previous->next = next;
    next->previous = previous;
}

static void AppendLink(WCollisionLink *head, WCollisionLink *link) {
    WCollisionLink *tail = head->previous;
    tail->next = link;
    head->previous = link;
    link->previous = tail;
    link->next = head;
}

static void PrependLink(WCollisionLink *head, WCollisionLink *link) {
    WCollisionLink *first = head->next;
    head->next = link;
    first->previous = link;
    link->previous = head;
    link->next = first;
}

// Original 0x43be80: list length (counts up to and including `stop` when found).
static int CountLinks(WCollisionLink *head, WCollisionLink *stop) {
    int count = 0;
    for (WCollisionLink *link = head->next; link != head; link = link->next) {
        ++count;
        if (link == stop) {
            return count;
        }
    }
    return stop ? 0 : count;
}

// Original 0x43d410: 2D bounds test with a margin.
static int InsideBounds(float *minimum, float *maximum, float *point, float margin) {
    return minimum[0] - margin <= point[0] && point[0] <= margin + maximum[0]
        && minimum[1] - margin <= point[1] && point[1] <= margin + maximum[1];
}

// Original 0x43ce50: 2D normalize; a zero vector becomes (1, 0).
static void Normalize2(float *output, float *input) {
    float length = (float)sqrt(input[0] * input[0] + input[1] * input[1]);
    if (length != 0.0f) {
        float y = input[1];
        float inverse = 1.0f / length;
        output[0] = inverse * input[0];
        output[1] = inverse * y;
        return;
    }
    output[0] = 1.0f;
    output[1] = 0.0f;
}

// Original 0x5d6dd0 (point passed in EAX): even-odd crossing test.
static bool PointInPolygon(float *point, float (*points)[2], int count) {
    float x = point[0], y = point[1];
    bool inside = false;
    for (int i = 0, j = count - 1; i < count; j = i++) {
        if (((points[i][1] <= y && y < points[j][1]) || (points[j][1] <= y && y < points[i][1]))
            && x < (y - points[i][1]) * (points[j][0] - points[i][0]) / (points[j][1] - points[i][1])
                   + points[i][0]) {
            inside = !inside;
        }
    }
    return inside;
}

// Distance from a point to a section outline (shared body of 0x5d7270/0x5d70d0).
static float OutlineDistance(TrackSection *section, float *point, float initial) {
    float best = initial;
    int count = section->pointCountA;
    float *vertex = section->points24[0];
    for (int i = 0; i < count;) {
        ++i;
        float dx = point[0] - vertex[0];
        float dy = point[1] - vertex[1];
        float edge[2];
        edge[1] = section->points24[i % count][1] - vertex[1];
        edge[0] = section->points24[i % count][0] - vertex[0];
        float length = (float)sqrt(edge[0] * edge[0] + edge[1] * edge[1]);
        Normalize2(edge, edge);
        float along = dx * edge[0] + dy * edge[1];
        float across = dx * edge[1] + -edge[0] * dy;
        float distance;
        if (0.0f <= along) {
            if (along <= length) {
                distance = across < 0.0f ? -across : across;
            } else {
                along = along - length;
                distance = (float)sqrt(across * across + along * along);
            }
        } else {
            distance = (float)sqrt(along * along + across * across);
        }
        if (distance < best) {
            best = distance;
        }
        count = section->pointCountA;
        vertex += 2;
    }
    return best;
}

// Original 0x5d7090.
bool TrackSection::Contains(float *point) {
    if (!InsideBounds(minimumC, maximum14, point, 0.0f)) {
        return false;
    }
    return PointInPolygon(point, points24, pointCountA);
}

// Original 0x5d7270: distance to the outline when inside, else zero.
float TrackSection::InteriorDistance(float *point) {
    if (InsideBounds(minimumC, maximum14, point, 0.0f) && PointInPolygon(point, points24, pointCountA)) {
        float dy = point[1] - points24[0][1];
        float dx = point[0] - points24[0][0];
        float start = (float)sqrt(dx * dx + dy * dy);
        return OutlineDistance(this, point, start);
    }
    return 0.0f;
}

// Original 0x5d70d0: distance to the section (zero inside), limited by `limit`.
float TrackSection::DistanceTo(float *point, float limit) {
    if (!InsideBounds(minimumC, maximum14, point, limit)) {
        return limit;
    }
    if (InsideBounds(minimumC, maximum14, point, 0.0f) && PointInPolygon(point, points24, pointCountA)) {
        return 0.0f;
    }
    return OutlineDistance(this, point, limit);
}

// Original 0x5db850: containing section, moved to the list front.
TrackSection *TrackTopology::FindSection(float *point) {
    for (WCollisionLink *link = sections0.next; link != &sections0; link = link->next) {
        TrackSection *section = reinterpret_cast<TrackSection *>(link);
        if (InsideBounds(section->minimumC, section->maximum14, point, 0.0f)
            && PointInPolygon(point, section->points24, section->pointCountA)) {
            Unlink(link);
            PrependLink(&sections0, link);
            return section;
        }
    }
    float distance;
    TrackSection *nearest = FindNearest(point, &distance);
    if (sectionNearLimit <= distance) {
        return 0;
    }
    return nearest;
}

// Original 0x5db770: containing or nearest section.
TrackSection *TrackTopology::FindNearest(float *point, float *distance) {
    TrackSection *best = 0;
    float bestDistance = 9999999.0f;
    for (WCollisionLink *link = sections0.next; link != &sections0; link = link->next) {
        TrackSection *section = reinterpret_cast<TrackSection *>(link);
        if (InsideBounds(section->minimumC, section->maximum14, point, 0.0f)
            && PointInPolygon(point, section->points24, section->pointCountA)) {
            Unlink(link);
            PrependLink(&sections0, link);
            bestDistance = 0.0f;
            best = section;
            break;
        }
        float candidate = section->DistanceTo(point, bestDistance);
        if (!best || candidate < bestDistance || (candidate == bestDistance && section->id8 < best->id8)) {
            bestDistance = candidate;
            best = section;
        }
    }
    if (distance) {
        *distance = bestDistance;
    }
    return best;
}

// Original 0x5e9c10: topology chunk 0x80034150.
int TrackTopology::LoadChunk(int *chunk) {
    if (*chunk != (int)0x80034150) {
        return 0;
    }
    int *end = reinterpret_cast<int *>(reinterpret_cast<char *>(chunk) + chunk[1] + 8);
    int *child = chunk + 2;
    while (child < end) {
        int id = child[0];
        if (id == 0x34152) {
            char *limit = reinterpret_cast<char *>(child) + child[1] + 8;
            for (char *record = reinterpret_cast<char *>(child + 2); record < limit;) {
                TrackSection *section = reinterpret_cast<TrackSection *>(record);
                int number = section->id8 % 100;
                char letter = (char)(section->id8 / 100) + '@';
                if (letter < 'A' || letter > 'T' || number < 1 || sectionNumberOffset <= number) {
                    AppendLink(&otherSections8, &section->link0);
                } else {
                    AppendLink(&sections0, &section->link0);
                }
                record += section->pointCountA * 8 + 0x24;
            }
        } else if (id == 0x34153) {
            unsigned int count = (unsigned int)child[1] / 0xa4;
            int *record = child + 2;
            for (; count != 0; --count) {
                AppendLink(&records10, reinterpret_cast<WCollisionLink *>(record));
                TrackSection *match = 0;
                for (WCollisionLink *link = sections0.next; link != &sections0; link = link->next) {
                    if (reinterpret_cast<TrackSection *>(link)->id8 == (short)record[2]) {
                        match = reinterpret_cast<TrackSection *>(link);
                        break;
                    }
                }
                if (!match) {
                    for (WCollisionLink *link = otherSections8.next; link != &otherSections8; link = link->next) {
                        if (reinterpret_cast<TrackSection *>(link)->id8 == (short)record[2]) {
                            match = reinterpret_cast<TrackSection *>(link);
                            break;
                        }
                    }
                }
                record[3] = (int)match;
                record += 0x29;
            }
        } else if (id == 0x34151) {
            header4C = child + 2;
            sectionNumberOffset = child[2];
        } else if (id == 0x34154) {
            unsigned int count = (unsigned int)child[1] >> 5;
            for (int *record = child + 2; count != 0; --count, record += 8) {
                AppendLink(&records28, reinterpret_cast<WCollisionLink *>(record));
            }
        } else if (id == 0x34155) {
            unsigned int count = (unsigned int)child[1] / 0x4c;
            for (int *record = child + 2; count != 0; --count, record += 0x13) {
                AppendLink(&records20, reinterpret_cast<WCollisionLink *>(record));
            }
        }
        child = reinterpret_cast<int *>(reinterpret_cast<char *>(child) + child[1] + 8);
    }
    if (topologyStreaming864F7C != 0) {
        // 0x5dfd30 and the streamer refresh run here when streaming is active (not recovered).
        return -1;
    }
    return 1;
}

// Original 0x5dfeb0: refresh the current section when the point leaves the safe radius.
bool WSectionTracker::Update(float *point, TrackTopology *topology, float radius) {
    float dx = point[0] - x0;
    float dy = point[1] - y4;
    if (!(radius + radiusC < (float)sqrt(dx * dx + dy * dy))) {
        return false;
    }
    y4 = point[1];
    x0 = point[0];
    if (section8 && section8->Contains(reinterpret_cast<float *>(this))) {
        radiusC = section8->InteriorDistance(reinterpret_cast<float *>(this));
        return false;
    }
    section8 = topology->FindSection(reinterpret_cast<float *>(this));
    if (section8) {
        radiusC = section8->InteriorDistance(reinterpret_cast<float *>(this));
        return true;
    }
    topology->FindNearest(reinterpret_cast<float *>(this), &radiusC);
    return true;
}

// Original 0x5d3c10: section number to pack table index (20 slots per letter).
int CollisionSectionIndex(short section) {
    if (section == 0) {
        return 0;
    }
    int number = section % 100;
    if (sectionNumberOffset <= number && number < sectionNumberOffset * 2) {
        section = (short)(section - sectionNumberOffset);
    }
    return section % 100 + ((char)((char)(section / 100) + '@') - 'A') * 20;
}

// Original 0x591380.
WCollisionPack *WCollisionManager::GetPack(short section) {
    return sections8[CollisionSectionIndex(section)];
}

// Original 0x5d2cc0: decode four vertices from fixed-point coordinates.
void WCollisionPolygon::Decode(float *vertices) {
    for (int i = 0; i < 4; ++i) {
        float *vertex = vertices + i * 4;
        vertex[0] = (float)((int)x8[i] << 13) * collisionCoordinateScale;
        vertex[1] = (float)((int)y10[i] << 13) * collisionCoordinateScale;
        vertex[2] = (float)((heightBase4 * 0x100 + (int)z18[i]) * 0x100) * collisionCoordinateScale;
    }
}

// Original 0x5d7f10: expand a compressed polygon into this cache entry.
WCollisionFace *WCollisionFace::Expand(WCollisionPolygon *polygon) {
    surfaceB = (signed char)polygon->surface2;
    flagsA = polygon->flags3;
    references8 = 0;
    value10 = 0;
    unsigned char extents = polygon->extents1;
    short y0 = polygon->y10[(extents >> 2) & 3];
    short x1 = polygon->x8[(extents >> 4) & 3];
    short y1 = polygon->y10[extents >> 6];
    minimumX14 = (float)((int)polygon->x8[extents & 3] << 13) * collisionCoordinateScale;
    minimumY18 = (float)((int)y0 << 13) * collisionCoordinateScale;
    maximumX1C = (float)((int)x1 << 13) * collisionCoordinateScale;
    maximumY20 = (float)((int)y1 << 13) * collisionCoordinateScale;
    polygon->Decode(vertices30[0]);
    vertexCount9 = 4;
    if (!(flagsA & 0x10)) {
        vertexCount9 = 3;
    }
    if (flagsA & 4) {
        vertices30[0][2] = -2048.0f;
        vertices30[1][2] = 2048.0f;
        vertices30[2][2] = 2048.0f;
        vertices30[3][2] = -2048.0f;
    }
    WheelVector first, second, normal;
    first.x = vertices30[1][0] - vertices30[0][0];
    first.y = vertices30[1][1] - vertices30[0][1];
    first.z = vertices30[1][2] - vertices30[0][2];
    second.x = vertices30[2][0] - vertices30[0][0];
    second.y = vertices30[2][1] - vertices30[0][1];
    second.z = vertices30[2][2] - vertices30[0][2];
    WheelCross(&normal, &first, &second);
    WheelNormalize(&normal, &normal);
    normal70 = normal;
    distance7C = vertices30[0][0] * normal70.x + vertices30[0][1] * normal70.y + vertices30[0][2] * normal70.z;
    polygon->cacheIndex6 = (short)((reinterpret_cast<char *>(this) - reinterpret_cast<char *>(faceCache)) >> 7);
    source0C = polygon;
    return this;
}

// Original 0x5d3330: largest edge function; non-positive means the point is inside.
float WCollisionFace::EdgeDistance(float *point) {
    float result = collisionNegativeOne;
    float edge = (point[0] - vertices30[0][0]) * (vertices30[1][1] - vertices30[0][1])
               - (vertices30[1][0] - vertices30[0][0]) * (point[1] - vertices30[0][1]);
    if (collisionNegativeOne <= edge) {
        result = edge;
        if (0.0f < edge) {
            return edge;
        }
    }
    edge = (point[0] - vertices30[1][0]) * (vertices30[2][1] - vertices30[1][1])
         - (vertices30[2][0] - vertices30[1][0]) * (point[1] - vertices30[1][1]);
    if (result <= edge) {
        result = edge;
    }
    if (result <= 0.0f) {
        edge = (point[0] - vertices30[3][0]) * (vertices30[0][1] - vertices30[3][1])
             - (vertices30[0][0] - vertices30[3][0]) * (point[1] - vertices30[3][1]);
        if (result <= edge) {
            result = edge;
        }
        if (result <= 0.0f && vertexCount9 == 4) {
            edge = (point[0] - vertices30[2][0]) * (vertices30[3][1] - vertices30[2][1])
                 - (vertices30[3][0] - vertices30[2][0]) * (point[1] - vertices30[2][1]);
            if (result <= edge) {
                result = edge;
            }
        }
    }
    return result;
}

// Original 0x5dca70: return up to `count` unreferenced faces to the free list.
int WCollisionPack::ReleaseUnusedFaces(int count) {
    int released = 0;
    WCollisionLink *link = faces60.next;
    while (link != &faces60 && released < count) {
        WCollisionLink *next = link->next;
        WCollisionFace *face = reinterpret_cast<WCollisionFace *>(link);
        if (face->references8 == 0) {
            if (face->source0C) {
                face->source0C->cacheIndex6 = -1;
            }
            Unlink(link);
            AppendLink(&freeFaces, link);
            ++released;
        }
        link = next;
    }
    return released;
}

// Original 0x5d82c0: shrink oversized collider query areas back to their bounds.
void WCollisionPack::ShrinkColliderAreas() {
    for (WCollisionLink *link = colliders68.next; link != &colliders68; link = link->next) {
        WCollider *collider = reinterpret_cast<WCollider *>(link);
        if (collider->areaValue2E4 != 0
            && (collider->boundsMaximum24[0] - collider->boundsMinimum1C[0])
                   * (collider->boundsMaximum24[1] - collider->boundsMinimum1C[1]) * collisionAreaRatio
               < (collider->areaMaximum34[0] - collider->areaMinimum2C[0])
                   * (collider->areaMaximum34[1] - collider->areaMinimum2C[1])) {
            collider->ReleaseArea();
            collider->areaMinimum2C[0] = collider->boundsMinimum1C[0] - collisionAreaMargin;
            collider->areaMinimum2C[1] = collider->boundsMinimum1C[1] - collisionAreaMargin;
            collider->areaMaximum34[0] = collider->boundsMaximum24[0] + collisionAreaMargin;
            collider->areaMaximum34[1] = collider->boundsMaximum24[1] + collisionAreaMargin;
            collider->RefreshArea();
        }
    }
}

// Original 0x5dcad0: free faces across packs, rotating packs that cannot supply.
static int ReleaseFaces(WCollisionLink *packs, int count) {
    int released = 0;
    if (packs->next == packs) {
        return released;
    }
    int passes = CountLinks(packs, 0) * 2;
    for (int pass = 0; pass < passes;) {
        WCollisionLink *link = packs->next;
        Unlink(link);
        WCollisionPack *pack = reinterpret_cast<WCollisionPack *>(link);
        if (passes / 2 <= pass) {
            pack->ShrinkColliderAreas();
        }
        int freed = pack->ReleaseUnusedFaces(count - released);
        if (freed == count - released) {
            PrependLink(packs, link);
        } else {
            AppendLink(packs, link);
        }
        released += freed;
        if (released >= count) {
            break;
        }
        ++pass;
    }
    return released;
}

// Original 0x5e0c90: cached face for a polygon, expanding it on first use.
WCollisionFace *WCollisionPack::GetFace(WCollisionPolygon *polygon) {
    WCollisionFace *face;
    if (polygon->cacheIndex6 < 0) {
        if (freeFaces.next == &freeFaces) {
            ReleaseFaces(&collisionManager.packs0, 100);
        }
        WCollisionLink *link = freeFaces.next;
        Unlink(link);
        face = reinterpret_cast<WCollisionFace *>(link)->Expand(polygon);
        AppendLink(&faces60, &face->link0);
    } else {
        face = &faceCache[polygon->cacheIndex6];
    }
    face->references8 = face->references8 + 1;
    return face;
}

// Original 0x5d8140: locate the pack's sub-chunks inside a 0x80034130 chunk.
void WCollisionPack::Fixup(int *chunk) {
    int *end = reinterpret_cast<int *>(reinterpret_cast<char *>(chunk) + chunk[1] + 8);
    for (int *child = chunk + 2; child < end;
         child = reinterpret_cast<int *>(reinterpret_cast<char *>(child) + child[1] + 8)) {
        int id = child[0];
        if (id == 0x34131) {
            unsigned char *header = reinterpret_cast<unsigned char *>(
                (reinterpret_cast<unsigned int>(child) + 0x17) & ~0xfu);
            section30 = *reinterpret_cast<short *>(header);
            value32 = header[2];
            value33 = header[3];
            columns34 = *reinterpret_cast<short *>(header + 4);
            rows36 = *reinterpret_cast<short *>(header + 6);
            float *values = reinterpret_cast<float *>(header + 8);
            originY3C = values[1];
            originX38 = values[0];
            minimum40[0] = values[2];
            minimum40[1] = values[3];
            minimum40[2] = values[4];
            maximum50[0] = values[6];
            maximum50[1] = values[7];
            maximum50[2] = values[8];
            header[2] = 1;
        } else if (id == 0x34134) {
            polygons8 = reinterpret_cast<WCollisionPolygon *>(child + 2);
            polygonCountC = (unsigned int)child[1] >> 5;
        } else if (id == 0x34135) {
            values10 = reinterpret_cast<short *>(child + 2);
            valueCount14 = (unsigned int)child[1] >> 1;
        } else if (id == 0x34136) {
            refs18 = reinterpret_cast<WCollisionRef *>(child + 2);
            refCount1C = (unsigned int)child[1] >> 2;
        } else if (id == 0x34137) {
            cells20 = reinterpret_cast<WCollisionCell *>(child + 2);
            cellCount24 = (unsigned int)child[1] >> 2;
        }
    }
}

// Original 0x5d8350: pack slot pool, face cache and its free list.
void InitialiseCollisionCache(WCollisionManager *manager, char large) {
    collisionPackPool = bNewSlotPool(0x70, large ? 0x60 : 0x30, "TopologyTreeSlotPool", 0);
    WCollisionFace *cache = new WCollisionFace[0x400];
    faceCache = cache;
    for (int offset = 0; offset < 0x400; ++offset) {
        AppendLink(&freeFaces, &faceCache[offset].link0);
    }
    for (int i = 0; i < manager->barrierCount828; ++i) {
        manager->barriers82C[i].enabled8 = i == 0;
    }
}

// Original 0x5e78a0: chunk handler for collision packs and barrier groups.
int WCollisionManager::LoadChunk(int *chunk) {
    if (*chunk != (int)0x80034130) {
        if (*chunk == 0x3412f) {
            barriers82C = reinterpret_cast<WBarrierGroup *>(chunk + 2);
            barrierCount828 = (unsigned int)chunk[1] >> 5;
            return 1;
        }
        return 0;
    }
    if (collisionRelocation864F60 != 0) {
        return -1;  // Relocation of an already loaded pack (not recovered).
    }
    WCollisionPack *pack = static_cast<WCollisionPack *>(bOMalloc(collisionPackPool));
    if (pack) {
        pack->faces60.next = &pack->faces60;
        pack->faces60.previous = &pack->faces60;
        pack->colliders68.next = &pack->colliders68;
        pack->colliders68.previous = &pack->colliders68;
    }
    pack->Fixup(chunk);
    int index = CollisionSectionIndex(pack->section30);
    AppendLink(&packs0, &pack->link0);
    sections8[index] = pack;
    RegisterCollisionPack(pack);
    return 1;
}

// Original 0x5e6980: attach unpacked colliders already in this pack's section.
void RegisterCollisionPack(WCollisionPack *pack) {
    int section = pack->section30 - sectionNumberOffset;
    WCollisionLink *link = unpackedColliders.next;
    while (link != &unpackedColliders) {
        WCollisionLink *next = link->next;
        WCollider *collider = reinterpret_cast<WCollider *>(link);
        int current = collider->trackerC.section8 ? collider->trackerC.section8->id8 : 0;
        if (current == section) {
            collider->Release();
            Unlink(link);
            collider->pack8 = pack;
            AppendLink(&pack->colliders68, link);
            collider->RefreshArea();
        }
        link = next;
    }
}

// Static constructors 0x5e81d0 (collision manager) and the list part of 0x5e6360
// (topology), plus list heads 0x88f748/0x88ea8c built by 0x57bcd0/0x598750.
// The remainder of 0x5e6360 (section streaming state from +0x50) is not yet recovered.
void RunWorldStaticInitializers() {
    unpackedColliders.next = &unpackedColliders;
    unpackedColliders.previous = &unpackedColliders;
    freeFaces.next = &freeFaces;
    freeFaces.previous = &freeFaces;
    collisionManager.packs0.next = &collisionManager.packs0;
    collisionManager.packs0.previous = &collisionManager.packs0;
    for (int i = 0; i < 0x208; ++i) {
        collisionManager.sections8[i] = 0;
    }
    collisionManager.barrierCount828 = 0;
    collisionManager.barriers82C = 0;
    WCollisionLink *lists = &trackTopology.sections0;
    for (int i = 0; i < 9; ++i) {
        lists[i].next = &lists[i];
        lists[i].previous = &lists[i];
    }
    trackTopology.header4C = 0;
}

// Original 0x5e0b40: collider constructor body.
void WCollider::Construct() {
    trackerC.radiusC = -999.0f;
    trackerC.x0 = 0.0f;
    trackerC.y4 = 0.0f;
    trackerC.section8 = 0;
    pack8 = 0;
    AppendLink(&unpackedColliders, &link0);
    areaCount2E0 = 0;
    areaValue2E4 = 0;
    areaValue2E8 = 0;
    areaValue2EC = 0;
    value304 = 0;
    cachedCount2BC = 0;
    groupCount2F0 = 0;
    value308 = 0;
    value30C = 0;
    value310 = 0;
    boundsMinimum1C[0] = boundsMinimum1C[1] = 3.40282347e+38f;
    boundsMaximum24[0] = boundsMaximum24[1] = -3.40282347e+38f;
    Release();
    areaMinimum2C[0] = areaMinimum2C[1] = 3.40282347e+38f;
    areaMaximum34[0] = areaMaximum34[1] = -3.40282347e+38f;
    trackerC.x0 = 0.0f;
    trackerC.y4 = 0.0f;
    trackerC.section8 = 0;
    trackerC.radiusC = -999.0f;
}

// Original 0x5d80a0.
void WCollider::ReleaseArea() {
    int count = areaCount2E0;
    if (count == 0) {
        return;
    }
    for (int i = 0; i < count; ++i) {
        areaFaces3C[i]->references8 = areaFaces3C[i]->references8 - 1;
    }
    areaCount2E0 = 0;
    areaValue2E4 = 0;
    areaValue2E8 = 0;
    areaValue2EC = 0;
    value304 = 0;
}

// Original 0x5dc9c0: drop cached faces and return to the unpacked list.
void WCollider::Release() {
    if (!pack8) {
        return;
    }
    for (int i = 0; i < cachedCount2BC; ++i) {
        cached2C0[i]->references8 = cached2C0[i]->references8 - 1;
    }
    cachedCount2BC = 0;
    ReleaseArea();
    Unlink(&link0);
    pack8 = 0;
    AppendLink(&unpackedColliders, &link0);
}

// Original 0x5e6890.
void WCollider::SetPack(WCollisionPack *pack) {
    Release();
    if (!pack) {
        return;
    }
    Unlink(&link0);
    pack8 = pack;
    AppendLink(&pack->colliders68, &link0);
    RefreshArea();
}

// Original 0x5e5890.
void WCollider::RefreshArea() {
    ReleaseArea();
    if (!pack8 || !(areaMinimum2C[0] <= areaMaximum34[0])) {
        return;
    }
    for (int i = 0; i < groupCount2F0; ++i) {
        groups2F4[i]->enabled8 = 1;
    }
    FillColliderArea(this, areaMinimum2C, areaMaximum34);
    for (int i = 0; i < groupCount2F0; ++i) {
        groups2F4[i]->enabled8 = 0;
    }
}

// Original 0x5e68d0: follow the section topology and switch packs when needed.
void WCollider::UpdatePack(float *point, float radius) {
    float dy = point[1] - trackerC.y4;
    float dx = point[0] - trackerC.x0;
    float manhattan = (dy < 0.0f ? -dy : dy) + (dx < 0.0f ? -dx : dx);
    if (!(radius + trackerC.radiusC < manhattan)) {
        return;
    }
    if (!trackerC.Update(point, &trackTopology, radius)) {
        return;
    }
    int section = trackerC.section8 ? trackerC.section8->id8 : 0;
    if (!pack8) {
        if (section == 0) {
            return;
        }
        WCollisionPack *pack = collisionManager.GetPack((short)section);
        if (pack) {
            SetPack(pack);
        }
        return;
    }
    if (pack8->section30 - sectionNumberOffset != section) {
        SetPack(collisionManager.GetPack((short)section));
    }
}

static float PlaneHeightXY(WCollisionFace *face, float *point) {
    return collisionNegativeOne / face->normal70.z
         * ((face->normal70.x * point[0] + face->normal70.y * point[1]) - face->distance7C);
}

// Original 0x5e72e0 (via jump 0x5e7e70): ground height below a point.
float WCollider::QueryGround(WheelVector *position, int *surface, WheelVector *normal, char *found) {
    float *point = &position->x;
    UpdatePack(point, 9.0f);
    if (!pack8) {
        if (found) {
            *found = 0;
        }
        if (surface) {
            *surface = 1;
        }
        if (normal) {
            normal->x = 0.0f;
            normal->y = 0.0f;
            normal->z = -1.0f;
        }
        return point[2];
    }
    volatile float limit = point[2] + 1.0f;
    for (int i = 0; i < cachedCount2BC; ++i) {
        WCollisionFace *face = cached2C0[i];
        if (!(face->EdgeDistance(point) < 0.0f)) {
            continue;
        }
        float height = collisionNegativeOne / face->normal70.z
                     * ((face->normal70.y * point[1] + face->normal70.x * point[0]) - face->distance7C);
        if ((face->flagsA & 1) && (height > limit || !(limit - height < collisionStepLimit))) {
            continue;
        }
        for (int k = i; k > 0; --k) {
            cached2C0[k] = cached2C0[k - 1];
        }
        cached2C0[0] = face;
        if (found) {
            *found = 1;
        }
        if (surface) {
            *surface = face->surfaceB;
        }
        if (normal) {
            *normal = face->normal70;
        }
        return height;
    }
    for (int i = 0; i < groupCount2F0; ++i) {
        groups2F4[i]->enabled8 = 1;
    }
    WCollisionPack *pack = pack8;
    int cellX = (int)((point[0] - pack->originX38) * collisionCellScale);
    int cellY = (int)((point[1] - pack->originY3C) * collisionCellScale);
    float cellLeft = (float)cellX * collisionCellSize + pack->originX38;
    volatile float cellBottom = (float)cellY * collisionCellSize + pack->originY3C;
    int subX = (int)((point[0] - cellLeft) * collisionSubcellScale);
    int subY = (int)((point[1] - cellBottom) * collisionSubcellScale);
    unsigned int mask = (1u << subX) << (subY << 2);
    int column = cellX <= 0 ? 0 : cellX;
    if (pack->columns34 - 1 < column) {
        column = pack->columns34 - 1;
    }
    int row = cellY <= 0 ? 0 : cellY;
    if (pack->rows36 - 1 < row) {
        row = pack->rows36 - 1;
    }
    WCollisionCell *cell = &pack->cells20[pack->columns34 * row + column];
    WCollisionFace *visited[128];
    int visitedCount = 0;
    WCollisionFace *best = 0;
    volatile float bestHeight = -99999.0f;
    for (int pass = 0; pass < 2; ++pass) {
        int fixedX = (int)(point[0] * collisionFixedScale);
        int fixedY = (int)(point[1] * collisionFixedScale);
        int end = cell->start + cell->count;
        for (int r = cell->start; r < end; ++r) {
            WCollisionRef *ref = &pack->refs18[r];
            if (!(mask & ref->mask)) {
                continue;
            }
            WCollisionPolygon *polygon = &pack->polygons8[ref->polygon];
            if (polygon->flags3 & 0xa) {
                break;
            }
            unsigned char extents = polygon->extents1;
            if (fixedX < (int)polygon->x8[extents & 3] << 13
                || fixedY < (int)polygon->y10[(extents >> 2) & 3] << 13
                || fixedX > (int)polygon->x8[(extents >> 4) & 3] << 13
                || fixedY > (int)polygon->y10[extents >> 6] << 13) {
                continue;
            }
            if (polygon->group0 && !collisionManager.barriers82C[polygon->group0].enabled8) {
                continue;
            }
            WCollisionFace *face = pack->GetFace(polygon);
            visited[visitedCount++] = face;
            float edge = face->EdgeDistance(point);
            if (edge > 0.0f) {
                continue;
            }
            volatile float height = PlaneHeightXY(face, point);
            if (!(face->flagsA & 1) && edge < 0.0f) {
                bestHeight = height;
                best = face;
                break;
            }
            if (!(height > bestHeight) || height > limit) {
                continue;
            }
            bestHeight = height;
            best = face;
            if (face->flagsA & 0x40) {
                continue;
            }
            if (limit - height < collisionStepLimit) {
                break;
            }
        }
        if (best) {
            break;
        }
        limit = 3.40282347e+38f;
    }
    for (int i = 0; i < groupCount2F0; ++i) {
        groups2F4[i]->enabled8 = 0;
    }
    for (int i = 0; i < visitedCount; ++i) {
        visited[i]->references8 = visited[i]->references8 - 1;
    }
    if (!best) {
        if (found) {
            *found = 0;
        }
        if (surface) {
            *surface = 1;
        }
        if (normal) {
            normal->x = 0.0f;
            normal->y = 0.0f;
            normal->z = -1.0f;
        }
        return point[2];
    }
    if (!(best->flagsA & 0x40)) {
        best->references8 = best->references8 + 1;
        int count = cachedCount2BC + 1;
        cachedCount2BC = count;
        if (count > 4) {
            cachedCount2BC = 4;
            cached2C0[3]->references8 = cached2C0[3]->references8 - 1;
        }
        for (int k = cachedCount2BC - 1; k > 0; --k) {
            cached2C0[k] = cached2C0[k - 1];
        }
        cached2C0[0] = best;
    }
    if (found) {
        *found = 1;
    }
    if (surface) {
        *surface = best->surfaceB;
    }
    if (normal) {
        *normal = best->normal70;
    }
    return bestHeight;
}
