#include "Collision.h"

extern "C" double __cdecl sqrt(double);
#pragma intrinsic(sqrt)
extern "C" void *malloc(unsigned int size);
extern "C" void free(void *memory);

// Convex collision shapes: vertex access and GJK support points.

extern float collisionShapeHalf;
extern float collisionSupportFloor;
extern int collisionPolygonCentres86B2DC;
extern int collisionHullClimb86B2D8;
extern float collisionThirdWeight;
extern float collisionThirdRemainder;
extern float collisionQuarterWeight;

extern unsigned int bRandomSeed7EA624;

// Original 0x43c1c0: xorshift step; returns the previous state modulo `range`.
unsigned int bRandom(unsigned int range) {
    unsigned int result = bRandomSeed7EA624 % range;
    unsigned int mixed = bRandomSeed7EA624 ^ 0x1d872b41;
    unsigned int shifted = mixed >> 5 ^ mixed;
    bRandomSeed7EA624 = shifted << 0x1b ^ shifted ^ mixed;
    return result;
}

// Original 0x43d190.
int VectorsNear(float *a, float *b, float tolerance) {
    float x = a[0] - b[0];
    float y = a[1] - b[1];
    float z = a[2] - b[2];
    if ((x < 0.0f ? -x : x) <= tolerance && (y < 0.0f ? -y : y) <= tolerance
        && (z < 0.0f ? -z : z) <= tolerance) {
        return 1;
    }
    return 0;
}

// Original 0x59fb50.
static void ClearVertex(CollisionVertex *vertex) {
    vertex->neighbourCount8 = 0;
    vertex->neighbourStartA = 0;
    vertex->indexC = 0;
    vertex->stampE = 0;
    vertex->point10[0] = 0.0f;
    vertex->point10[1] = 0.0f;
    vertex->point10[2] = 0.0f;
}

// Original 0x5a1750: link the hulls of a 0x80034020 chunk (0x39200 header, 0x39201
// vertices, 0x39202 neighbour indices) into the hull list, in place.
int LoadCollisionHulls(int *chunk) {
    if (chunk[0] != (int)0x80034020) {
        return 0;
    }
    char *end = reinterpret_cast<char *>(chunk) + chunk[1] + 8;
    char *child = reinterpret_cast<char *>(chunk + 2);
    while (child < end) {
        int id = *reinterpret_cast<int *>(child);
        if (id != 0x34021 && id == 0x39200) {
            char *vertices = child + *reinterpret_cast<int *>(child + 4) + 8;
            char *neighbours = vertices + *reinterpret_cast<int *>(vertices + 4) + 8;
            char *alignedVertices = reinterpret_cast<char *>((reinterpret_cast<unsigned int>(vertices) + 0x17) & ~0xfu);
            unsigned int vertexBytes = *reinterpret_cast<int *>(vertices + 4) + (vertices - alignedVertices) + 8;
            char *alignedNeighbours = reinterpret_cast<char *>((reinterpret_cast<unsigned int>(neighbours) + 0x17) & ~0xfu);
            unsigned int neighbourBytes = *reinterpret_cast<int *>(neighbours + 4) + (neighbours - alignedNeighbours) + 8;
            CollisionHull *hull = reinterpret_cast<CollisionHull *>((reinterpret_cast<unsigned int>(child) + 0x17) & ~0xfu);
            WCollisionLink *tail = collisionHulls86B598.previous;
            tail->next = reinterpret_cast<WCollisionLink *>(hull);
            collisionHulls86B598.previous = reinterpret_cast<WCollisionLink *>(hull);
            hull->neighbourCount3C = (short)(neighbourBytes >> 1);
            hull->vertices54 = reinterpret_cast<CollisionVertex *>(alignedVertices);
            hull->count14 = (short)(vertexBytes / 0x30);
            hull->previous4 = reinterpret_cast<CollisionShape *>(tail);
            hull->next0 = reinterpret_cast<CollisionShape *>(&collisionHulls86B598);
            hull->type10 = 1;
            hull->neighbours50 = reinterpret_cast<short *>(alignedNeighbours);
            hull->offset40.x = 0.0f;
            hull->offset40.y = 0.0f;
            hull->offset40.z = 0.0f;
            hull->offsetValid3E = 0;
            child = neighbours + *reinterpret_cast<int *>(neighbours + 4) + 8;
        }
    }
    return 1;
}

