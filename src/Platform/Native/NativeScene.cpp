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

GLuint NativeTrackModel::Build(const NativeSolid &solid, int pass) {
    // Scaffold render states from the texture's alpha fields (the D3D renderer is not
    // recovered): blend type 0 draws opaque, alpha-tested only for punch-through usage, so
    // the alpha of modulated road and terrain textures is not treated as coverage; blend
    // type 1 is alpha blended and 2 additive, both after the opaque pass.
    bool any = false;
    for (size_t p = 0; p < solid.parts.size(); ++p) {
        textures_->Get(solid.parts[p].textureHash, 0);
        const NativeImage *image = textures_->Find(solid.parts[p].textureHash);
        int blend = image && image->alpha ? image->alphaBlend : 0;
        if ((blend > 2 ? 1 : blend) == pass) any = true;
    }
    if (!any) return 0;
    GLuint list = glGenLists(1);
    glNewList(list, GL_COMPILE);
    for (size_t p = 0; p < solid.parts.size(); ++p) {
        const NativeMeshPart &part = solid.parts[p];
        const NativeImage *image = textures_->Find(part.textureHash);
        int blend = image && image->alpha ? image->alphaBlend : 0;
        if ((blend > 2 ? 1 : blend) != pass) continue;
        GLuint texture = textures_->Get(part.textureHash, 0);
        if (texture) {
            glEnable(GL_TEXTURE_2D);
            glBindTexture(GL_TEXTURE_2D, texture);
        } else {
            glDisable(GL_TEXTURE_2D);
        }
        if (pass == 0 && image && image->alpha && image->alphaUsage == 1) glEnable(GL_ALPHA_TEST);
        else glDisable(GL_ALPHA_TEST);
        glBegin(GL_TRIANGLES);
        for (int k = 0; k < part.count; ++k) {
            int v = solid.indices[part.start + k];
            // Vertex colours are D3D ARGB words holding baked lighting; their alpha only
            // enters the blended passes.
            unsigned int colour = solid.colours[v];
            glColor4ub((GLubyte)(colour >> 16), (GLubyte)(colour >> 8), (GLubyte)colour,
                       pass == 0 ? 255 : (GLubyte)(colour >> 24));
            glTexCoord2fv(&solid.uvs[v * 2]);
            glVertex3fv(&solid.positions[v * 3]);
        }
        glEnd();
    }
    glDisable(GL_ALPHA_TEST);
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
        LoadNativeDrivableSections(&data[0], data.size(), &drivable_);
        NativeImageMap images;
        LoadNativeTextures(&data[0], data.size(), &images);
        textures->Add(images);
    }
    // Shared packs the scenery draws from: location dynamic textures ("TRACKS\LOC%dDYNTEX.BIN",
    // SKYDOME), the in-game common pack (SFX_LIGHT_BEAMA), GLOBALB (neon signs) and the
    // regional global textures (billboards; the NA variant of "GLOBAL\GLOBALTEXTURESNA.BIN").
    const char *shared[] = {"/TRACKS/LOC4DYNTEX.BIN", "/GLOBAL/InGameCommon.bun", "/GLOBAL/GLOBALB.BUN",
                            "/GLOBAL/GlobalTexturesNA.bin"};
    for (int i = 0; i < 4; ++i) {
        std::vector<unsigned char> data;
        if (!ReadNativeFile(game + shared[i], &data)) continue;
        NativeImageMap images;
        LoadNativeTextures(&data[0], data.size(), &images);
        textures->Add(images);
    }
    std::map<unsigned int, std::vector<GLuint> > lists;
    std::map<unsigned int, const NativeSolid *> byHash;
    for (size_t i = 0; i < solids_.size(); ++i) byHash[solids_[i].hash] = &solids_[i];
    // View flags as the view setup 0x4888a0 builds them for the player view: 0x10 always,
    // 0x08 while 0x89e7a4 (race direction) is 0. Bits 0x02/0x01 come from eView +4 and
    // 0x8707dc, which are not recovered, and are left clear. The instance test is the one
    // in 0x5d6570: (instance +0x1a ^ 0x60) & (byte)view +0x44 skips the instance.
    const unsigned int viewFlags = 0x10 | 0x08;
    for (size_t i = 0; i < instances_.size(); ++i) {
        const NativeInstance &instance = instances_[i];
        if (((instance.flags ^ 0x60) & viewFlags & 0xff) != 0) continue;
        std::map<unsigned int, const NativeSolid *>::iterator solid = byHash.find(instance.solidHash);
        if (solid == byHash.end()) continue;
        std::vector<GLuint> &list = lists[instance.solidHash];
        if (list.empty()) {
            for (int pass = 0; pass < 3; ++pass) list.push_back(Build(*solid->second, pass));
        }
        Placed placed;
        for (int pass = 0; pass < 3; ++pass) placed.lists[pass] = list[pass];
        placed.radius = 0.0f;
        for (int k = 0; k < 3; ++k) {
            placed.centre[k] = (instance.minimum[k] + instance.maximum[k]) * 0.5f;
            float half = (instance.maximum[k] - instance.minimum[k]) * 0.5f;
            placed.radius += half * half;
        }
        placed.radius = sqrtf(placed.radius);
        // Instance flag 0x100 (0x5d6570, +0x1b & 1) queues the model with no matrix: the
        // solid's vertices are already in world space.
        static const float identity[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
        placed.matrix = (instance.flags & 0x100) ? identity : instance.matrix;
        placed.sky = instance.infoName.compare(0, 7, "SKYDOME") == 0;
        placed.instance = (int)i;
        placed.section = instance.section;
        placed_.push_back(placed);
    }
    if (placed_.empty()) {
        snprintf(error, errorSize, "no scenery instances in %s", region);
        return false;
    }
    currentSection_ = -2;
    return true;
}

