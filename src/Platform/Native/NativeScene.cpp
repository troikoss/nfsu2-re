// Platform scaffolding: OpenGL drawing of UG2's own car assets.
#include "NativeScene.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef GL_GENERATE_MIPMAP
#define GL_GENERATE_MIPMAP 0x8191
#endif

void NativeTextureCache::Add(const NativeImageMap &images) {
    for (NativeImageMap::const_iterator it = images.begin(); it != images.end(); ++it) {
        if (!images_.count(it->first)) images_[it->first] = it->second;
    }
}

const NativeImage *NativeTextureCache::Find(unsigned int hash) const {
    NativeImageMap::const_iterator it = images_.find(hash);
    return it == images_.end() ? 0 : &it->second;
}

GLuint NativeTextureCache::Get(unsigned int hash, bool *alpha) {
    std::map<unsigned int, Entry>::iterator found = uploaded_.find(hash);
    if (found != uploaded_.end()) {
        if (alpha) *alpha = found->second.alpha;
        return found->second.id;
    }
    NativeImageMap::iterator it = images_.find(hash);
    Entry entry = {0, false};
    if (it != images_.end()) {
        glGenTextures(1, &entry.id);
        glBindTexture(GL_TEXTURE_2D, entry.id);
        glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, it->second.width, it->second.height, 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, &it->second.rgba[0]);
        entry.alpha = it->second.alpha;
        std::vector<unsigned char>().swap(it->second.rgba);
    }
    uploaded_[hash] = entry;
    if (alpha) *alpha = entry.alpha;
    return entry.id;
}

namespace {

const NativeSolid *FindSolid(const std::vector<NativeSolid> &solids, const std::string &name) {
    unsigned int hash = NativeStringHash(name.c_str());
    for (size_t i = 0; i < solids.size(); ++i) {
        if (solids[i].hash == hash) return &solids[i];
    }
    return 0;
}

}  // namespace

unsigned int NativeCarModel::Resolve(unsigned int hash) const {
    std::map<unsigned int, unsigned int>::const_iterator it = replacements_.find(hash);
    return it == replacements_.end() ? hash : it->second;
}

GLuint NativeCarModel::Build(const NativeSolid &solid, bool translucent) {
    // Upload outside list compilation; texture definitions would otherwise be recorded.
    for (size_t p = 0; p < solid.parts.size(); ++p) {
        unsigned int key = Resolve(solid.parts[p].textureHash);
        if (key) textures_->Get(key, 0);
    }
    GLuint list = glGenLists(1);
    glNewList(list, GL_COMPILE);
    for (size_t p = 0; p < solid.parts.size(); ++p) {
        const NativeMeshPart &part = solid.parts[p];
        unsigned int key = Resolve(part.textureHash);
        bool alpha = false;
        GLuint texture = key ? textures_->Get(key, &alpha) : 0;
        // Body panel (0x3c84d757), skirt (0xa7366ae6) and SKIN1 slots have no texture in any
        // pack; the game fills them from the paint/vinyl layers at run time.
        bool paint = part.textureHash == skinHash_ || part.textureHash == 0x3c84d757 ||
                     part.textureHash == 0xa7366ae6;
        if (alpha != translucent) continue;
        if (!translucent) {
            ++parts_;
            if (texture) ++texturedParts_;
        }
        if (texture) {
            glEnable(GL_TEXTURE_2D);
            glBindTexture(GL_TEXTURE_2D, texture);
            glColor3f(1.0f, 1.0f, 1.0f);
        } else {
            glDisable(GL_TEXTURE_2D);
            if (paint) glColor3fv(paint_);
            else glColor3f(0.12f, 0.12f, 0.13f);
        }
        glBegin(GL_TRIANGLES);
        for (int k = 0; k < part.count; ++k) {
            int v = solid.indices[part.start + k];
            if (!solid.normals.empty()) glNormal3fv(&solid.normals[v * 3]);
            glTexCoord2fv(&solid.uvs[v * 2]);
            glVertex3fv(&solid.positions[v * 3]);
        }
        glEnd();
    }
    glDisable(GL_TEXTURE_2D);
    glEndList();
    return list;
}

