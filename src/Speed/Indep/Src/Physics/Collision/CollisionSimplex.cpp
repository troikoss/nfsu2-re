#include "Collision.h"

extern "C" double __cdecl sqrt(double);
#pragma intrinsic(sqrt)
extern "C" void *malloc(unsigned int size);
extern "C" void free(void *memory);

// GJK simplex state and the sub-algorithm that finds the closest features of the two
// point sets (up to four points each).

extern float collisionParallelLimit;

// Original 0x598130: copy without the w components.
void CollisionSimplex::CopyFrom(CollisionSimplex *source) {
    for (int i = 0; i < 4; ++i) {
        a0[i][0] = source->a0[i][0];
        a0[i][1] = source->a0[i][1];
        a0[i][2] = source->a0[i][2];
    }
    for (int i = 0; i < 4; ++i) {
        b40[i][0] = source->b40[i][0];
        b40[i][1] = source->b40[i][1];
        b40[i][2] = source->b40[i][2];
    }
    closestA80 = source->closestA80;
    closestB90 = source->closestB90;
    normalA0 = source->normalA0;
    for (int i = 0; i < 5; ++i) {
        indicesB0[i] = source->indicesB0[i];
    }
    for (int i = 0; i < 5; ++i) {
        indicesC4[i] = source->indicesC4[i];
    }
    startedD8 = source->startedD8;
}

// Original 0x598240: the same copy (second copy routine in the original).
void CollisionSimplex::Assign(CollisionSimplex *source) {
    for (int i = 0; i < 4; ++i) {
        a0[i][0] = source->a0[i][0];
        a0[i][1] = source->a0[i][1];
        a0[i][2] = source->a0[i][2];
    }
    for (int i = 0; i < 4; ++i) {
        b40[i][0] = source->b40[i][0];
        b40[i][1] = source->b40[i][1];
        b40[i][2] = source->b40[i][2];
    }
    closestA80 = source->closestA80;
    closestB90 = source->closestB90;
    normalA0 = source->normalA0;
    for (int i = 0; i < 5; ++i) {
        indicesB0[i] = source->indicesB0[i];
    }
    for (int i = 0; i < 5; ++i) {
        indicesC4[i] = source->indicesC4[i];
    }
    startedD8 = source->startedD8;
}

static void Load(WheelVector *output, float *input) {
    output->x = input[0];
    output->y = input[1];
    output->z = input[2];
}

static void Store(float *output, float *input) {
    output[0] = input[0];
    output[1] = input[1];
    output[2] = input[2];
}

static float SquaredGap(WheelVector *a, WheelVector *b) {
    return (b->x - a->x) * (b->x - a->x) + (b->y - a->y) * (b->y - a->y) + (b->z - a->z) * (b->z - a->z);
}