namespace {

// Even-odd crossing test over the boundary points, edge by edge from the last point, as in
// 0x5d6dd0 (after the 0x43d410 bounds test with zero margin).
bool InsideBoundary(const NativeDrivableSection &section, float x, float y) {
    if (x < section.minimum[0] || x > section.maximum[0] || y < section.minimum[1] || y > section.maximum[1])
        return false;
    size_t count = section.points.size() / 2;
    if (count < 4) return false;
    bool inside = false;
    for (size_t i = 0, j = count - 1; i < count; j = i++) {
        const float *a = &section.points[i * 2], *b = &section.points[j * 2];
        if ((a[1] <= y && y < b[1]) || (b[1] <= y && y < a[1])) {
            if ((y - a[1]) * (b[0] - a[0]) / (b[1] - a[1]) + a[0] > x) inside = !inside;
        }
    }
    return inside;
}

}  // namespace

void NativeTrackModel::SelectSections(const float *eye) {
    // Visible-section choice of 0x5e96b0 for the player view: the drivable section whose
    // boundary holds the eye (0x5e00d0) gives its list; the Z sections 2600-2699 are always
    // drawn. Without a drivable section the game draws the loaded sections numbered below
    // 10 in their letter. The nearest-boundary fallback 0x5db770 is not recovered.
    int found = -1;
    for (size_t i = 0; i < drivable_.size() && found < 0; ++i) {
        if (InsideBoundary(drivable_[i], eye[0], eye[1])) found = (int)i;
    }
    int section = found < 0 ? -1 : drivable_[found].section;
    if (section == currentSection_) return;
    currentSection_ = section;
    visible_.assign(2700, false);
    for (int z = 2600; z < 2700; ++z) visible_[z] = true;
    if (found >= 0) {
        const std::vector<short> &list = drivable_[found].visible;
        for (size_t i = 0; i < list.size(); ++i) {
            if (list[i] >= 0 && list[i] < 2700) visible_[list[i]] = true;
        }
    } else {
        for (int n = 0; n < 2600; ++n) visible_[n] = n % 100 < 10;
    }
    if (getenv("UG2_DEBUG_SECTIONS")) {
        fprintf(stderr, "ug2demo: eye %.1f %.1f in section %d (%c%d)\n", eye[0], eye[1], section,
                section < 0 ? '-' : section / 100 + 0x40, section < 0 ? 0 : section % 100);
        for (size_t i = 0; found >= 0 && i < drivable_[found].visible.size(); ++i)
            fprintf(stderr, " %d", drivable_[found].visible[i]);
        fprintf(stderr, "\n");
    }
}

