#ifndef UG2_NATIVE_ASSETS_H
#define UG2_NATIVE_ASSETS_H

// Platform scaffolding: reads UG2's own solid packs (GEOMETRY.BIN, track bundles) and
// texture packs from the player's installation into plain arrays for the demo renderer.
// The original eSolid/TextureInfo loaders (0x493bb0, 0x494ea0) and the D3D renderer are
// not recovered; only the decompressors in bWare/Compression.cpp are original code.
#include <map>
#include <string>
#include <vector>

struct NativeImage {
    unsigned int hash;
    std::string name;
    int width;
    int height;
    bool alpha;
    int alphaUsage;  // TextureInfo +0x49: 0 none, 1 punch-through, 2 modulated (survey of L4RA)
    int alphaBlend;  // TextureInfo +0x4a: 0 none, 1 blended, 2 additive (survey of L4RA)
    std::vector<unsigned char> rgba;
};

// One 0x134b02 draw record (60 bytes): index range and per-solid texture slot.
struct NativeMeshPart {
    unsigned int textureHash;
    int start;
    int count;
    unsigned int flags;
};

struct NativeSolid {
    std::string name;
    unsigned int hash;
    int stride;
    std::vector<float> positions;
    std::vector<float> normals;
    std::vector<unsigned int> colours;
    std::vector<float> uvs;
    std::vector<unsigned short> indices;
    std::vector<NativeMeshPart> parts;
    float minimum[3];
    float maximum[3];
};

typedef std::map<unsigned int, NativeImage> NativeImageMap;

// One 0x34103 scenery instance (0x40 bytes) resolved through its section's 0x34102 info.
struct NativeInstance {
    unsigned int solidHash;
    unsigned int flags;
    std::string infoName;
    int section;  // 0x34101 header +0xc: letter (n / 100 + 0x40) and number (n % 100)
    float minimum[3];
    float maximum[3];
    float matrix[16];
};

// One 0x34153 drivable visible section (0xa4 bytes) joined with its 0x34152 boundary
// (point count +0xa, bounds +0xc/+0x14, x/y points from +0x24).
struct NativeDrivableSection {
    int section;              // +0x8
    std::vector<short> visible;  // +0x10 count, +0x14 section numbers
    float minimum[2];
    float maximum[2];
    std::vector<float> points;
};

unsigned int NativeStringHash(const char *text);
bool ReadNativeFile(const std::string &path, std::vector<unsigned char> *data);
// Walks every 0x80134000 solid pack in [data, data+size).
void LoadNativeSolids(const unsigned char *data, size_t size, std::vector<NativeSolid> *solids);
// Walks every 0x80034100 scenery section in the buffer.
void LoadNativeInstances(const unsigned char *data, size_t size, std::vector<NativeInstance> *instances);
// Reads the 0x80034150 visible-section pack (loader 0x5e9c10).
void LoadNativeDrivableSections(const unsigned char *data, size_t size, std::vector<NativeDrivableSection> *sections);
// Reads every 0xb3300000 texture pack (plain or slot-compressed) in the buffer.
void LoadNativeTextures(const unsigned char *data, size_t size, NativeImageMap *images);

#endif
