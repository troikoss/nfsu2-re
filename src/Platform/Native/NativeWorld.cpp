// Platform scaffolding: loads track bundles from the player's installation and feeds
// their chunks to the recovered handlers. The original streaming loader (section
// visibility, asynchronous reads) is not recovered; every collision pack is loaded.
#include "NativeWorld.h"
#include "NativeVehicle.h"
#include "../../Speed/Indep/Src/World/TrackPositionMarkers.h"
#include "../../Speed/Indep/Src/Hud/HudMinimap.h"

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

extern TrackInfo *currentTrack883DAC;
void NativeTrap(const char *what);
void HudMinimapSeam(const char *what) { NativeTrap(what); }

// Road navigation (0x5da370) needs the unrecovered road network; the free-roam markers make the
// start grid's navigator fallback unreachable.
void WRoadNav::Advance5DA370(float, float, int) { NativeTrap("road navigator 0x5da370"); }

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
    // Original start: the free-roam marker file (0x57f2d0 picks TrackPosMarkersFreeRoam.bin
    // when the track number is a multiple of 1000), loaded by recovered 0x5d8470, and the race
    // grid 0x5eb5d0 as built by 0x6065b0. Native session seam: the one human participant has
    // grid index 1, so it takes the first of the six positions.
    std::vector<unsigned char> markerData;
    path = std::string(gameDirectory) + "/TRACKS/ROUTES" + region + "/TrackPosMarkersFreeRoam.bin";
    if (!ReadWholeFile(path, &markerData)) {
        snprintf(error, errorSize, "cannot read %s", path.c_str());
        return false;
    }
    InitialiseTrackPositionMarkers();
    for (size_t offset = 0; offset + 8 <= markerData.size();) {
        unsigned int size = Read32(markerData, offset + 4);
        LoadTrackPositionMarkers(reinterpret_cast<unsigned int *>(CopyChunk(markerData.data(), offset, size)));
        offset += 8 + size;
    }
    currentTrack883DAC = FindTrackInfo(4000);
    StartingPosition positions[6];
    memset(positions, 0, sizeof(positions));
    GetStartingPositions(6, positions);
    track->spawn[0] = positions[0].position0[0];
    track->spawn[1] = positions[0].position0[1];
    track->spawn[2] = positions[0].position0[2];
    track->heading = positions[0].angle10 * (6.28318530718f / 65536.0f);
    track->spawnFound = true;
    return true;
}
