// Platform scaffolding: decodes UG2 solid and texture packs for the demo renderer.
// Chunk layouts follow the retail data and the loader fixups noted in docs/NATIVE_DEMO.md;
// pixel formats are standard DXT/palette formats converted to RGBA for OpenGL.
#include "NativeAssets.h"
#include "../../Speed/Indep/Libs/bWare/Compression.h"

#include <stdio.h>
#include <string.h>

namespace {

unsigned int Get32(const unsigned char *p) {
    return p[0] | p[1] << 8 | p[2] << 16 | (unsigned int)p[3] << 24;
}

unsigned short Get16(const unsigned char *p) {
    return (unsigned short)(p[0] | p[1] << 8);
}

float GetFloat(const unsigned char *p) {
    unsigned int bits = Get32(p);
    float value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

// Payloads are aligned relative to the file start, as the game loads bundles aligned.
size_t Align(size_t offset, size_t alignment) {
    return (offset + alignment - 1) & ~(alignment - 1);
}

struct Chunk {
    unsigned int id;
    size_t begin;
    size_t end;
};

void Children(const unsigned char *data, size_t begin, size_t end, std::vector<Chunk> *out) {
    for (size_t offset = begin; offset + 8 <= end;) {
        Chunk chunk;
        chunk.id = Get32(data + offset);
        chunk.begin = offset + 8;
        chunk.end = chunk.begin + Get32(data + offset + 4);
        if (chunk.end > end) {
            break;
        }
        out->push_back(chunk);
        offset = chunk.end;
    }
}

void Expand565(unsigned short colour, unsigned char *out) {
    out[0] = (unsigned char)(((colour >> 11) & 0x1f) * 255 / 31);
    out[1] = (unsigned char)(((colour >> 5) & 0x3f) * 255 / 63);
    out[2] = (unsigned char)((colour & 0x1f) * 255 / 31);
    out[3] = 255;
}

void DecodeColourBlock(const unsigned char *block, bool allowTransparent, unsigned char palette[4][4]) {
    unsigned short c0 = Get16(block), c1 = Get16(block + 2);
    Expand565(c0, palette[0]);
    Expand565(c1, palette[1]);
    for (int k = 0; k < 3; ++k) {
        if (c0 > c1 || !allowTransparent) {
            palette[2][k] = (unsigned char)((2 * palette[0][k] + palette[1][k]) / 3);
            palette[3][k] = (unsigned char)((palette[0][k] + 2 * palette[1][k]) / 3);
        } else {
            palette[2][k] = (unsigned char)((palette[0][k] + palette[1][k]) / 2);
            palette[3][k] = 0;
        }
    }
    palette[2][3] = 255;
    palette[3][3] = (c0 > c1 || !allowTransparent) ? 255 : 0;
}

// Decodes DXT1/3/5 base level into RGBA.
void DecodeDxt(const unsigned char *src, int width, int height, int format, unsigned char *out) {
    int blockBytes = format == 1 ? 8 : 16;
    for (int by = 0; by < (height + 3) / 4; ++by) {
        for (int bx = 0; bx < (width + 3) / 4; ++bx) {
            const unsigned char *block = src + (by * ((width + 3) / 4) + bx) * blockBytes;
            const unsigned char *colour = format == 1 ? block : block + 8;
            unsigned char palette[4][4];
            DecodeColourBlock(colour, format == 1, palette);
            unsigned char alpha[16];
            if (format == 3) {
                for (int i = 0; i < 16; ++i) {
                    int nibble = (block[i / 2] >> ((i & 1) * 4)) & 0xf;
                    alpha[i] = (unsigned char)(nibble * 17);
                }
            } else if (format == 5) {
                unsigned char a[8];
                a[0] = block[0];
                a[1] = block[1];
                for (int i = 2; i < 8; ++i) {
                    a[i] = a[0] > a[1] ? (unsigned char)(((8 - i) * a[0] + (i - 1) * a[1]) / 7)
                                       : (i < 6 ? (unsigned char)(((6 - i) * a[0] + (i - 1) * a[1]) / 5)
                                                : (unsigned char)(i == 6 ? 0 : 255));
                }
                unsigned long long bits = 0;
                for (int i = 0; i < 6; ++i) bits |= (unsigned long long)block[2 + i] << (8 * i);
                for (int i = 0; i < 16; ++i) alpha[i] = a[(bits >> (3 * i)) & 7];
            }
            unsigned int indices = Get32(colour + 4);
            for (int i = 0; i < 16; ++i) {
                int x = bx * 4 + (i & 3), y = by * 4 + (i >> 2);
                if (x >= width || y >= height) continue;
                unsigned char *pixel = out + (y * width + x) * 4;
                memcpy(pixel, palette[(indices >> (2 * i)) & 3], 4);
                if (format != 1) pixel[3] = alpha[i];
            }
        }
    }
}

// `info` points at a 0x7c-byte TextureInfo (name[24], hash +0x18, data offset +0x24,
// palette offset +0x28, data size +0x2c, size +0x38, compression type +0x3e).
bool DecodeImage(const unsigned char *info, const unsigned char *pixels, size_t pixelBytes,
                 const unsigned char *palette, NativeImage *image) {
    char name[25];
    memcpy(name, info, 24);
    name[24] = 0;
    image->name = name;
    image->hash = Get32(info + 0x18);
    image->width = Get16(info + 0x38);
    image->height = Get16(info + 0x3a);
    int type = info[0x3e];
    int width = image->width, height = image->height;
    if (width <= 0 || height <= 0 || width > 4096 || height > 4096) return false;
    size_t count = (size_t)width * height;
    image->rgba.resize(count * 4);
    unsigned char *out = &image->rgba[0];
    if (type == 0x22 || type == 0x24 || type == 0x26) {
        int format = type == 0x22 ? 1 : (type == 0x24 ? 3 : 5);
        size_t need = (size_t)((width + 3) / 4) * ((height + 3) / 4) * (format == 1 ? 8 : 16);
        if (need > pixelBytes) return false;
        DecodeDxt(pixels, width, height, format, out);
    } else if (type == 0x20) {
        if (count * 4 > pixelBytes) return false;
        for (size_t i = 0; i < count; ++i) {
            out[i * 4 + 0] = pixels[i * 4 + 2];
            out[i * 4 + 1] = pixels[i * 4 + 1];
            out[i * 4 + 2] = pixels[i * 4 + 0];
            out[i * 4 + 3] = pixels[i * 4 + 3];
        }
    } else if (type == 0x08 && palette) {
        if (count > pixelBytes) return false;
        for (size_t i = 0; i < count; ++i) {
            const unsigned char *entry = palette + pixels[i] * 4;
            out[i * 4 + 0] = entry[2];
            out[i * 4 + 1] = entry[1];
            out[i * 4 + 2] = entry[0];
            out[i * 4 + 3] = entry[3];
        }
    } else {
        return false;
    }
    image->alpha = false;
    for (size_t i = 0; i < count; ++i) {
        if (out[i * 4 + 3] != 255) {
            image->alpha = true;
            break;
        }
    }
    return true;
}

void LoadTexturePack(const unsigned char *data, size_t begin, size_t end, NativeImageMap *images) {
    std::vector<Chunk> top;
    Children(data, begin, end, &top);
    const unsigned char *infos = 0;
    size_t infoCount = 0;
    const unsigned char *slots = 0;
    size_t slotCount = 0;
    const unsigned char *pixelBase = 0;
    size_t pixelBytes = 0;
    for (size_t i = 0; i < top.size(); ++i) {
        std::vector<Chunk> inner;
        Children(data, top[i].begin, top[i].end, &inner);
        for (size_t j = 0; j < inner.size(); ++j) {
            const Chunk &c = inner[j];
            if (c.id == 0x33310004) {
                // Records follow a fixed 12-byte pad; the chunk size counts whole records, so
                // the last record's unused zero tail lies past the chunk.
                infos = data + c.begin + 12;
                infoCount = (c.end - c.begin) / 0x7c;
            } else if (c.id == 0x33310003) {
                slots = data + c.begin;
                slotCount = (c.end - c.begin) / 0x18;
            } else if (c.id == 0x33320002) {
                size_t start = Align(c.begin, 0x80);
                pixelBase = data + start;
                pixelBytes = c.end - start;
            }
        }
    }
    for (size_t i = 0; infos && pixelBase && i < infoCount; ++i) {
        const unsigned char *info = infos + i * 0x7c;
        size_t offset = Get32(info + 0x24), bytes = Get32(info + 0x2c);
        if (offset + bytes > pixelBytes) continue;
        size_t paletteOffset = Get32(info + 0x28);
        const unsigned char *palette = Get32(info + 0x30) && paletteOffset + 1024 <= pixelBytes
                                           ? pixelBase + paletteOffset : 0;
        NativeImage image;
        if (DecodeImage(info, pixelBase + offset, bytes, palette, &image) && !images->count(image.hash)) {
            (*images)[image.hash] = image;
        }
    }
    // Car packs: each 24-byte slot is (hash, file offset, packed size, size, info distance
    // from the end, 0); the payload is the pixels followed by the TextureInfo.
    for (size_t i = 0; slots && i < slotCount; ++i) {
        const unsigned char *slot = slots + i * 0x18;
        unsigned int hash = Get32(slot), offset = Get32(slot + 4), packed = Get32(slot + 8);
        unsigned int size = Get32(slot + 12), fromEnd = Get32(slot + 16);
        if (images->count(hash) || offset + packed > end || size < 0x7c || fromEnd > size + 0x18) continue;
        std::vector<unsigned char> source(packed + 64), unpacked(size + 64);
        memcpy(&source[0], data + offset, packed);
        if (bDecompress(reinterpret_cast<unsigned int *>(&source[0]), &unpacked[0]) != (int)size) continue;
        const unsigned char *info = &unpacked[size - fromEnd + 0x70];
        if (Get32(info + 0x18) != hash) continue;
        NativeImage image;
        if (DecodeImage(info, &unpacked[0], size - fromEnd + 0x70, 0, &image)) {
            (*images)[hash] = image;
        }
    }
}

void LoadSolid(const unsigned char *data, size_t begin, size_t end, std::vector<NativeSolid> *solids) {
    std::vector<Chunk> chunks, stack;
    Children(data, begin, end, &stack);
    // Flatten nested containers (0x80134100 platform blocks).
    while (!stack.empty()) {
        Chunk c = stack.front();
        stack.erase(stack.begin());
        if (c.id & 0x80000000) {
            Children(data, c.begin, c.end, &stack);
        } else {
            chunks.push_back(c);
        }
    }
    NativeSolid solid;
    std::vector<unsigned int> slots;
    const unsigned char *records = 0;
    size_t recordCount = 0;
    const unsigned char *vertices = 0;
    size_t vertexBytes = 0;
    int vertexCount = 0;
    for (size_t i = 0; i < chunks.size(); ++i) {
        const Chunk &c = chunks[i];
        size_t start = Align(c.begin, 16);
        if (c.id == 0x134011) {
            char name[29];
            memcpy(name, data + start + 0xa4, 28);
            name[28] = 0;
            solid.name = name;
            solid.hash = Get32(data + start + 0x10);
            for (int k = 0; k < 3; ++k) {
                solid.minimum[k] = GetFloat(data + start + 0x20 + 4 * k);
                solid.maximum[k] = GetFloat(data + start + 0x30 + 4 * k);
            }
        } else if (c.id == 0x134012) {
            for (size_t p = c.begin; p + 8 <= c.end; p += 8) slots.push_back(Get32(data + p));
        } else if (c.id == 0x134900) {
            vertexCount = (int)Get32(data + start + 0x34);
        } else if (c.id == 0x134b01) {
            size_t aligned = Align(c.begin, 128);
            vertices = data + aligned;
            vertexBytes = c.end - aligned;
        } else if (c.id == 0x134b02) {
            records = data + start;
            recordCount = (c.end - start) / 60;
        } else if (c.id == 0x134b03) {
            for (size_t p = start; p + 2 <= c.end; p += 2) solid.indices.push_back(Get16(data + p));
        }
    }
    if (!vertices || vertexCount <= 0 || !records || solid.indices.empty()) return;
    size_t stride = vertexBytes / vertexCount;
    if (stride != 24 && stride != 36) return;
    solid.stride = (int)stride;
    for (int v = 0; v < vertexCount; ++v) {
        const unsigned char *p = vertices + v * stride;
        for (int k = 0; k < 3; ++k) solid.positions.push_back(GetFloat(p + 4 * k));
        if (stride == 36) {
            for (int k = 0; k < 3; ++k) solid.normals.push_back(GetFloat(p + 12 + 4 * k));
            solid.colours.push_back(Get32(p + 24));
            solid.uvs.push_back(GetFloat(p + 28));
            solid.uvs.push_back(GetFloat(p + 32));
        } else {
            solid.colours.push_back(Get32(p + 12));
            solid.uvs.push_back(GetFloat(p + 16));
            solid.uvs.push_back(GetFloat(p + 20));
        }
    }
    for (size_t r = 0; r < recordCount; ++r) {
        const unsigned char *record = records + r * 60;
        NativeMeshPart part;
        unsigned int slot = Get32(record + 0x1c);
        part.textureHash = slot < slots.size() ? slots[slot] : 0;
        part.count = (int)Get32(record + 0xc);
        part.start = (int)Get32(record + 0x34);
        part.flags = Get32(record + 0x38);
        if (part.start < 0 || part.count <= 0 || (size_t)(part.start + part.count) > solid.indices.size()) continue;
        bool valid = true;
        for (int k = 0; k < part.count && valid; ++k) valid = solid.indices[part.start + k] < vertexCount;
        if (valid) solid.parts.push_back(part);
    }
    solids->push_back(solid);
}

void WalkSolids(const unsigned char *data, size_t begin, size_t end, std::vector<NativeSolid> *solids) {
    std::vector<Chunk> chunks;
    Children(data, begin, end, &chunks);
    for (size_t i = 0; i < chunks.size(); ++i) {
        if (chunks[i].id == 0x80134010) {
            LoadSolid(data, chunks[i].begin, chunks[i].end, solids);
        } else if (chunks[i].id == 0x80134000) {
            WalkSolids(data, chunks[i].begin, chunks[i].end, solids);
        }
    }
}

}  // namespace

unsigned int NativeStringHash(const char *text) {
    unsigned int hash = 0xffffffff;
    for (; *text; ++text) hash = hash * 33 + (unsigned char)*text;
    return hash;
}

bool ReadNativeFile(const std::string &path, std::vector<unsigned char> *data) {
    FILE *file = fopen(path.c_str(), "rb");
    if (!file) return false;
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    data->resize(size + 64);
    size_t read = fread(&(*data)[0], 1, size, file);
    fclose(file);
    data->resize(size);
    return read == (size_t)size;
}

void LoadNativeSolids(const unsigned char *data, size_t size, std::vector<NativeSolid> *solids) {
    WalkSolids(data, 0, size, solids);
}

// Section layout: 0x34102 holds 0x44-byte infos (name[0x20], LOD solid hashes +0x20/+0x24)
// and 0x34103 the 16-aligned instances: bounds +0/+0xc, info index (low half) and flags
// (high half) at +0x18, position +0x20, 3x3 rotation rows as int16 (0x2000 = 1) at +0x2c.
void LoadNativeInstances(const unsigned char *data, size_t size, std::vector<NativeInstance> *instances) {
    std::vector<Chunk> chunks;
    Children(data, 0, size, &chunks);
    for (size_t i = 0; i < chunks.size(); ++i) {
        if (chunks[i].id != 0x80034100) continue;
        std::vector<Chunk> inner;
        Children(data, chunks[i].begin, chunks[i].end, &inner);
        const Chunk *infos = 0, *placements = 0;
        for (size_t j = 0; j < inner.size(); ++j) {
            if (inner[j].id == 0x34102) infos = &inner[j];
            if (inner[j].id == 0x34103) placements = &inner[j];
        }
        if (!infos || !placements) continue;
        size_t infoCount = (infos->end - infos->begin) / 0x44;
        for (size_t p = Align(placements->begin, 16); p + 0x40 <= placements->end; p += 0x40) {
            const unsigned char *record = data + p;
            unsigned int word = Get32(record + 0x18);
            size_t index = word & 0xffff;
            if (index >= infoCount) continue;
            const unsigned char *info = data + infos->begin + index * 0x44;
            NativeInstance instance;
            char name[0x21];
            memcpy(name, info, 0x20);
            name[0x20] = 0;
            instance.infoName = name;
            instance.solidHash = Get32(info + 0x20);
            instance.flags = word >> 16;
            for (int k = 0; k < 3; ++k) {
                instance.minimum[k] = GetFloat(record + 4 * k);
                instance.maximum[k] = GetFloat(record + 0xc + 4 * k);
            }
            for (int r = 0; r < 3; ++r) {
                for (int c = 0; c < 3; ++c) {
                    instance.matrix[r * 4 + c] = (short)Get16(record + 0x2c + 2 * (r * 3 + c)) / 8192.0f;
                }
                instance.matrix[r * 4 + 3] = 0.0f;
                instance.matrix[12 + r] = GetFloat(record + 0x20 + 4 * r);
            }
            instance.matrix[15] = 1.0f;
            instances->push_back(instance);
        }
    }
}

void LoadNativeTextures(const unsigned char *data, size_t size, NativeImageMap *images) {
    std::vector<Chunk> chunks;
    Children(data, 0, size, &chunks);
    for (size_t i = 0; i < chunks.size(); ++i) {
        if (chunks[i].id == 0xb3300000) LoadTexturePack(data, chunks[i].begin, chunks[i].end, images);
    }
}