bool NativeCarModel::Load(const char *gameDirectory, const char *carName, NativeTextureCache *textures,
                          char *error, int errorSize) {
    textures_ = textures;
    texturedParts_ = parts_ = 0;
    // Scaffold paint: the stock skin layer (%s_SKIN1, built at run time from the car
    // preset's paint) is not recovered, so SKIN1 surfaces use a fixed colour.
    paint_[0] = 0.62f;
    paint_[1] = 0.64f;
    paint_[2] = 0.68f;
    std::string game(gameDirectory), car(carName);
    skinHash_ = NativeStringHash((car + "_SKIN1").c_str());
    std::vector<unsigned char> geometry;
    if (!ReadNativeFile(game + "/CARS/" + car + "/GEOMETRY.BIN", &geometry)) {
        snprintf(error, errorSize, "cannot read %s geometry", carName);
        return false;
    }
    LoadNativeSolids(&geometry[0], geometry.size(), &solids_);
    const char *packs[] = {"/CARS/%s/TEXTURES.BIN", "/CARS/TEXTURES.BIN", "/CARS/BRAKES/TEXTURES.BIN",
                           "/GLOBAL/GLOBALB.BUN"};
    for (int i = 0; i < 4; ++i) {
        char path[256];
        snprintf(path, sizeof(path), packs[i], carName);
        std::vector<unsigned char> data;
        if (!ReadNativeFile(game + path, &data)) continue;
        NativeImageMap images;
        LoadNativeTextures(&data[0], data.size(), &images);
        textures->Add(images);
    }
    // Replacement keys from the CarRenderInfo constructor 0x638190 and the used-texture
    // setup 0x631e20, with lights off and no damage. Which damage/variant texture the
    // game finally binds to the window keys is not recovered; WINDOW is used.
    unsigned int window = NativeStringHash("WINDOW");
    const char *windows[] = {"WINDOW_FRONT", "WINDOW_REAR", "WINDOW_LEFT_FRONT", "WINDOW_RIGHT_FRONT",
                             "WINDOW_LEFT_REAR", "WINDOW_RIGHT_REAR"};
    for (int i = 0; i < 6; ++i) replacements_[NativeStringHash(windows[i])] = window;
    replacements_[NativeStringHash("DRIVER")] = NativeStringHash("DRIVER_MALE");
    std::string brake = car + "_KIT00_BRAKELIGHT_OFF";
    replacements_[NativeStringHash("BRAKELIGHT_LEFT")] = NativeStringHash(brake.c_str());
    replacements_[NativeStringHash("BRAKELIGHT_RIGHT")] = NativeStringHash(brake.c_str());
    replacements_[NativeStringHash("BRAKELIGHT_CENTRE")] =
        NativeStringHash((car + "_CENTRE_BRAKELIGHT_OFF").c_str());
    std::string headlight = car + "_KIT00_HEADLIGHT_OFF";
    replacements_[NativeStringHash("HEADLIGHT_LEFT")] = NativeStringHash(headlight.c_str());
    replacements_[NativeStringHash("HEADLIGHT_RIGHT")] = NativeStringHash(headlight.c_str());

    // Stock configuration: BASE plus the KIT00 parts of LOD A. Stored names are cut to 27
    // characters, so parts are matched by the full-name hash at header +0x10. The door
    // panel/sill and trunk audio variants are left out.
    const char *parts[] = {"BODY", "BRAKELIGHT", "DOOR_LEFT", "DOOR_RIGHT", "ENGINE", "EXHAUST", "FRONT_BUMPER",
                           "HEADLIGHT_LEFT", "HEADLIGHT_RIGHT", "HOOD", "HOOD_UNDER", "LEFT_SIDE_MIRROR",
                           "RIGHT_SIDE_MIRROR", "REAR_BUMPER", "ROOF", "SKIRT", "SPOILER", "TRUNK", "TRUNK_UNDER"};
    std::string prefix = car + "_KIT00_";
    std::vector<std::string> wanted(1, car + "_BASE_A");
    for (size_t i = 0; i < sizeof(parts) / sizeof(parts[0]); ++i) wanted.push_back(prefix + parts[i] + "_A");
    for (size_t w = 0; w < wanted.size(); ++w) {
        const NativeSolid *solid = FindSolid(solids_, wanted[w]);
        if (!solid) continue;
        NativeDrawSolid draw = {solid, Build(*solid, false), Build(*solid, true)};
        body_.push_back(draw);
        if (getenv("UG2_DEBUG_PARTS")) {
            for (size_t p = 0; p < solid->parts.size(); ++p) {
                unsigned int key = Resolve(solid->parts[p].textureHash);
                const NativeImage *image = textures->Find(key);
                fprintf(stderr, "%s part %d slot %08x -> %08x %s (%d idx, flags %x)\n", wanted[w].c_str(), (int)p,
                        solid->parts[p].textureHash, key, image ? image->name.c_str() : "-",
                        solid->parts[p].count, solid->parts[p].flags);
            }
        }
    }
    const NativeSolid *wheel = FindSolid(solids_, prefix + "FRONT_WHEEL_A");
    const NativeSolid *front = FindSolid(solids_, prefix + "FRONT_BRAKE_A");
    const NativeSolid *rear = FindSolid(solids_, prefix + "REAR_BRAKE_A");
    if (body_.empty() || !wheel) {
        snprintf(error, errorSize, "%s has no stock KIT00 solids", carName);
        return false;
    }
    wheel_.solid = wheel;
    wheel_.opaqueList = Build(*wheel, false);
    wheel_.translucentList = Build(*wheel, true);
    frontBrake_.solid = front;
    rearBrake_.solid = rear;
    frontBrake_.opaqueList = front ? Build(*front, false) : 0;
    frontBrake_.translucentList = front ? Build(*front, true) : 0;
    rearBrake_.opaqueList = rear ? Build(*rear, false) : 0;
    rearBrake_.translucentList = rear ? Build(*rear, true) : 0;
    return true;
}

