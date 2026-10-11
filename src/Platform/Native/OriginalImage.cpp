#include "OriginalImage.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <vector>

namespace {

const uint32_t kImageBase = 0x400000;
const uint32_t kMapStart = 0x783000;
const uint32_t kMapEnd = 0x8bc000;
const char kExpectedSha256[] = "f9dd86c054878ce6276beb07c1fd61874f7a1e4bf1f241b084c65b73e24168a7";

struct Sha256 {
    uint32_t state[8];
    uint64_t length;
    uint8_t block[64];
    size_t used;
};

uint32_t Rotate(uint32_t value, int count) {
    return (value >> count) | (value << (32 - count));
}

void Sha256Block(Sha256 *sha, const uint8_t *data) {
    static const uint32_t k[64] = {
        0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
        0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
        0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
        0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
        0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
        0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
        0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
        0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};
    uint32_t w[64];
    for (int i = 0; i < 16; ++i) {
        w[i] = (uint32_t)data[i * 4] << 24 | (uint32_t)data[i * 4 + 1] << 16 |
               (uint32_t)data[i * 4 + 2] << 8 | data[i * 4 + 3];
    }
    for (int i = 16; i < 64; ++i) {
        uint32_t s0 = Rotate(w[i - 15], 7) ^ Rotate(w[i - 15], 18) ^ (w[i - 15] >> 3);
        uint32_t s1 = Rotate(w[i - 2], 17) ^ Rotate(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    uint32_t a = sha->state[0], b = sha->state[1], c = sha->state[2], d = sha->state[3];
    uint32_t e = sha->state[4], f = sha->state[5], g = sha->state[6], h = sha->state[7];
    for (int i = 0; i < 64; ++i) {
        uint32_t t1 = h + (Rotate(e, 6) ^ Rotate(e, 11) ^ Rotate(e, 25)) + ((e & f) ^ (~e & g)) + k[i] + w[i];
        uint32_t t2 = (Rotate(a, 2) ^ Rotate(a, 13) ^ Rotate(a, 22)) + ((a & b) ^ (a & c) ^ (b & c));
        h = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
    }
    sha->state[0] += a; sha->state[1] += b; sha->state[2] += c; sha->state[3] += d;
    sha->state[4] += e; sha->state[5] += f; sha->state[6] += g; sha->state[7] += h;
}

void Sha256Hex(const uint8_t *data, size_t size, char *output) {
    Sha256 sha = {{0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab,
                   0x5be0cd19}, 0, {0}, 0};
    size_t offset = 0;
    for (; offset + 64 <= size; offset += 64) {
        Sha256Block(&sha, data + offset);
    }
    uint8_t tail[128] = {0};
    size_t rest = size - offset;
    memcpy(tail, data + offset, rest);
    tail[rest] = 0x80;
    size_t blocks = rest + 9 > 64 ? 2 : 1;
    uint64_t bits = (uint64_t)size * 8;
    for (int i = 0; i < 8; ++i) {
        tail[blocks * 64 - 1 - i] = (uint8_t)(bits >> (i * 8));
    }
    for (size_t i = 0; i < blocks; ++i) {
        Sha256Block(&sha, tail + i * 64);
    }
    for (int i = 0; i < 8; ++i) {
        sprintf(output + i * 8, "%08x", sha.state[i]);
    }
}

uint32_t Read32(const std::vector<uint8_t> &file, size_t offset) {
    return file[offset] | file[offset + 1] << 8 | file[offset + 2] << 16 | (uint32_t)file[offset + 3] << 24;
}

}  // namespace

bool MapOriginalImage(const char *path, char *error, int errorSize) {
    FILE *input = fopen(path, "rb");
    if (!input) {
        snprintf(error, errorSize, "cannot open %s", path);
        return false;
    }
    std::vector<uint8_t> file;
    uint8_t buffer[65536];
    size_t count;
    while ((count = fread(buffer, 1, sizeof(buffer), input)) > 0) {
        file.insert(file.end(), buffer, buffer + count);
    }
    fclose(input);
    char digest[65];
    Sha256Hex(file.data(), file.size(), digest);
    if (strcmp(digest, kExpectedSha256) != 0) {
        snprintf(error, errorSize, "%s is not UG2 PC v1.2 speed2.exe (SHA-256 %s)", path, digest);
        return false;
    }
    void *mapped = mmap((void *)(uintptr_t)kMapStart, kMapEnd - kMapStart, PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
    if (mapped != (void *)(uintptr_t)kMapStart) {
        snprintf(error, errorSize, "cannot reserve original data range 0x%x-0x%x", kMapStart, kMapEnd);
        return false;
    }
    uint32_t header = Read32(file, 0x3c);
    int sections = file[header + 6] | file[header + 7] << 8;
    int optional = file[header + 20] | file[header + 21] << 8;
    size_t table = header + 24 + optional;
    for (int i = 0; i < sections; ++i) {
        size_t entry = table + i * 40;
        uint32_t virtualAddress = kImageBase + Read32(file, entry + 12);
        uint32_t rawSize = Read32(file, entry + 16);
        uint32_t rawPointer = Read32(file, entry + 20);
        uint32_t virtualSize = Read32(file, entry + 8);
        if (virtualAddress < kMapStart || virtualAddress >= kMapEnd) {
            continue;
        }
        uint32_t size = rawSize < virtualSize ? rawSize : virtualSize;
        if (virtualAddress + size > kMapEnd) {
            size = kMapEnd - virtualAddress;
        }
        memcpy((void *)(uintptr_t)virtualAddress, file.data() + rawPointer, size);
    }
    return true;
}