// Original 0x5a18c0: find a hull by key and optionally set its centring offset.
CollisionHull *FindCollisionHull(unsigned int key, WheelVector *offset) {
    WCollisionLink *link = collisionHulls86B598.next;
    while (link != &collisionHulls86B598) {
        CollisionHull *hull = reinterpret_cast<CollisionHull *>(link);
        if (hull->key18 == key) {
            if (offset) {
                hull->offset40 = *offset;
                hull->offsetValid3E = 1;
            }
            return hull;
        }
        link = link->next;
    }
    return 0;
}

// Original 0x5a1920 (owned hulls only; chunk hulls are never released).
void CollisionHull::Release() {
    if (vertices54) {
        free(reinterpret_cast<char *>(vertices54) - 0x10);
    }
    free(neighbours50);
    vertices54 = 0;
    neighbours50 = 0;
    count14 = 0;
    neighbourCount3C = 0;
}

// Original 0x5a1970.
void CollisionHull::GetVertex(int index, CollisionVertex *output) {
    CollisionVertex *vertex = &vertices54[index];
    output->neighbourCount8 = vertex->neighbourCount8;
    output->neighbourStartA = vertex->neighbourStartA;
    output->indexC = vertex->indexC;
    output->stampE = vertex->stampE;
    output->point10[0] = vertex->point10[0];
    output->point10[1] = vertex->point10[1];
    output->point10[2] = vertex->point10[2];
    if (offsetValid3E) {
        float z = offset40.z;
        float y = offset40.y;
        output->point10[0] = output->point10[0] - offset40.x;
        output->point10[1] = output->point10[1] - y;
        output->point10[2] = output->point10[2] - z;
    }
}

// Original 0x5a19f0: bounds of the (offset) vertices.
void CollisionHull::GetBounds(WheelVector *minimum, WheelVector *maximum) {
    CollisionVertex vertex;
    ClearVertex(&vertex);
    GetVertex(0, &vertex);
    *minimum = *reinterpret_cast<WheelVector *>(vertex.point10);
    *maximum = *reinterpret_cast<WheelVector *>(vertex.point10);
    for (int i = 1; i < count14; ++i) {
        GetVertex(i, &vertex);
        WheelVector *point = reinterpret_cast<WheelVector *>(vertex.point10);
        if (point->x < minimum->x) minimum->x = point->x;
        if (point->y < minimum->y) minimum->y = point->y;
        if (point->z < minimum->z) minimum->z = point->z;
        if (point->x > maximum->x) maximum->x = point->x;
        if (point->y > maximum->y) maximum->y = point->y;
        if (point->z > maximum->z) maximum->z = point->z;
    }
}

static float HullDot(CollisionHull *hull, CollisionVertex *vertex, float *direction) {
    float x = vertex->point10[0];
    float y = vertex->point10[1];
    float z = vertex->point10[2];
    if (hull->offsetValid3E) {
        x = x - hull->offset40.x;
        y = y - hull->offset40.y;
        z = z - hull->offset40.z;
    }
    return (z * direction[2] + y * direction[1]) + x * direction[0];
}