void NativeCarModel::Draw(const float *body, const float wheels[4][16], const float spins[4][16],
                          const bool mirrored[4], bool translucent) {
    glPushMatrix();
    glMultMatrixf(body);
    for (size_t i = 0; i < body_.size(); ++i) glCallList(translucent ? body_[i].translucentList : body_[i].opaqueList);
    glPopMatrix();
    for (int w = 0; w < 4; ++w) {
        const NativeDrawSolid &brake = w < 2 ? frontBrake_ : rearBrake_;
        GLuint lists[2] = {translucent ? wheel_.translucentList : wheel_.opaqueList,
                           translucent ? brake.translucentList : brake.opaqueList};
        const float *frames[2] = {spins[w], wheels[w]};
        for (int k = 0; k < 2; ++k) {
            if (!lists[k]) continue;
            glPushMatrix();
            glMultMatrixf(frames[k]);
            if (mirrored[w]) {
                glScalef(1.0f, -1.0f, 1.0f);
                glFrontFace(GL_CW);
            }
            glCallList(lists[k]);
            glFrontFace(GL_CCW);
            glPopMatrix();
        }
    }
}

GLuint NativeTrackModel::Build(const NativeSolid &solid) {
    for (size_t p = 0; p < solid.parts.size(); ++p) textures_->Get(solid.parts[p].textureHash, 0);
    GLuint list = glGenLists(1);
    glNewList(list, GL_COMPILE);
    for (size_t p = 0; p < solid.parts.size(); ++p) {
        const NativeMeshPart &part = solid.parts[p];
        GLuint texture = textures_->Get(part.textureHash, 0);
        if (texture) {
            glEnable(GL_TEXTURE_2D);
            glBindTexture(GL_TEXTURE_2D, texture);
        } else {
            glDisable(GL_TEXTURE_2D);
        }
        glBegin(GL_TRIANGLES);
        for (int k = 0; k < part.count; ++k) {
            int v = solid.indices[part.start + k];
            // Vertex colours are D3D ARGB words holding baked lighting.
            unsigned int colour = solid.colours[v];
            glColor4ub((GLubyte)(colour >> 16), (GLubyte)(colour >> 8), (GLubyte)colour, (GLubyte)(colour >> 24));
            glTexCoord2fv(&solid.uvs[v * 2]);
            glVertex3fv(&solid.positions[v * 3]);
        }
        glEnd();
    }
    glDisable(GL_TEXTURE_2D);
    glEndList();
    return list;
}

