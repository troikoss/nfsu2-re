#ifndef UG2_NATIVE_SCENE_H
#define UG2_NATIVE_SCENE_H

// Platform scaffolding: OpenGL drawing of UG2's own car and track assets. The original
// render path (CarRenderInfo 0x638190, eSolid rendering 0x5cf420) is not recovered.
#include "NativeAssets.h"

#include <GL/gl.h>
#include <map>
#include <string>
#include <vector>

class NativeTextureCache {
public:
    void Add(const NativeImageMap &images);
    // Returns 0 when the hash is not loaded.
    GLuint Get(unsigned int hash, bool *alpha);
    const NativeImage *Find(unsigned int hash) const;
    size_t Count() const { return images_.size(); }

private:
    struct Entry {
        GLuint id;
        bool alpha;
    };
    NativeImageMap images_;
    std::map<unsigned int, Entry> uploaded_;
};

struct NativeDrawSolid {
    const NativeSolid *solid;
    GLuint opaqueList;
    GLuint translucentList;
};

class NativeCarModel {
public:
    bool Load(const char *gameDirectory, const char *carName, NativeTextureCache *textures, char *error,
              int errorSize);
    // `body` is the column-major render matrix of the car origin; `wheels` the four
    // column-major wheel frames, `spins` their spin-including frames.
    void Draw(const float *body, const float wheels[4][16], const float spins[4][16], const bool mirrored[4],
              bool translucent);
    int SolidCount() const { return (int)solids_.size(); }
    int TexturedParts() const { return texturedParts_; }
    int Parts() const { return parts_; }

private:
    GLuint Build(const NativeSolid &solid, bool translucent);
    unsigned int Resolve(unsigned int hash) const;
    std::vector<NativeSolid> solids_;
    std::vector<NativeDrawSolid> body_;
    NativeDrawSolid wheel_;
    NativeDrawSolid frontBrake_;
    NativeDrawSolid rearBrake_;
    std::map<unsigned int, unsigned int> replacements_;
    NativeTextureCache *textures_;
    float paint_[3];
    unsigned int skinHash_;
    int texturedParts_;
    int parts_;
};

class NativeTrackModel {
public:
    bool Load(const char *gameDirectory, const char *region, NativeTextureCache *textures, char *error,
              int errorSize);
    void Draw(const float *eye, const float *forward, float range);
    int InstanceCount() const { return (int)instances_.size(); }
    int SolidCount() const { return (int)solids_.size(); }

private:
    struct Placed {
        GLuint list;
        float centre[3];
        float radius;
        const float *matrix;
        bool sky;
    };
    GLuint Build(const NativeSolid &solid);
    std::vector<NativeSolid> solids_;
    std::vector<NativeInstance> instances_;
    std::vector<Placed> placed_;
    NativeTextureCache *textures_;
};

#endif