// Original 0x5afba0: support point by hill climbing over the vertex neighbours, falling
// back to a full scan. Visited vertices carry a per-call stamp.
CollisionVertex *CollisionHull::Support(WheelVector *direction, CollisionVertex *start, WheelMatrix *matrix,
                                        WheelMatrix *inverse, CollisionVertex *output) {
    CollisionVertex *best = 0;
    WCollisionLink open;
    open.next = &open;
    open.previous = &open;
    short stamp = (short)bRandom(0xffff);
    stamp = (short)(stamp + bGetTicker());
    WheelVector local;
    WheelTransform(&local, inverse, direction);
    int climb = 0x10 < count14;
    CollisionVertex *first = &vertices54[start->indexC];
    WCollisionLink *tail = open.previous;
    tail->next = reinterpret_cast<WCollisionLink *>(first);
    open.previous = reinterpret_cast<WCollisionLink *>(first);
    first->previous4 = reinterpret_cast<CollisionVertex *>(tail);
    first->next0 = reinterpret_cast<CollisionVertex *>(&open);
    for (;;) {
        if (open.next == &open) {
            break;
        }
        CollisionVertex *current = reinterpret_cast<CollisionVertex *>(open.next);
        reinterpret_cast<WCollisionLink *>(current->previous4)->next = reinterpret_cast<WCollisionLink *>(current->next0);
        reinterpret_cast<WCollisionLink *>(current->next0)->previous = reinterpret_cast<WCollisionLink *>(current->previous4);
        CollisionVertex *candidate = current;
        if (current->stampE != stamp) {
            float x = current->point10[0];
            float y = current->point10[1];
            float z = current->point10[2];
            if (offsetValid3E) {
                x = x - offset40.x;
                y = y - offset40.y;
                z = z - offset40.z;
            }
            current->stampE = stamp;
            current->dot20 = (local.z * z + local.y * y) + x * local.x;
        }
        float currentDot = current->dot20;
        if (current->neighbourCount8 == 0) {
            best = 0;
            continue;
        }
        int end = current->neighbourStartA + current->neighbourCount8;
        for (int k = current->neighbourStartA; k < end; ++k) {
            CollisionVertex *neighbour = &vertices54[neighbours50[k]];
            short previousStamp = neighbour->stampE;
            if (previousStamp != stamp) {
                neighbour->dot20 = HullDot(this, neighbour, &local.x);
            }
            if (!(neighbour->dot20 <= currentDot)) {
                candidate = 0;
                if (climb && previousStamp != stamp) {
                    WCollisionLink *last = open.previous;
                    last->next = reinterpret_cast<WCollisionLink *>(neighbour);
                    open.previous = reinterpret_cast<WCollisionLink *>(neighbour);
                    neighbour->previous4 = reinterpret_cast<CollisionVertex *>(last);
                    neighbour->next0 = reinterpret_cast<CollisionVertex *>(&open);
                }
            }
            neighbour->stampE = stamp;
        }
        best = candidate;
        climb = collisionHullClimb86B2D8;
        if (best) {
            break;
        }
    }
    if (!best) {
        float bestDot = collisionSupportFloor;
        for (int i = 0; i < count14; ++i) {
            CollisionVertex *vertex = &vertices54[i];
            if (vertex->stampE != stamp) {
                vertex->dot20 = HullDot(this, vertex, &local.x);
                vertex->stampE = stamp;
            }
            if (vertex->dot20 > bestDot || bestDot == collisionSupportFloor) {
                bestDot = vertex->dot20;
                best = vertex;
            }
        }
        if (!best) {
            return output;
        }
    }
    if (offsetValid3E) {
        float z = best->point10[2];
        float y = best->point10[1];
        output->point10[0] = best->point10[0] - offset40.x;
        output->point10[1] = y - offset40.y;
        output->point10[2] = z - offset40.z;
    }
    WheelTransform(reinterpret_cast<WheelVector *>(output->point10), matrix,
                   reinterpret_cast<WheelVector *>(best->point10));
    output->indexC = best->indexC;
    return output;
}

// Original 0x598760: box from minimum/maximum corners.
void CollisionBox::Set(float minimumX, float minimumY, float minimumZ, float maximumX, float maximumY,
                       float maximumZ) {
    count14 = 8;
    centre20.x = (minimumX + maximumX) * collisionShapeHalf;
    centre20.y = (minimumY + maximumY) * collisionShapeHalf;
    centre20.z = (minimumZ + maximumZ) * collisionShapeHalf;
    extent30.x = (maximumX - minimumX) * collisionShapeHalf;
    extent30.y = (maximumY - minimumY) * collisionShapeHalf;
    extent30.z = (maximumZ - minimumZ) * collisionShapeHalf;
}

// Original 0x5987d0: corner `index` (bit 0/1/2 negate x/y/z).
void CollisionBox::GetVertex(unsigned short index, CollisionVertex *output) {
    output->point10[0] = extent30.x;
    output->point10[1] = extent30.y;
    output->point10[2] = extent30.z;
    output->indexC = index;
    if (index & 1) {
        output->point10[0] = -extent30.x;
    }
    if (index & 2) {
        output->point10[1] = -extent30.y;
    }
    if (index & 4) {
        output->point10[2] = -extent30.z;
    }
    float y = centre20.y;
    float z = centre20.z;
    output->point10[0] = centre20.x + output->point10[0];
    output->point10[1] = y + output->point10[1];
    output->point10[2] = z + output->point10[2];
}

