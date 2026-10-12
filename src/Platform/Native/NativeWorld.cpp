// Platform scaffolding: loads track bundles from the player's installation and feeds
// their chunks to the recovered handlers. The original streaming loader (section
// visibility, asynchronous reads) is not recovered; every collision pack is loaded.
#include "NativeWorld.h"
#include "NativeVehicle.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>

namespace {

std::vector<unsigned char *> &Buffers() {
    static std::vector<unsigned char *> buffers;
    return buffers;
}

// Copy a chunk into memory that keeps the file's 16-byte alignment phase.
int *CopyChunk(const unsigned char *data, size_t offset, unsigned int size) {
    unsigned char *raw = static_cast<unsigned char *>(malloc(size + 8 + 32));
    unsigned char *aligned = reinterpret_cast<unsigned char *>(
        ((reinterpret_cast<size_t>(raw) + 15) & ~static_cast<size_t>(15)) + (offset & 15));
    memcpy(aligned, data + offset, size + 8);
    Buffers().push_back(raw);
    return reinterpret_cast<int *>(aligned);
}

bool ReadWholeFile(const std::string &path, std::vector<unsigned char> *data) {
    FILE *file = fopen(path.c_str(), "rb");
    if (!file) {
        return false;
    }
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    data->resize(size);
    size_t read = fread(data->data(), 1, size, file);
    fclose(file);
    return read == (size_t)size;
}

unsigned int Read32(const std::vector<unsigned char> &data, size_t offset) {
    return data[offset] | data[offset + 1] << 8 | data[offset + 2] << 16 | (unsigned int)data[offset + 3] << 24;
}

}  // namespace

// Platform seam for the bWare heap (0x440390 bMalloc / 0x440540 free) that the
// recovered slot pools (Libs/bWare/bSlotPool.cpp) allocate their blocks from.
void *bMalloc(int size, const char *, int, int) {
    return calloc(1, size);
}

void bFreeMemory(void *memory) {
    free(memory);
}

bool LoadNativeTrack(const char *gameDirectory, const char *region, NativeTrack *track, char *error,
                     int errorSize) {
    memset(track, 0, sizeof(*track));
    RunWorldStaticInitializers();
    std::vector<unsigned char> regionData;
    std::string path = std::string(gameDirectory) + "/TRACKS/" + region + ".BUN";
    if (!ReadWholeFile(path, &regionData)) {
        snprintf(error, errorSize, "cannot read %s", path.c_str());
        return false;
    }
    for (size_t offset = 0; offset + 8 <= regionData.size();) {
        unsigned int id = Read32(regionData, offset);
        unsigned int size = Read32(regionData, offset + 4);
        if (id == 0x80034150) {
            trackTopology.LoadChunk(CopyChunk(regionData.data(), offset, size));
        } else if (id == 0x3412f) {
            collisionManager.LoadChunk(CopyChunk(regionData.data(), offset, size));
        }
        offset += 8 + size;
    }
    InitialiseCollisionCache(&collisionManager, 0);
    std::vector<unsigned char> streamData;
    path = std::string(gameDirectory) + "/TRACKS/STREAM" + region + ".BUN";
    if (!ReadWholeFile(path, &streamData)) {
        snprintf(error, errorSize, "cannot read %s", path.c_str());
        return false;
    }
    for (size_t offset = 0; offset + 8 <= streamData.size();) {
        unsigned int id = Read32(streamData, offset);
        unsigned int size = Read32(streamData, offset + 4);
        if (id == 0x80034130) {
            collisionManager.LoadChunk(CopyChunk(streamData.data(), offset, size));
            ++track->packCount;
        }
        offset += 8 + size;
    }
    // Scaffold spawn: a flat ROAD polygon near the map centre with road 30 m ahead and
    // behind along its longest edge (the original race start data is not yet recovered).
    float minimum[2] = {1e30f, 1e30f}, maximum[2] = {-1e30f, -1e30f};
    for (WCollisionLink *link = collisionManager.packs0.next; link != &collisionManager.packs0;
         link = link->next) {
        WCollisionPack *pack = reinterpret_cast<WCollisionPack *>(link);
        for (int k = 0; k < 2; ++k) {
            if (pack->minimum40[k] < minimum[k]) minimum[k] = pack->minimum40[k];
            if (pack->maximum50[k] > maximum[k]) maximum[k] = pack->maximum50[k];
        }
    }
    float centre[2] = {(minimum[0] + maximum[0]) * 0.5f, (minimum[1] + maximum[1]) * 0.5f};
    float bestDistance = 1e30f;
    for (WCollisionLink *link = collisionManager.packs0.next; link != &collisionManager.packs0;
         link = link->next) {
        WCollisionPack *pack = reinterpret_cast<WCollisionPack *>(link);
        for (int i = 0; i < pack->polygonCountC; ++i) {
            WCollisionPolygon *polygon = &pack->polygons8[i];
            if (polygon->surface2 != 1 || (polygon->flags3 & 0x0e)) {
                continue;
            }
            float vertices[4][4];
            polygon->Decode(vertices[0]);
            int count = (polygon->flags3 & 0x10) ? 4 : 3;
            float x = 0, y = 0, z = 0, edge[2] = {0, 0}, longest = 0;
            for (int k = 0; k < count; ++k) {
                x += vertices[k][0];
                y += vertices[k][1];
                z += vertices[k][2];
                float dx = vertices[(k + 1) % count][0] - vertices[k][0];
                float dy = vertices[(k + 1) % count][1] - vertices[k][1];
                float dz = vertices[(k + 1) % count][2] - vertices[k][2];
                if (dz * dz > 0.01f * (dx * dx + dy * dy)) {
                    longest = -1;
                    break;
                }
                if (dx * dx + dy * dy > longest) {
                    longest = dx * dx + dy * dy;
                    edge[0] = dx;
                    edge[1] = dy;
                }
            }
            if (longest <= 4.0f) {
                continue;
            }
            x /= count;
            y /= count;
            z /= count;
            float distance = (x - centre[0]) * (x - centre[0]) + (y - centre[1]) * (y - centre[1]);
            if (distance >= bestDistance) {
                continue;
            }
            float length = sqrtf(longest);
            bool covered = true;
            for (int side = -1; side <= 1 && covered; side += 2) {
                for (float along = 10.0f; along <= 30.0f && covered; along += 10.0f) {
                    float px = x + side * edge[0] / length * along;
                    float py = y + side * edge[1] / length * along;
                    bool hit = false;
                    for (int q = 0; q < pack->polygonCountC && !hit; ++q) {
                        WCollisionPolygon *other = &pack->polygons8[q];
                        if (other->surface2 != 1 || (other->flags3 & 0x0e)) continue;
                        float v[4][4];
                        other->Decode(v[0]);
                        int n = (other->flags3 & 0x10) ? 4 : 3;
                        bool inside = false;
                        for (int a = 0, b = n - 1; a < n; b = a++) {
                            if (((v[a][1] <= py && py < v[b][1]) || (v[b][1] <= py && py < v[a][1]))
                                && px < (py - v[a][1]) * (v[b][0] - v[a][0]) / (v[b][1] - v[a][1]) + v[a][0]) {
                                inside = !inside;
                            }
                        }
                        hit = inside;
                    }
                    covered = hit;
                }
            }
            if (!covered) {
                continue;
            }
            bestDistance = distance;
            track->spawn[0] = x;
            track->spawn[1] = y;
            track->spawn[2] = z;
            track->heading = atan2f(edge[1], edge[0]);
            track->spawnFound = true;
        }
    }
    return true;
}