void NativeTrackModel::Draw(const float *eye, const float *forward, float range) {
    SelectSections(eye);
    // Sky first, centred on the viewer, without depth. The dome is ~19 km wide, beyond the
    // demo's far plane, so it is drawn scaled down about the eye.
    const float skyScale = 0.1f;
    glDepthMask(GL_FALSE);
    glDisable(GL_DEPTH_TEST);
    for (size_t i = 0; i < placed_.size(); ++i) {
        if (!placed_[i].sky) continue;
        float m[16];
        memcpy(m, placed_[i].matrix, sizeof(m));
        for (int k = 0; k < 12; ++k) m[k] *= skyScale;
        m[12] = eye[0];
        m[13] = eye[1];
        m[14] = eye[2] - 60.0f * skyScale;
        glPushMatrix();
        glMultMatrixf(m);
        for (int pass = 0; pass < 3; ++pass) {
            if (placed_[i].lists[pass]) glCallList(placed_[i].lists[pass]);
        }
        glPopMatrix();
    }
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glAlphaFunc(GL_GREATER, 0.5f);
    DrawPlaced(eye, forward, range, -1, 0);
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    DrawPlaced(eye, forward, range, -1, 1);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    DrawPlaced(eye, forward, range, -1, 2);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
}

void NativeTrackModel::DrawPlaced(const float *eye, const float *forward, float range, int pickShift, int pass) {
    for (size_t i = 0; i < placed_.size(); ++i) {
        const Placed &placed = placed_[i];
        if (!placed.lists[pass]) continue;
        if (placed.sky || placed.section < 0 || placed.section >= (int)visible_.size() || !visible_[placed.section])
            continue;
        float dx = placed.centre[0] - eye[0], dy = placed.centre[1] - eye[1], dz = placed.centre[2] - eye[2];
        float limit = range + placed.radius;
        if (dx * dx + dy * dy + dz * dz > limit * limit) continue;
        if (dx * forward[0] + dy * forward[1] + dz * forward[2] < -placed.radius) continue;
        if (pickShift >= 0) glStencilFunc(GL_ALWAYS, ((int)i + 1) >> pickShift & 0xff, 0xff);
        glPushMatrix();
        glMultMatrixf(placed.matrix);
        glCallList(placed.lists[pass]);
        glPopMatrix();
    }
}

void NativeTrackModel::DebugPick(const float *eye, const float *forward, float range, int x, int y) {
    // One pass per byte of (placed index + 1); depth is cleared each pass so the same
    // instance wins every time.
    int picked = 0;
    glEnable(GL_STENCIL_TEST);
    glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
    for (int shift = 0; shift < 24; shift += 8) {
        glClear(GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
        DrawPlaced(eye, forward, range, shift, 0);
        GLubyte value = 0;
        glReadPixels(x, y, 1, 1, GL_STENCIL_INDEX, GL_UNSIGNED_BYTE, &value);
        picked |= value << shift;
    }
    glDisable(GL_STENCIL_TEST);
    if (!picked) {
        fprintf(stderr, "ug2demo: no scenery at pixel %d %d\n", x, y);
        return;
    }
    const Placed &placed = placed_[picked - 1];
    fprintf(stderr, "ug2demo: pixel %d %d shows\n", x, y);
    PrintInstance(placed.instance);
}

void NativeTrackModel::PrintInstance(size_t i) const {
    const NativeInstance &instance = instances_[i];
    const NativeSolid *solid = 0;
    for (size_t k = 0; k < solids_.size() && !solid; ++k) {
        if (solids_[k].hash == instance.solidHash) solid = &solids_[k];
    }
    fprintf(stderr, "%6d %-32s sec %4d flags %04x min %.1f %.1f %.1f max %.1f %.1f %.1f solid %s\n", (int)i,
            instance.infoName.c_str(), instance.section, instance.flags, instance.minimum[0], instance.minimum[1], instance.minimum[2],
            instance.maximum[0], instance.maximum[1], instance.maximum[2], solid ? solid->name.c_str() : "(missing)");
    if (!solid) return;
    for (size_t p = 0; p < solid->parts.size(); ++p) {
        const NativeMeshPart &part = solid->parts[p];
        const NativeImage *image = textures_->Find(part.textureHash);
        fprintf(stderr, "         part %d tris %d flags %08x texture %08x %s %dx%d\n", (int)p, part.count / 3, part.flags,
                part.textureHash, image ? image->name.c_str() : "(missing)", image ? image->width : 0,
                image ? image->height : 0);
    }
}

void NativeTrackModel::DebugAt(float x, float y) const {
    for (size_t i = 0; i < instances_.size(); ++i) {
        const NativeInstance &instance = instances_[i];
        if (x < instance.minimum[0] || x > instance.maximum[0] || y < instance.minimum[1] || y > instance.maximum[1])
            continue;
        PrintInstance(i);
    }
}