// Original 0x5a2280.
CollisionVertex *CollisionBox::Support(WheelVector *direction, CollisionVertex *, WheelMatrix *matrix,
                                       WheelMatrix *inverse, CollisionVertex *output) {
    WheelVector local;
    WheelTransform(&local, inverse, direction);
    WheelVector corner;
    corner.x = extent30.x;
    corner.y = extent30.y;
    corner.z = extent30.z;
    if (local.x < 0.0f) {
        corner.x = -corner.x;
    }
    unsigned short index = local.x < 0.0f;
    if (local.y < 0.0f) {
        index |= 2;
        corner.y = -corner.y;
    }
    if (local.z < 0.0f) {
        index |= 4;
        corner.z = -corner.z;
    }
    corner.x = corner.x + centre20.x;
    corner.y = centre20.y + corner.y;
    corner.z = centre20.z + corner.z;
    WheelTransform(&corner, matrix, &corner);
    output->point10[0] = corner.x;
    output->point10[1] = corner.y;
    output->point10[2] = corner.z;
    output->point10[3] = 0.0f;
    output->indexC = index;
    return output;
}

// Original 0x5a2360.
CollisionVertex *CollisionPoint::Support(WheelVector *, CollisionVertex *, WheelMatrix *matrix,
                                         WheelMatrix *, CollisionVertex *output) {
    WheelVector point = point30;
    WheelTransform(&point, matrix, &point);
    output->point10[0] = point.x;
    output->point10[1] = point.y;
    output->point10[2] = point.z;
    output->point10[3] = 0.0f;
    output->indexC = index2C;
    return output;
}

// Original 0x5a2010.
void CollisionPolygon::GetVertex(int index, CollisionVertex *output) {
    CollisionVertex *vertex = &vertices20[index];
    output->neighbourCount8 = vertex->neighbourCount8;
    output->neighbourStartA = vertex->neighbourStartA;
    output->indexC = vertex->indexC;
    output->stampE = vertex->stampE;
    output->point10[0] = vertex->point10[0];
    output->point10[1] = vertex->point10[1];
    output->point10[2] = vertex->point10[2];
}

// Original 0x5a2060: polygons are in world space; a thick polygon whose extrusion is not
// active only offers its front vertices.
CollisionVertex *CollisionPolygon::Support(WheelVector *direction, CollisionVertex *, WheelMatrix *,
                                           WheelMatrix *, CollisionVertex *output) {
    int count = count14;
    float bestDot = collisionSupportFloor;
    CollisionVertex *best = 0;
    if (thickness1D4 > 0.0f && !thick1D0) {
        count = count / 2;
    }
    for (int i = 0; i < count; ++i) {
        CollisionVertex *vertex = &vertices20[i];
        float dot = vertex->point10[0] * direction->x + vertex->point10[2] * direction->z
                  + vertex->point10[1] * direction->y;
        if (bestDot < dot || bestDot == collisionSupportFloor) {
            best = vertex;
            bestDot = dot;
        }
    }
    if (best) {
        output->point10[0] = best->point10[0];
        output->point10[3] = 0.0f;
        output->point10[1] = best->point10[1];
        output->point10[2] = best->point10[2];
        output->indexC = best->indexC;
    }
    return output;
}