// Original 0x595040: closest features of the A and B point sets. Each vertex/edge/triangle
// pair is tried; a pair whose separating direction leaves every other point behind it ends
// the search at once (the simplex shrinks to that pair); otherwise the closest valid pair
// is used. Writes the closest points and the separating direction (A towards B) and returns
// the squared distance.
float CollisionSimplex::Distance(WheelVector *direction) {
    int countA = countC0;
    int countB = countD4;
    int found = 0;
    float best = 3.40282347e+38f;
    WheelVector bestA;
    WheelVector bestB;
    WheelVector bestDirection;
    int bestCountA = 0;
    int bestCountB = 0;
    int bestIndicesA[4];
    int bestIndicesB[4];
    float (*a)[4] = a0;
    float (*b)[4] = b40;
    if (countA == 1) {
        if (countB == 1) {
            Load(&closestA80, a[0]);
            Load(&closestB90, b[0]);
            float x = closestB90.x - closestA80.x;
            float y = closestB90.y - closestA80.y;
            float z = closestB90.z - closestA80.z;
            direction->x = x;
            direction->y = y;
            direction->z = z;
            return z * z + y * y + x * x;
        }
        // Point A0 against the edges of B.
        int last = countB - 1;
        for (int i = 0; i < last; ++i) {
            WheelVector offset;
            offset.x = b[i][0] - a[0][0];
            offset.y = b[i][1] - a[0][1];
            offset.z = b[i][2] - a[0][2];
            for (int j = i + 1; j < countB; ++j) {
                WheelVector edge;
                edge.x = b[j][0] - b[i][0];
                edge.y = b[j][1] - b[i][1];
                edge.z = b[j][2] - b[i][2];
                float t = -(edge.y * offset.y + edge.z * offset.z + edge.x * offset.x)
                        / (edge.y * edge.y + edge.z * edge.z + edge.x * edge.x);
                if (t < 0.0f) {
                    t = 0.0f;
                    direction->x = b[i][0] - a[0][0];
                    direction->z = b[i][2] - a[0][2];
                    direction->y = b[i][1] - a[0][1];
                } else if (t > 1.0f) {
                    t = 1.0f;
                    direction->x = b[j][0] - a[0][0];
                    direction->z = b[j][2] - a[0][2];
                    direction->y = b[j][1] - a[0][1];
                } else {
                    WheelCross(direction, &offset, &edge);
                    WheelCross(direction, &edge, direction);
                }
                int lowest = 0;
                float low = direction->z * b[0][2] + direction->y * b[0][1] + b[0][0] * direction->x;
                for (int k = 1; k < countB; ++k) {
                    float value = direction->y * b[k][1] + b[k][2] * direction->z + b[k][0] * direction->x;
                    if (value < low) {
                        low = value;
                        lowest = k;
                    }
                }
                if (direction->y * a[0][1] + direction->z * a[0][2] + direction->x * a[0][0] < low) {
                    Load(&closestA80, a[0]);
                    closestB90.x = edge.x * t + b[i][0];
                    closestB90.z = b[i][2] + edge.z * t;
                    closestB90.y = b[i][1] + edge.y * t;
                    float distance = SquaredGap(&closestA80, &closestB90);
                    if (lowest == i || lowest == j) {
                        if (t != 0.0f) {
                            if (t == 1.0f) {
                                countD4 = 1;
                                indicesC4[0] = indicesC4[j];
                                Store(b[0], b[j]);
                                return distance;
                            }
                            countD4 = 2;
                            indicesC4[0] = indicesC4[i];
                            indicesC4[1] = indicesC4[j];
                            Store(b[0], b[i]);
                            Store(b[1], b[j]);
                            return distance;
                        }
                        countD4 = 1;
                        indicesC4[0] = indicesC4[i];
                        Store(b[0], b[i]);
                        return distance;
                    }
                    if (distance < best) {
                        bestA = closestA80;
                        bestB = closestB90;
                        bestDirection = *direction;
                        found = 1;
                        bestCountA = 1;
                        bestIndicesA[0] = 0;
                        best = distance;
                        if (t == 0.0f) {
                            bestCountB = 1;
                            bestIndicesB[0] = i;
                        } else if (t == 1.0f) {
                            bestCountB = 1;
                            bestIndicesB[0] = j;
                        } else {
                            bestCountB = 2;
                            bestIndicesB[0] = i;
                            bestIndicesB[1] = j;
                        }
                    }
                }
            }
        }
    } else if (countB == 1) {
        // Point B0 against the edges of A.
        int last = countA - 1;
        for (int i = 0; i < last; ++i) {
            WheelVector offset;
            offset.x = b[0][0] - a[i][0];
            offset.y = b[0][1] - a[i][1];
            offset.z = b[0][2] - a[i][2];
            for (int j = i + 1; j < countA; ++j) {
                WheelVector edge;
                edge.x = a[j][0] - a[i][0];
                edge.y = a[j][1] - a[i][1];
                edge.z = a[j][2] - a[i][2];
                float t = (edge.y * offset.y + edge.z * offset.z + edge.x * offset.x)
                        / (edge.y * edge.y + edge.z * edge.z + edge.x * edge.x);
                if (t < 0.0f) {
                    t = 0.0f;
                    direction->x = b[0][0] - a[i][0];
                    direction->z = b[0][2] - a[i][2];
                    direction->y = b[0][1] - a[i][1];
                } else if (t > 1.0f) {
                    t = 1.0f;
                    direction->x = b[0][0] - a[j][0];
                    direction->z = b[0][2] - a[j][2];
                    direction->y = b[0][1] - a[j][1];
                } else {
                    WheelCross(direction, &offset, &edge);
                    WheelCross(direction, &edge, direction);
                }
                int highest = 0;
                float high = direction->y * a[0][1] + direction->z * a[0][2] + direction->x * a[0][0];
                for (int k = 1; k < countA; ++k) {
                    float value = a[k][1] * direction->y + a[k][2] * direction->z + a[k][0] * direction->x;
                    if (high < value) {
                        high = value;
                        highest = k;
                    }
                }
                if (high < b[0][0] * direction->x + b[0][1] * direction->y + b[0][2] * direction->z) {
                    closestA80.x = edge.x * t + a[i][0];
                    closestA80.z = a[i][2] + edge.z * t;
                    closestA80.y = a[i][1] + edge.y * t;
                    Load(&closestB90, b[0]);
                    float distance = SquaredGap(&closestA80, &closestB90);
                    if (highest == i || highest == j) {
                        if (t == 0.0f) {
                            countC0 = 1;
                            indicesB0[0] = indicesB0[i];
                            Store(a[0], a[i]);
                            return distance;
                        }
                        if (t == 1.0f) {
                            countC0 = 1;
                            indicesB0[0] = indicesB0[j];
                            Store(a[0], a[j]);
                            return distance;
                        }
                        countC0 = 2;
                        indicesB0[0] = indicesB0[i];
                        Store(a[0], a[i]);
                        indicesB0[1] = indicesB0[j];
                        Store(a[1], a[j]);
                        return distance;
                    }
                    if (distance < best) {
                        bestA = closestA80;
                        bestB = closestB90;
                        bestDirection = *direction;
                        found = 1;
                        if (t == 0.0f) {
                            bestCountA = 1;
                            bestIndicesA[0] = i;
                        } else if (t != 1.0f) {
                            bestCountA = 2;
                            bestIndicesA[1] = j;
                            bestIndicesA[0] = i;
                        } else {
                            bestCountA = 1;
                            bestIndicesA[0] = j;
                        }
                        bestCountB = 1;
                        bestIndicesB[0] = 0;
                        best = distance;
                    }
                }
            }
        }
    } else {
        // Edges of A against edges of B.
        int lastA = countA - 1;
        for (int i = 0; i < lastA; ++i) {
            for (int j = i + 1; j < countA; ++j) {
                float *start = a[i];
                float *end = a[j];
                WheelVector edgeA;
                edgeA.x = end[0] - start[0];
                edgeA.y = end[1] - start[1];
                edgeA.z = end[2] - start[2];
                float lengthA = edgeA.y * edgeA.y + edgeA.z * edgeA.z + edgeA.x * edgeA.x;
                int lastB = countB - 1;
                for (int k = 0; k < lastB; ++k) {
                    float *other = b[k];
                    WheelVector offset;
                    offset.x = other[0] - start[0];
                    offset.y = other[1] - start[1];
                    offset.z = other[2] - start[2];
                    float alongA = edgeA.y * offset.y + edgeA.z * offset.z + edgeA.x * offset.x;
                    for (int l = k + 1; l < countB; ++l) {
                        float *otherEnd = b[l];
                        WheelVector edgeB;
                        edgeB.x = otherEnd[0] - other[0];
                        edgeB.y = otherEnd[1] - other[1];
                        edgeB.z = otherEnd[2] - other[2];
                        float lengthB = edgeB.y * edgeB.y + edgeB.z * edgeB.z + edgeB.x * edgeB.x;
                        float cross = edgeA.y * edgeB.y + edgeA.z * edgeB.z + edgeA.x * edgeB.x;
                        float alongB = edgeB.y * offset.y + edgeB.z * offset.z + edgeB.x * offset.x;
                        float denominator = lengthA * lengthB - cross * cross;
                        float s = alongA * lengthB - cross * alongB;
                        if ((denominator < 0.0f ? -denominator : denominator) > collisionParallelLimit) {
                            s = s / denominator;
                            if (s < 0.0f) {
                                s = 0.0f;
                            } else if (s > 1.0f) {
                                s = 1.0f;
                            }
                        } else {
                            s = s * denominator > 0.0f ? 1.0f : 0.0f;
                        }
                        float u = (cross * s - alongB) / lengthB;
                        float t;
                        if (!(u > 0.0f && u < 1.0f)) {
                            t = u <= 0.0f ? 0.0f : 1.0f;
                            float s2 = (cross * t + alongA) / lengthA;
                            if (!(s2 > 0.0f && s2 < 1.0f)) {
                                s = s2 <= 0.0f ? 0.0f : 1.0f;
                                WheelVector pointA;
                                WheelVector pointB;
                                WheelVector startA;
                                WheelVector startB;
                                Load(&startA, start);
                                Load(&startB, other);
                                WheelScaleAdd(&pointA, &startA, &edgeA, s);
                                WheelScaleAdd(&pointB, &startB, &edgeB, t);
                                direction->x = pointB.x - pointA.x;
                                direction->y = pointB.y - pointA.y;
                                direction->z = pointB.z - pointA.z;
                            } else {
                                WheelVector pointB;
                                WheelVector startB;
                                Load(&startB, other);
                                WheelScaleAdd(&pointB, &startB, &edgeB, t);
                                WheelVector toward;
                                toward.x = pointB.x - start[0];
                                toward.y = pointB.y - start[1];
                                toward.z = pointB.z - start[2];
                                WheelCross(direction, &toward, &edgeA);
                                WheelCross(direction, &edgeA, direction);
                                s = s2;
                            }
                        } else {
                            t = u;
                            if (!(s > 0.0f && s < 1.0f)) {
                                WheelVector toward;
                                toward.x = other[0] - (edgeA.x * s + start[0]);
                                toward.y = other[1] - (start[1] + edgeA.y * s);
                                toward.z = other[2] - (start[2] + edgeA.z * s);
                                WheelCross(direction, &toward, &edgeB);
                                WheelCross(direction, &edgeB, direction);
                            } else {
                                WheelCross(direction, &edgeA, &edgeB);
                                WheelNormalize(direction, direction);
                                if (offset.z * direction->z + offset.x * direction->x + offset.y * direction->y < 0.0f) {
                                    direction->z = -direction->z;
                                    direction->x = -direction->x;
                                    direction->y = -direction->y;
                                }
                                WheelVector toward;
                                toward.x = other[0] - (edgeA.x * s + start[0]);
                                toward.y = other[1] - (start[1] + edgeA.y * s);
                                toward.z = other[2] - (start[2] + edgeA.z * s);
                                WheelVector alternative;
                                WheelCross(&alternative, &toward, &edgeB);
                                WheelCross(&alternative, &edgeB, &alternative);
                                WheelVector startB;
                                Load(&startB, other);
                                WheelVector pointB;
                                WheelScaleAdd(&pointB, &startB, &edgeB, u);
                                toward.x = pointB.x - start[0];
                                toward.y = pointB.y - start[1];
                                toward.z = pointB.z - start[2];
                                WheelVector third;
                                WheelCross(&third, &toward, &edgeA);
                                WheelCross(&third, &edgeA, &third);
                                float lowA = direction->x * end[0] + end[1] * direction->y + end[2] * direction->z;
                                float value = start[0] * direction->x + start[1] * direction->y + start[2] * direction->z;
                                if (value < lowA) {
                                    lowA = value;
                                }
                                float lowB = direction->x * otherEnd[0] + otherEnd[1] * direction->y + otherEnd[2] * direction->z;
                                value = other[0] * direction->x + other[1] * direction->y + other[2] * direction->z;
                                if (value < lowB) {
                                    lowB = value;
                                }
                                float lowA2 = alternative.x * end[0] + alternative.z * end[2] + alternative.y * end[1];
                                value = alternative.x * start[0] + alternative.z * start[2] + alternative.y * start[1];
                                if (value < lowA2) {
                                    lowA2 = value;
                                }
                                float lowB2 = alternative.x * otherEnd[0] + alternative.y * otherEnd[1] + alternative.z * otherEnd[2];
                                value = alternative.x * other[0] + alternative.z * other[2] + alternative.y * other[1];
                                if (value < lowB2) {
                                    lowB2 = value;
                                }
                                float gap = lowB - lowA;
                                if (lowB - lowA < lowB2 - lowA2) {
                                    *direction = alternative;
                                    gap = lowB2 - lowA2;
                                }
                                float lowA3 = third.x * end[0] + third.z * end[2] + third.y * end[1];
                                value = third.x * start[0] + third.z * start[2] + third.y * start[1];
                                if (value < lowA3) {
                                    lowA3 = value;
                                }
                                float lowB3 = third.x * otherEnd[0] + third.z * otherEnd[2] + third.y * otherEnd[1];
                                value = third.x * other[0] + third.z * other[2] + third.y * other[1];
                                if (value < lowB3) {
                                    lowB3 = value;
                                }
                                if (gap < lowB3 - lowA3) {
                                    *direction = third;
                                }
                            }
                        }
                        int highest = 0;
                        float high = direction->y * a[0][1] + direction->z * a[0][2] + direction->x * a[0][0];
                        for (int m = 1; m < countA; ++m) {
                            float value = a[m][1] * direction->y + a[m][2] * direction->z + a[m][0] * direction->x;
                            if (high < value) {
                                high = value;
                                highest = m;
                            }
                        }
                        int lowest = 0;
                        float low = b[0][2] * direction->z + b[0][0] * direction->x + b[0][1] * direction->y;
                        for (int m = 1; m < countB; ++m) {
                            float value = direction->y * b[m][1] + b[m][2] * direction->z + b[m][0] * direction->x;
                            if (value < low) {
                                low = value;
                                lowest = m;
                            }
                        }
                        if (high < low) {
                            closestA80.x = edgeA.x * s + start[0];
                            closestA80.z = start[2] + edgeA.z * s;
                            closestA80.y = start[1] + edgeA.y * s;
                            closestB90.x = edgeB.x * t + other[0];
                            closestB90.z = other[2] + edgeB.z * t;
                            closestB90.y = other[1] + edgeB.y * t;
                            float distance = SquaredGap(&closestA80, &closestB90);
                            if ((highest == i || highest == j) && (lowest == k || lowest == l)) {
                                if (s == 0.0f) {
                                    countC0 = 1;
                                    indicesB0[0] = indicesB0[i];
                                    Store(a[0], a[i]);
                                } else if (s == 1.0f) {
                                    countC0 = 1;
                                    indicesB0[0] = indicesB0[j];
                                    Store(a[0], a[j]);
                                } else {
                                    countC0 = 2;
                                    indicesB0[0] = indicesB0[i];
                                    Store(a[0], a[i]);
                                    indicesB0[1] = indicesB0[j];
                                    Store(a[1], a[j]);
                                }
                                if (t == 0.0f) {
                                    countD4 = 1;
                                    indicesC4[0] = indicesC4[k];
                                    Store(b[0], b[k]);
                                    return distance;
                                }
                                if (t == 1.0f) {
                                    countD4 = 1;
                                    indicesC4[0] = indicesC4[l];
                                    Store(b[0], b[l]);
                                    return distance;
                                }
                                countD4 = 2;
                                indicesC4[0] = indicesC4[k];
                                indicesC4[1] = indicesC4[l];
                                Store(b[0], b[k]);
                                Store(b[1], b[l]);
                                return distance;
                            }
                            if (distance < best) {
                                bestA = closestA80;
                                bestB = closestB90;
                                bestDirection = *direction;
                                found = 1;
                                if (s == 0.0f) {
                                    bestCountA = 1;
                                    bestIndicesA[0] = i;
                                } else if (s != 1.0f) {
                                    bestCountA = 2;
                                    bestIndicesA[1] = j;
                                    bestIndicesA[0] = i;
                                } else {
                                    bestCountA = 1;
                                    bestIndicesA[0] = j;
                                }
                                best = distance;
                                if (t == 0.0f) {
                                    bestCountB = 1;
                                    bestIndicesB[0] = k;
                                } else if (t == 1.0f) {
                                    bestCountB = 1;
                                    bestIndicesB[0] = l;
                                } else {
                                    bestCountB = 2;
                                    bestIndicesB[0] = k;
                                    bestIndicesB[1] = l;
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    // Triangles of A against the points of B.
    if (countA != 2 && countA - 2 >= 0) {
        for (int i = 0; i < countA - 2; ++i) {
            for (int j = i + 1; j < countA - 1; ++j) {
                for (int k = j + 1; k < countA; ++k) {
                    int remaining = 6 - i - j - k;
                    WheelVector first;
                    first.x = a[j][0] - a[i][0];
                    first.y = a[j][1] - a[i][1];
                    first.z = a[j][2] - a[i][2];
                    WheelVector second;
                    second.x = a[k][0] - a[i][0];
                    second.y = a[k][1] - a[i][1];
                    second.z = a[k][2] - a[i][2];
                    WheelVector normal;
                    WheelCross(&normal, &first, &second);
                    WheelNormalize(&normal, &normal);
                    if (0.0f <= (b[0][0] - a[i][0]) * normal.x + normal.y * (b[0][1] - a[i][1])
                                  + normal.z * (b[0][2] - a[i][2])) {
                        direction->x = normal.x;
                        direction->y = normal.y;
                        direction->z = normal.z;
                    } else {
                        direction->x = -normal.x;
                        direction->y = -normal.y;
                        direction->z = -normal.z;
                    }
                    int highest = i;
                    float plane = a[i][2] * direction->z + a[i][0] * direction->x + direction->y * a[i][1];
                    float high = plane;
                    if (remaining < countA) {
                        float value = direction->y * a[remaining][1] + a[remaining][2] * direction->z
                                    + a[remaining][0] * direction->x;
                        if (plane < value) {
                            highest = remaining;
                            high = value;
                        }
                    }
                    int lowest = 0;
                    float low = b[0][0] * direction->x + b[0][1] * direction->y + b[0][2] * direction->z;
                    for (int m = 1; m < countB; ++m) {
                        float value = direction->y * b[m][1] + b[m][2] * direction->z + b[m][0] * direction->x;
                        if (value < low) {
                            low = value;
                            lowest = m;
                        }
                    }
                    if (!(high < low)) {
                        continue;
                    }
                    float *point = b[lowest];
                    WheelVector toward;
                    WheelVector edge;
                    WheelVector side;
                    toward.x = point[0] - a[i][0];
                    toward.y = point[1] - a[i][1];
                    toward.z = point[2] - a[i][2];
                    edge.x = a[j][0] - a[i][0];
                    edge.y = a[j][1] - a[i][1];
                    edge.z = a[j][2] - a[i][2];
                    WheelCross(&side, &normal, &edge);
                    if (!(0.0f < side.x * toward.x + side.y * toward.y + side.z * toward.z)) {
                        continue;
                    }
                    toward.x = point[0] - a[j][0];
                    toward.y = point[1] - a[j][1];
                    toward.z = point[2] - a[j][2];
                    edge.x = a[k][0] - a[j][0];
                    edge.y = a[k][1] - a[j][1];
                    edge.z = a[k][2] - a[j][2];
                    WheelCross(&side, &normal, &edge);
                    if (!(0.0f < side.x * toward.x + side.y * toward.y + side.z * toward.z)) {
                        continue;
                    }
                    toward.x = point[0] - a[k][0];
                    toward.y = point[1] - a[k][1];
                    toward.z = point[2] - a[k][2];
                    edge.x = a[i][0] - a[k][0];
                    edge.y = a[i][1] - a[k][1];
                    edge.z = a[i][2] - a[k][2];
                    WheelCross(&side, &normal, &edge);
                    if (!(0.0f < side.x * toward.x + side.y * toward.y + side.z * toward.z)) {
                        continue;
                    }
                    float gap = low - plane;
                    WheelVector shift;
                    shift.x = gap * direction->x;
                    shift.y = gap * direction->y;
                    shift.z = gap * direction->z;
                    closestA80.x = point[0] - shift.x;
                    closestA80.y = point[1] - shift.y;
                    closestA80.z = point[2] - shift.z;
                    Load(&closestB90, point);
                    float distance = shift.x * shift.x + shift.y * shift.y + shift.z * shift.z;
                    if (highest == i) {
                        countC0 = 3;
                        indicesB0[0] = indicesB0[i];
                        Store(a[0], a[i]);
                        indicesB0[1] = indicesB0[j];
                        Store(a[1], a[j]);
                        indicesB0[2] = indicesB0[k];
                        Store(a[2], a[k]);
                        countD4 = 1;
                        indicesC4[0] = indicesC4[lowest];
                        Store(b[0], b[lowest]);
                        return distance;
                    }
                    if (distance < best) {
                        bestA = closestA80;
                        bestB = closestB90;
                        bestDirection = *direction;
                        found = 1;
                        bestIndicesA[0] = i;
                        bestCountB = 1;
                        bestCountA = 3;
                        bestIndicesA[1] = j;
                        bestIndicesA[2] = k;
                        bestIndicesB[0] = lowest;
                        best = distance;
                    }
                }
            }
        }
    }
    // Triangles of B against the points of A.
    if (countB != 2 && countB - 2 >= 0) {
        for (int i = 0; i < countB - 2; ++i) {
            for (int j = i + 1; j < countB - 1; ++j) {
                for (int k = j + 1; k < countB; ++k) {
                    int remaining = 6 - i - j - k;
                    WheelVector first;
                    first.x = b[j][0] - b[i][0];
                    first.y = b[j][1] - b[i][1];
                    first.z = b[j][2] - b[i][2];
                    WheelVector second;
                    second.x = b[k][0] - b[i][0];
                    second.y = b[k][1] - b[i][1];
                    second.z = b[k][2] - b[i][2];
                    WheelVector normal;
                    WheelCross(&normal, &first, &second);
                    WheelNormalize(&normal, &normal);
                    if (0.0f <= (b[i][0] - a[0][0]) * normal.x + (b[i][1] - a[0][1]) * normal.y
                                  + normal.z * (b[i][2] - a[0][2])) {
                        direction->x = normal.x;
                        direction->y = normal.y;
                        direction->z = normal.z;
                    } else {
                        direction->x = -normal.x;
                        direction->y = -normal.y;
                        direction->z = -normal.z;
                    }
                    int lowest = i;
                    float plane = b[i][0] * direction->x + b[i][1] * direction->y + b[i][2] * direction->z;
                    float low = plane;
                    if (remaining < countB) {
                        float value = b[remaining][1] * direction->y + b[remaining][2] * direction->z
                                    + b[remaining][0] * direction->x;
                        if (value < plane) {
                            lowest = remaining;
                            low = value;
                        }
                    }
                    int highest = 0;
                    float high = direction->y * a[0][1] + direction->z * a[0][2] + direction->x * a[0][0];
                    for (int m = 1; m < countA; ++m) {
                        float value = a[m][1] * direction->y + a[m][2] * direction->z + a[m][0] * direction->x;
                        if (high < value) {
                            high = value;
                            highest = m;
                        }
                    }
                    (void)low;
                    if (!(high < low)) {
                        continue;
                    }
                    float *point = a[highest];
                    WheelVector toward;
                    WheelVector edge;
                    WheelVector side;
                    toward.x = point[0] - b[i][0];
                    toward.y = point[1] - b[i][1];
                    toward.z = point[2] - b[i][2];
                    edge.x = b[j][0] - b[i][0];
                    edge.y = b[j][1] - b[i][1];
                    edge.z = b[j][2] - b[i][2];
                    WheelCross(&side, &normal, &edge);
                    if (!(0.0f < side.x * toward.x + side.y * toward.y + side.z * toward.z)) {
                        continue;
                    }
                    toward.x = point[0] - b[j][0];
                    toward.y = point[1] - b[j][1];
                    toward.z = point[2] - b[j][2];
                    edge.x = b[k][0] - b[j][0];
                    edge.y = b[k][1] - b[j][1];
                    edge.z = b[k][2] - b[j][2];
                    WheelCross(&side, &normal, &edge);
                    if (!(0.0f < side.x * toward.x + side.y * toward.y + side.z * toward.z)) {
                        continue;
                    }
                    toward.x = point[0] - b[k][0];
                    toward.y = point[1] - b[k][1];
                    toward.z = point[2] - b[k][2];
                    edge.x = b[i][0] - b[k][0];
                    edge.y = b[i][1] - b[k][1];
                    edge.z = b[i][2] - b[k][2];
                    WheelCross(&side, &normal, &edge);
                    if (!(0.0f < side.x * toward.x + side.y * toward.y + side.z * toward.z)) {
                        continue;
                    }
                    float gap = plane - high;
                    WheelVector shift;
                    shift.x = gap * direction->x;
                    shift.y = gap * direction->y;
                    shift.z = gap * direction->z;
                    Load(&closestA80, point);
                    closestB90.x = shift.x + point[0];
                    closestB90.y = point[1] + shift.y;
                    closestB90.z = point[2] + shift.z;
                    float distance = shift.x * shift.x + shift.y * shift.y + shift.z * shift.z;
                    if (lowest == i) {
                        countC0 = 1;
                        indicesB0[0] = indicesB0[highest];
                        Store(a[0], a[highest]);
                        countD4 = 3;
                        indicesC4[0] = indicesC4[i];
                        Store(b[0], b[i]);
                        indicesC4[1] = indicesC4[j];
                        Store(b[1], b[j]);
                        indicesC4[2] = indicesC4[k];
                        Store(b[2], b[k]);
                        return distance;
                    }
                    if (distance < best) {
                        bestA = closestA80;
                        bestB = closestB90;
                        bestDirection = *direction;
                        found = 1;
                        bestCountA = 1;
                        bestIndicesB[0] = i;
                        bestIndicesA[0] = highest;
                        bestCountB = 3;
                        bestIndicesB[1] = j;
                        bestIndicesB[2] = k;
                        best = distance;
                    }
                }
            }
        }
    }
    if (!found) {
        return 0.0f;
    }
    closestA80 = bestA;
    closestB90 = bestB;
    *direction = bestDirection;
    countC0 = bestCountA;
    countD4 = bestCountB;
    for (int m = 0; m < bestCountA; ++m) {
        int index = bestIndicesA[m];
        indicesB0[m] = indicesB0[index];
        Store(a[m], a[index]);
    }
    for (int m = 0; m < bestCountB; ++m) {
        int index = bestIndicesB[m];
        indicesC4[m] = indicesC4[index];
        Store(b[m], b[index]);
    }
    return best;
}