bool NativeTrackModel::Load(const char *gameDirectory, const char *region, NativeTextureCache *textures,
                            char *error, int errorSize) {
    textures_ = textures;
    std::string game(gameDirectory);
    const char *files[] = {"/TRACKS/%s.BUN", "/TRACKS/STREAM%s.BUN"};
    for (int i = 0; i < 2; ++i) {
        char path[256];
        snprintf(path, sizeof(path), files[i], region);
        std::vector<unsigned char> data;
        if (!ReadNativeFile(game + path, &data)) {
            snprintf(error, errorSize, "cannot read %s", path);
            return false;
        }
        LoadNativeSolids(&data[0], data.size(), &solids_);
        LoadNativeInstances(&data[0], data.size(), &instances_);
        NativeImageMap images;
        LoadNativeTextures(&data[0], data.size(), &images);
        textures->Add(images);
    }
    std::map<unsigned int, GLuint> lists;
    std::map<unsigned int, const NativeSolid *> byHash;
    for (size_t i = 0; i < solids_.size(); ++i) byHash[solids_[i].hash] = &solids_[i];
    for (size_t i = 0; i < instances_.size(); ++i) {
        const NativeInstance &instance = instances_[i];
        std::map<unsigned int, const NativeSolid *>::iterator solid = byHash.find(instance.solidHash);
        if (solid == byHash.end()) continue;
        GLuint &list = lists[instance.solidHash];
        if (!list) list = Build(*solid->second);
        Placed placed;
        placed.list = list;
        placed.radius = 0.0f;
        for (int k = 0; k < 3; ++k) {
            placed.centre[k] = (instance.minimum[k] + instance.maximum[k]) * 0.5f;
            float half = (instance.maximum[k] - instance.minimum[k]) * 0.5f;
            placed.radius += half * half;
        }
        placed.radius = sqrtf(placed.radius);
        placed.matrix = instance.matrix;
        placed.sky = instance.infoName.compare(0, 7, "SKYDOME") == 0;
        placed_.push_back(placed);
    }
    if (placed_.empty()) {
        snprintf(error, errorSize, "no scenery instances in %s", region);
        return false;
    }
    return true;
}

void NativeTrackModel::Draw(const float *eye, const float *forward, float range) {
    // Sky first, centred on the viewer, without depth.
    glDepthMask(GL_FALSE);
    glDisable(GL_DEPTH_TEST);
    for (size_t i = 0; i < placed_.size(); ++i) {
        if (!placed_[i].sky) continue;
        float m[16];
        memcpy(m, placed_[i].matrix, sizeof(m));
        m[12] = eye[0];
        m[13] = eye[1];
        m[14] = eye[2] - 60.0f;
        glPushMatrix();
        glMultMatrixf(m);
        glCallList(placed_[i].list);
        glPopMatrix();
    }
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glEnable(GL_ALPHA_TEST);
    glAlphaFunc(GL_GREATER, 0.5f);
    for (size_t i = 0; i < placed_.size(); ++i) {
        const Placed &placed = placed_[i];
        if (placed.sky) continue;
        float dx = placed.centre[0] - eye[0], dy = placed.centre[1] - eye[1], dz = placed.centre[2] - eye[2];
        float limit = range + placed.radius;
        if (dx * dx + dy * dy + dz * dz > limit * limit) continue;
        if (dx * forward[0] + dy * forward[1] + dz * forward[2] < -placed.radius) continue;
        glPushMatrix();
        glMultMatrixf(placed.matrix);
        glCallList(placed.list);
        glPopMatrix();
    }
    glDisable(GL_ALPHA_TEST);
}