// Original 0x5a1a90: copy the face outline (relative to `origin`) and, when thick, add the
// outline pushed `thickness` behind the face.
void CollisionPolygon::Build(WCollisionFace *face, WheelVector *origin, float thickness) {
    centre1E0.x = 0.0f;
    centre1E0.y = 0.0f;
    centre1E0.z = 0.0f;
    centre1F0.x = 0.0f;
    centre1F0.y = 0.0f;
    centre1F0.z = 0.0f;
    thickness1D4 = thickness;
    int count = 4 - (VectorsNear(face->vertices30[2], face->vertices30[3], 0.001f) != 0);
    count14 = count;
    for (int i = 0; i < count14; ++i) {
        CollisionVertex *vertex = &vertices20[i];
        float x = face->vertices30[i][0] - origin->x;
        float y = face->vertices30[i][1] - origin->y;
        float z = face->vertices30[i][2] - origin->z;
        vertex->point10[3] = 0.0f;
        vertex->point10[0] = x;
        vertex->point10[1] = y;
        vertex->point10[2] = z;
        vertex->indexC = (short)i;
        centre1E0.x = x + centre1E0.x;
        centre1E0.y = y + centre1E0.y;
        centre1E0.z = z + centre1E0.z;
    }
    centre1F0 = centre1E0;
    float scale = 1.0f / (float)count14;
    centre1E0.x = scale * centre1E0.x;
    centre1E0.y = scale * centre1E0.y;
    centre1E0.z = scale * centre1E0.z;
    if (0.0f < thickness1D4) {
        collisionPolygonCentres86B2DC = 0;
        float depth = -thickness1D4;
        float nx = face->normal70.x;
        float ny = face->normal70.y;
        float nz = face->normal70.z;
        for (int i = 0; i < count14; ++i) {
            CollisionVertex *front = &vertices20[i];
            float x = front->point10[0] + depth * nx;
            float y = front->point10[1] + ny * depth;
            float z = nz * depth + front->point10[2];
            CollisionVertex *back = &vertices20[count14 + i];
            back->point10[3] = 0.0f;
            back->point10[0] = x;
            back->point10[1] = y;
            back->point10[2] = z;
            back->indexC = (short)(count14 + i);
            centre1F0.x = x + centre1F0.x;
            centre1F0.y = y + centre1F0.y;
            centre1F0.z = z + centre1F0.z;
        }
        count14 = count14 << 1;
    }
    scale = 1.0f / (float)count14;
    centre1F0.x = scale * centre1F0.x;
    centre1F0.y = scale * centre1F0.y;
    centre1F0.z = scale * centre1F0.z;
    if (collisionPolygonCentres86B2DC) {
        float *v = face->vertices30[0];
        WheelVector middle;
        float x, y, z;
        if (count14 == 3) {
            middle.x = face->vertices30[1][0] * collisionThirdWeight + v[0] * collisionThirdWeight;
            middle.y = face->vertices30[1][1] * collisionThirdWeight + v[1] * collisionThirdWeight;
            middle.z = face->vertices30[1][2] * collisionThirdWeight + v[2] * collisionThirdWeight;
            x = face->vertices30[2][0] * collisionThirdRemainder;
            y = face->vertices30[2][1] * collisionThirdRemainder;
            z = face->vertices30[2][2] * collisionThirdRemainder;
        } else {
            middle.x = face->vertices30[2][0] * collisionQuarterWeight + face->vertices30[1][0] * collisionQuarterWeight
                     + v[0] * collisionQuarterWeight;
            middle.y = face->vertices30[2][1] * collisionQuarterWeight + face->vertices30[1][1] * collisionQuarterWeight
                     + v[1] * collisionQuarterWeight;
            middle.z = face->vertices30[2][2] * collisionQuarterWeight + v[2] * collisionQuarterWeight
                     + face->vertices30[1][2] * collisionQuarterWeight;
            x = face->vertices30[3][0] * collisionQuarterWeight;
            y = face->vertices30[3][1] * collisionQuarterWeight;
            z = face->vertices30[3][2] * collisionQuarterWeight;
        }
        CollisionVertex *centre = &vertices20[count14];
        centre->point10[3] = 0.0f;
        centre->point10[2] = z + middle.z;
        centre->point10[0] = x + middle.x;
        centre->point10[1] = y + middle.y;
        centre->indexC = (short)count14;
        for (int i = 0; i < count14; ++i) {
            CollisionVertex *base = &vertices20[count14];
            float cx = base->point10[0] * collisionShapeHalf;
            float cy = base->point10[1] * collisionShapeHalf;
            float cz = base->point10[2] * collisionShapeHalf;
            float px = vertices20[i].point10[0] * collisionShapeHalf;
            float py = vertices20[i].point10[1] * collisionShapeHalf;
            float pz = vertices20[i].point10[2] * collisionShapeHalf;
            CollisionVertex *half = &vertices20[count14 + 1 + i];
            half->point10[3] = 0.0f;
            half->point10[2] = pz + cz;
            half->point10[0] = px + cx;
            half->point10[1] = py + cy;
            half->indexC = (short)(count14 + i + 1);
        }
        count14 = (count14 != 3) * 2 + 7;
    }
}

// Original 0x5aad20: the index-th vertex of any shape.
CollisionVertex *CollisionShapeVertex(CollisionShape *shape, int index, CollisionVertex *output) {
    switch (shape->type10) {
    case 1:
        static_cast<CollisionHull *>(shape)->GetVertex(index, output);
        return output;
    case 2:
        static_cast<CollisionPolygon *>(shape)->GetVertex(index, output);
        return output;
    case 3:
        static_cast<CollisionBox *>(shape)->GetVertex((unsigned short)index, output);
        return output;
    case 4: {
        CollisionPoint *point = static_cast<CollisionPoint *>(shape);
        output->point10[0] = point->point30.x;
        output->point10[1] = point->point30.y;
        output->point10[3] = 0.0f;
        output->point10[2] = point->point30.z;
        output->indexC = point->index2C;
        return output;
    }
    case 5:
        static_cast<CollisionCylinder *>(shape)->GetVertex(index, output);
        return output;
    default:
        return output;
    }
}
