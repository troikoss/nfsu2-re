#include "NativeFng.h"
#include "NativeAssets.h"
#include "../../Speed/Indep/Src/Hud/HudAnimation.h"

#include <algorithm>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void NativeTrap(const char *what);

#define IMSTB_TRUETYPE_IMPLEMENTATION
#define STB_TRUETYPE_IMPLEMENTATION
#include "third_party/imstb_truetype.h"

namespace {

unsigned int Rd32(const unsigned char *p) { return p[0] | p[1] << 8 | p[2] << 16 | (unsigned int)p[3] << 24; }
unsigned int Rd16(const unsigned char *p) { return p[0] | p[1] << 8; }
float RdF(const unsigned char *p) {
    unsigned int v = Rd32(p);
    float f;
    memcpy(&f, &v, 4);
    return f;
}

const unsigned int kInitScript = 0x1744b3;  // 0x5515e0 / 0x5554a0: the "Init" script hash

// Retained retail tracks use the -1 key as a baseline, and later keys as
// offsets (0x52bdc0). This bounded platform adapter supports message tracks
// of types 4/6, noncycling interpolation 1, and constant tracks at time zero.
// Other animation forms remain explicit seams.
void ApplyTrack(const FengTrack &track, unsigned char *state, int stateBytes, int time) {
    if (track.keys.empty() || track.offset < 0 || track.offset * 4 + track.size > stateBytes) return;
    unsigned char *out = state + track.offset * 4;
    const unsigned char *base = &track.keys[0].second[0];
    if (track.keys.size() == 1) { memcpy(out, base, track.size); return; }
    if (track.interpolation != 1 || track.cycle != 0 || (track.type != 4 && track.type != 6)) {
        // The loader can still use a constant first key for unrecovered tracks.
        if (time == 0) { memcpy(out, base, track.size); return; }
        NativeTrap("FEng message track interpolation/cycle");
    }
    size_t upper = 1;
    while (upper + 1 < track.keys.size() && track.keys[upper].first < time) ++upper;
    size_t lower = upper > 1 ? upper - 1 : upper;
    float fraction = 1.0f;
    if (track.keys[upper].first > time) {
        if (upper == 1) fraction = 0.0f;
        else fraction = (float)(time - track.keys[lower].first) /
                        (float)(track.keys[upper].first - track.keys[lower].first);
    }
    const unsigned char *a = &track.keys[lower].second[0], *b = &track.keys[upper].second[0];
    if (track.type == 4)
        HudLerpVector3(reinterpret_cast<const float *>(a), reinterpret_cast<const float *>(b), fraction,
                       reinterpret_cast<const float *>(base), reinterpret_cast<float *>(out));
    else HudLerpColour(reinterpret_cast<const int *>(a), reinterpret_cast<const int *>(b), fraction,
                       reinterpret_cast<const int *>(base), reinterpret_cast<int *>(out));
}

void ReadState(FengObject *object, const unsigned char *state, int size) {
    if (size < 68) return;
    for (int i = 0; i < 4; ++i) object->colour[i] = Rd32(state + i * 4);
    for (int i = 0; i < 3; ++i) {
        object->pivot[i] = RdF(state + 16 + i * 4);
        object->position[i] = RdF(state + 28 + i * 4);
        object->size[i] = RdF(state + 56 + i * 4);
    }
    for (int i = 0; i < 4; ++i) object->rotation[i] = RdF(state + 40 + i * 4);
    if (size >= 84) for (int i = 0; i < 4; ++i) object->uv[i] = RdF(state + 68 + i * 4);
    object->angle = 2.0f * atan2f(object->rotation[2], object->rotation[3]) * 180.0f / 3.14159265f;
}

}  // namespace

unsigned int FengHash(const char *text) {
    unsigned int hash = 0xffffffff;
    for (; *text; ++text) {
        unsigned char c = (unsigned char)*text;
        if (c >= 'a' && c <= 'z') c -= 0x20;
        hash = hash * 33 + c;
    }
    return hash;
}

FengPackage::FengPackage() {}

FengPackage::~FengPackage() {
    for (size_t i = 0; i < objects_.size(); ++i) delete objects_[i];
}

bool FengPackage::Load(const unsigned char *data, size_t size, size_t offset, std::string *error) {
    if (offset + 16 > size || Rd32(data + offset) != 0x30203) {
        *error = "not an FNG chunk";
        return false;
    }
    size_t end = offset + 8 + Rd32(data + offset + 4);
    std::vector<unsigned int> resourceHashes;
    // Package: 'FEn\xe7' holds PkHd, TypS, Res\xcc (RsNm, RsRq) and Obj\xcc (Butn, FOb\xea...).
    size_t p = offset + 8;
    while (p + 8 <= end) {
        unsigned int id = Rd32(data + p);
        size_t chunkEnd = p + 8 + Rd32(data + p + 4);
        if (id == 0xe76e4546) {
            std::vector<unsigned char> names;
            std::vector<unsigned char> requests;
            size_t q = p + 8;
            while (q + 8 <= chunkEnd) {
                unsigned int id2 = Rd32(data + q);
                size_t end2 = q + 8 + Rd32(data + q + 4);
                if (id2 == 0xcc736552) {
                    size_t r = q + 8;
                    while (r + 8 <= end2) {
                        unsigned int id3 = Rd32(data + r);
                        size_t end3 = r + 8 + Rd32(data + r + 4);
                        if (id3 == 0x6d4e7352) names.assign(data + r + 8, data + end3);
                        if (id3 == 0x71527352) requests.assign(data + r + 8, data + end3);
                        r = end3;
                    }
                    if (requests.size() >= 4) {
                        unsigned int count = Rd32(&requests[0]);
                        for (unsigned int i = 0; i < count && 4 + (i + 1) * 24 <= requests.size(); ++i) {
                            unsigned int nameOffset = Rd32(&requests[4 + i * 24 + 4]);
                            std::string name;
                            if (nameOffset < names.size()) name = (const char *)&names[nameOffset];
                            std::string::size_type dot = name.rfind('.');
                            if (dot != std::string::npos) name.resize(dot);
                            resourceHashes.push_back(FengHash(name.c_str()));
                        }
                    }
                } else if (id2 == 0xcc6a624f) {
                    size_t r = q + 8;
                    while (r + 8 <= end2) {
                        unsigned int id3 = Rd32(data + r);
                        size_t end3 = r + 8 + Rd32(data + r + 4);
                        if (id3 == 0xea624f46) {
                            FengObject *object = new FengObject;
                            memset(object->colour, 0, sizeof(object->colour));
                            memset(static_cast<HudVisibilityObject *>(object), 0, sizeof(HudVisibilityObject));
                            object->hash = object->guid = object->flags1C = object->parentGuid = 0;
                            object->resource = -1;
                            object->textureHash = 0;
                            object->justification = 0;
                            object->font = 0;
                            object->parent = 0;
                            object->imageMask = 0;
                            object->activeScript = -1;
                            object->scriptMilliseconds = 0;
                            object->angle = 0;
                            object->depth = 0;
                            for (int i = 0; i < 3; ++i) object->pivot[i] = object->position[i] = 0, object->size[i] = 1;
                            object->rotation[0] = object->rotation[1] = object->rotation[2] = 0;
                            object->rotation[3] = 1;
                            object->uv[0] = object->uv[1] = 0;
                            object->uv[2] = object->uv[3] = 1;
                            std::vector<unsigned char> state;
                            std::vector<std::vector<unsigned char> > scripts;
                            size_t s = r + 8;
                            while (s + 8 <= end3) {
                                unsigned int id4 = Rd32(data + s);
                                size_t end4 = s + 8 + Rd32(data + s + 4);
                                if (id4 == 0x446a624f) {  // 'ObjD'
                                    size_t t = s + 8;
                                    while (t + 4 <= end4) {
                                        unsigned int tag = Rd16(data + t);
                                        unsigned int len = Rd16(data + t + 2);
                                        const unsigned char *pl = data + t + 4;
                                        switch (tag) {
                                        case 0x744f: object->type18 = (int)Rd32(pl); break;  // 'Ot'
                                        case 0x684f: object->hash = Rd32(pl); break;       // 'Oh'
                                        case 0x504f:                                       // 'OP'
                                            object->guid = Rd32(pl);
                                            object->flags1C = Rd32(pl + 8);
                                            object->resource = (short)Rd32(pl + 12);
                                            break;
                                        case 0x4150: object->parentGuid = Rd32(pl); break;  // 'PA'
                                        case 0x4153: state.assign(pl, pl + len); break;     // 'SA'
                                        case 0x7453: {                                      // 'St' UTF-16
                                            for (unsigned int i = 0; i + 1 < len; i += 2) {
                                                unsigned int c = Rd16(pl + i);
                                                if (!c) break;
                                                object->text += (char)(c < 128 ? c : '?');
                                            }
                                            break;
                                        }
                                        case 0x6a53: object->justification = (int)Rd32(pl); break;  // 'Sj'
                                        case 0x4853: object->font = Rd32(pl); break;                 // 'SH'
                                        }
                                        t += 4 + len;
                                    }
                                } else if (id4 == 0x70726353) {  // 'Scrp'
                                    scripts.push_back(std::vector<unsigned char>(data + s + 8, data + end4));
                                }
                                s = end4;
                            }
                            // Run the INIT script's tracks over the state block (0x541f60 at time 0).
                            for (size_t si = 0; si < scripts.size(); ++si) {
                                const std::vector<unsigned char> &sc = scripts[si];
                                FengScript script;
                                script.hash = 0;
                                script.duration = 0;
                                std::vector<FengTrack> &tracks = script.tracks;
                                size_t t = 0;
                                while (t + 4 <= sc.size()) {
                                    unsigned int tag = Rd16(&sc[t]);
                                    unsigned int len = Rd16(&sc[t + 2]);
                                    const unsigned char *pl = &sc[t + 4];
                                    if (tag == 0x6853) { script.hash = Rd32(pl); if (len >= 8) script.duration = (int)Rd32(pl + 4); }  // 'Sh'
                                    else if (tag == 0x4946) {            // 'FI'
                                        FengTrack track;
                                        track.type = pl[0];
                                        track.size = pl[1];
                                        track.interpolation = pl[2];
                                        track.cycle = pl[3] & 0x7f;
                                        track.duration = (int)(Rd32(pl + 4) & 0xffffff);
                                        track.offset = 0;
                                        tracks.push_back(track);
                                    } else if (tag == 0x6f54 && !tracks.empty()) {  // 'To'
                                        tracks.back().offset = (int)Rd32(pl);
                                    } else if (tag == 0x5645) {  // 'EV'
                                        for (unsigned int k = 0; k + 12 <= len; k += 12) {
                                            FengEvent event = {Rd32(pl + k), Rd32(pl + k + 4), (int)Rd32(pl + k + 8)};
                                            script.events.push_back(event);
                                        }
                                    } else if (tag == 0x644b && !tracks.empty()) {  // 'Kd'
                                        FengTrack &track = tracks.back();
                                        unsigned int stride = track.size + 4;
                                        for (unsigned int k = 0; k + stride <= len; k += stride) {
                                            int time = (int)Rd32(pl + k);
                                            track.keys.push_back(std::make_pair(
                                                time, std::vector<unsigned char>(pl + k + 4, pl + k + stride)));
                                        }
                                    }
                                    t += 4 + len;
                                }
                                object->scripts.push_back(script);
                                if (script.hash == kInitScript) {
                                    for (size_t k = 0; k < tracks.size(); ++k)
                                        ApplyTrack(tracks[k], state.empty() ? 0 : &state[0], (int)state.size(), 0);
                                }
                            }
                            if (!state.empty()) ReadState(object, &state[0], (int)state.size());
                            if (object->type18 == 1 && object->resource >= 0 &&
                                object->resource < (int)resourceHashes.size()) {
                                object->textureHash = resourceHashes[object->resource];
                            }
                            // Rotation about z from the quaternion (screen y down, positive = clockwise).
                            object->angle = 2.0f * atan2f(object->rotation[2], object->rotation[3]) * 180.0f / 3.14159265f;
                            objects_.push_back(object);
                        }
                        r = end3;
                    }
                }
                q = end2;
            }
        }
        p = chunkEnd;
    }
    // Parent links by GUID, in file order.
    for (size_t i = 0; i < objects_.size(); ++i) {
        FengObject *object = objects_[i];
        if (object->parentGuid) {
            for (size_t j = 0; j < objects_.size(); ++j) {
                if (objects_[j]->guid == object->parentGuid) {
                    object->parent = objects_[j];
                    objects_[j]->children.push_back(object);
                    break;
                }
            }
        }
        if (!object->parent) roots_.push_back(object);
    }
    // Native loader seam: build the original FEng child-chain prefix from PA links.
    for (size_t i = 0; i < objects_.size(); ++i) {
        FengObject *object = objects_[i];
        object->childCount60 = (int)object->children.size();
        object->firstChild64 = object->children.empty() ? 0 : object->children[0];
        for (size_t j = 0; j < object->children.size(); ++j)
            object->children[j]->next4 = j + 1 < object->children.size() ? object->children[j + 1] : 0;
    }
    return !objects_.empty();
}

FengObject *FengPackage::Find(unsigned int hash) {
    for (size_t i = 0; i < objects_.size(); ++i) {
        if (objects_[i]->hash == hash) return objects_[i];
    }
    return 0;
}

void FengPackage::Describe() const {
    for (size_t i = 0; i < objects_.size(); ++i) {
        const FengObject *o = objects_[i];
        fprintf(stderr, "#%zu type %d hash %08x guid %x parent %x flags %08x pos %.0f,%.0f,%.1f size %.1f,%.1f tex %08x pivot %.1f,%.1f angle %.1f\n", i,
                o->type18, o->hash, o->guid, o->parentGuid, o->flags1C, o->position[0], o->position[1], o->position[2], o->size[0],
                o->size[1], o->textureHash, o->pivot[0], o->pivot[1], o->angle);
    }
}

void FengPackage::Hide(FengObject *object) { HudHideObject(object); }

void FengPackage::Show(FengObject *object) { HudShowObject(object); }

void FengPackage::SetRotation(FengObject *object, float degrees) {
    if (object) object->angle = degrees;
}

void FengPackage::PlayScript(FengObject *object, const char *name, bool restart) {
    if (!object) return;
    unsigned int hash = FengHash(name);
    for (size_t i = 0; i < object->scripts.size(); ++i) {
        if (object->scripts[i].hash != hash) continue;
        if (object->activeScript != (int)i || restart) object->scriptMilliseconds = 0;
        object->activeScript = (int)i;
        return;
    }
}

void FengPackage::AdvanceScripts(float seconds) {
    for (size_t i = 0; i < objects_.size(); ++i) {
        FengObject *object = objects_[i];
        if (object->activeScript < 0) continue;
        const FengScript &script = object->scripts[object->activeScript];
        unsigned char state[84] = {};
        memcpy(state, object->colour, 16);
        memcpy(state + 16, object->pivot, 12);
        memcpy(state + 28, object->position, 12);
        memcpy(state + 40, object->rotation, 16);
        memcpy(state + 56, object->size, 12);
        memcpy(state + 68, object->uv, 16);
        for (size_t k = 0; k < script.tracks.size(); ++k)
            ApplyTrack(script.tracks[k], state, sizeof(state), (int)object->scriptMilliseconds);
        ReadState(object, state, sizeof(state));
        object->scriptMilliseconds += seconds * 1000.0f;
        if (object->scriptMilliseconds > script.duration) object->scriptMilliseconds = (float)script.duration + 1.0f;
    }
}

void FengPackage::SetColour(FengObject *object, unsigned int argb) {
    if (!object) return;
    object->colour[0] = argb & 0xff;
    object->colour[1] = (argb >> 8) & 0xff;
    object->colour[2] = (argb >> 16) & 0xff;
    object->colour[3] = argb >> 24;
}

void FengPackage::SetTexture(FengObject *object, unsigned int hash) {
    if (object) object->textureHash = hash;
}

void FengPackage::SetPosition(FengObject *object, float x, float y) {
    if (!object) return;
    object->position[0] = x;
    object->position[1] = y;
}

void FengPackage::GetPosition(FengObject *object, float *x, float *y) {
    if (!object) return;
    *x = object->position[0];
    *y = object->position[1];
}

void FengPackage::GetSize(FengObject *object, float *x, float *y) {
    if (!object) return;
    *x = object->size[0];
    *y = object->size[1];
}

void FengPackage::SetState(FengObject *object, int key, const float *value) {
    if (!object) return;
    if (key == 5 || key == 6) {
        int offset = key == 5 ? 0 : 2;
        object->uv[offset] = value[0];
        object->uv[offset + 1] = value[1];
    }
}

void FengPackage::SetText(FengObject *object, const char *text) {
    if (object) object->text = text;
}

void FengPackage::Gather(FengObject *object, const float *parent, float parentAlpha, std::vector<DrawItem> *items) {
    if (object->Hidden()) return;
    float a = parent[0], b = parent[1], c = parent[2], d = parent[3], tx = parent[4], ty = parent[5];
    float rad = object->angle * 3.14159265f / 180.0f;
    float cs = cosf(rad), sn = sinf(rad);
    // Rotate about the FEng pivot relative to the object's centre. The north
    // marker's (0, 72) pivot puts its rotation centre at the minimap centre.
    // Positive angles rotate clockwise in the screen's y-down coordinates.
    float pivotX = object->pivot[0], pivotY = object->pivot[1];
    float px = object->position[0] + pivotX - cs * pivotX + sn * pivotY;
    float py = object->position[1] + pivotY - sn * pivotX - cs * pivotY;
    float m[6];
    m[0] = a * cs + c * sn;
    m[1] = b * cs + d * sn;
    m[2] = -a * sn + c * cs;
    m[3] = -b * sn + d * cs;
    m[4] = a * px + c * py + tx;
    m[5] = b * px + d * py + ty;
    std::map<FengObject *, std::pair<float, float> >::const_iterator translation = modTranslations_.find(object);
    if (translation != modTranslations_.end()) {
        m[4] += translation->second.first;
        m[5] += translation->second.second;
    }
    float alpha = parentAlpha * (float)object->colour[3] / 255.0f;
    if (object->type18 == 5) {
        // Groups scale their children by their size.
        float child[6];
        child[0] = m[0] * object->size[0];
        child[1] = m[1] * object->size[0];
        child[2] = m[2] * object->size[1];
        child[3] = m[3] * object->size[1];
        child[4] = m[4];
        child[5] = m[5];
        for (size_t i = 0; i < object->children.size(); ++i) {
            FengObject *kid = object->children[i];
            kid->depth = object->depth + kid->position[2];
            Gather(kid, child, alpha, items);
        }
        return;
    }
    if (alpha <= 0.0f) return;
    DrawItem item;
    item.object = object;
    memcpy(item.matrix, m, sizeof(m));
    item.depth = object->depth;
    item.alpha = alpha;
    items->push_back(item);
}

static bool DepthFar(const std::pair<float, int> &a, const std::pair<float, int> &b) {
    return a.first > b.first;
}

void FengPackage::Draw(NativeTextureCache *textures, NativeFont *font, int width, int height) {
    std::vector<DrawItem> items;
    const float identity[6] = {1, 0, 0, 1, 0, 0};
    for (size_t i = 0; i < roots_.size(); ++i) {
        roots_[i]->depth = roots_[i]->position[2];
        Gather(roots_[i], identity, 1.0f, &items);
    }
    std::vector<std::pair<float, int> > order;
    for (size_t i = 0; i < items.size(); ++i) order.push_back(std::make_pair(items[i].depth, (int)i));
    std::stable_sort(order.begin(), order.end(), DepthFar);

    float scale = height / 480.0f;
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, width, height, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_TEXTURE_2D);
    for (size_t n = 0; n < order.size(); ++n) {
        const DrawItem &item = items[order[n].second];
        const FengObject *o = item.object;
        float R = o->colour[2] / 255.0f, G = o->colour[1] / 255.0f, B = o->colour[0] / 255.0f;
        // virtual -> window: origin at the centre, uniform scale by height
        float ma = item.matrix[0] * scale, mb = item.matrix[1] * scale, mc = item.matrix[2] * scale,
              md = item.matrix[3] * scale;
        float mtx = item.matrix[4] * scale + width * 0.5f, mty = item.matrix[5] * scale + height * 0.5f;
        if (o->type18 == 1) {
            // Original 0x5bcc30 selects SRCALPHA/ONE for mode 2 and
            // DESTALPHA/ONE for mode 6. Ordinary source-alpha blending made
            // the black NOS mask/gamebreaker background an opaque rectangle.
            // Destination-alpha/stencil composition remains a native seam.
            const NativeImage *image = textures->Find(o->textureHash);
            int blend = image ? image->alphaBlend : 1;
            if (blend == 2) glBlendFunc(GL_SRC_ALPHA, GL_ONE);
            else if (blend == 5) glBlendFunc(GL_DST_ALPHA, GL_ONE_MINUS_DST_ALPHA);
            else if (blend == 6) glBlendFunc(GL_DST_ALPHA, GL_ONE);
            else glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            bool alpha = false;
            GLuint tex = o->textureHash ? textures->Get(o->textureHash, &alpha) : 0;
            if (tex) glBindTexture(GL_TEXTURE_2D, tex);
            else {
                glDisable(GL_TEXTURE_2D);
                static bool warned[1] = {false};
                if (o->textureHash && getenv("UG2_DEBUG_HUD") && !warned[0]) fprintf(stderr, "HUD: texture %08x of object %08x not loaded\n", o->textureHash, o->hash);
            }
            glColor4f(R, G, B, item.alpha);
            float hw = o->size[0] * 0.5f, hh = o->size[1] * 0.5f;
            float xs[4] = {-hw, hw, hw, -hw}, ys[4] = {-hh, -hh, hh, hh};
            float us[4] = {o->uv[0], o->uv[2], o->uv[2], o->uv[0]};
            float vs[4] = {o->uv[1], o->uv[1], o->uv[3], o->uv[3]};
            bool masked = false;
            if (tex && o->imageMask) {
                for (size_t mi = 0; mi < items.size(); ++mi) {
                    const DrawItem &mask = items[mi];
                    if (mask.object != o->imageMask) continue;
                    GLuint maskTexture = textures->Get(mask.object->textureHash, 0);
                    if (!maskTexture) break;
                    // Native render seam: TRACKMAP4000's DXT3 alpha is zero; its
                    // observed TextureInfo blend byte is 5. Sample its RGB through
                    // the retail Minimap_Mask silhouette, without claiming D3D parity.
                    glPushAttrib(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT | GL_ENABLE_BIT | GL_TEXTURE_BIT);
                    glEnable(GL_STENCIL_TEST);
                    glStencilMask(0xff);
                    glClearStencil(0);
                    glClear(GL_STENCIL_BUFFER_BIT);
                    glStencilFunc(GL_ALWAYS, 1, 0xff);
                    glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
                    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
                    glEnable(GL_ALPHA_TEST);
                    glAlphaFunc(GL_GREATER, 0.01f);
                    glBindTexture(GL_TEXTURE_2D, maskTexture);
                    glBegin(GL_QUADS);
                    for (int k = 0; k < 4; ++k) {
                        float x = (k == 0 || k == 3 ? -0.5f : 0.5f) * mask.object->size[0];
                        float y = (k < 2 ? -0.5f : 0.5f) * mask.object->size[1];
                        glTexCoord2f(mask.object->uv[k == 0 || k == 3 ? 0 : 2],
                                     mask.object->uv[k < 2 ? 1 : 3]);
                        glVertex2f((mask.matrix[0] * x + mask.matrix[2] * y + mask.matrix[4]) * scale + width * 0.5f,
                                   (mask.matrix[1] * x + mask.matrix[3] * y + mask.matrix[5]) * scale + height * 0.5f);
                    }
                    glEnd();
                    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
                    glDisable(GL_ALPHA_TEST);
                    glStencilFunc(GL_EQUAL, 1, 0xff);
                    glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
                    glBindTexture(GL_TEXTURE_2D, tex);
                    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
                    glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_MODULATE);
                    glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_TEXTURE);
                    glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB, GL_PRIMARY_COLOR);
                    glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_REPLACE);
                    glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA, GL_PRIMARY_COLOR);
                    masked = true;
                    break;
                }
            }
            glBegin(GL_QUADS);
            for (int k = 0; k < 4; ++k) {
                glTexCoord2f(us[k], vs[k]);
                glVertex2f(ma * xs[k] + mc * ys[k] + mtx, mb * xs[k] + md * ys[k] + mty);
            }
            glEnd();
            if (masked) glPopAttrib();
            if (!tex) glEnable(GL_TEXTURE_2D);
        } else if (o->type18 == 2 && font && font->Ready() && !o->text.empty()) {
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glColor4f(R, G, B, item.alpha);
            glPushMatrix();
            float m16[16] = {ma, mb, 0, 0, mc, md, 0, 0, 0, 0, 1, 0, mtx, mty, 0, 1};
            glMultMatrixf(m16);
            font->Draw(o->text, 0, 0, o->size[0], o->justification);
            glPopMatrix();
        }
    }
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
}

NativeFont::NativeFont() : texture_(0), bakedSize_(48.0f) { memset(glyphs_, 0, sizeof(glyphs_)); }

bool NativeFont::Load(const char *explicitPath) {
    const char *candidates[] = {
        explicitPath,
        "/usr/share/fonts/truetype/lato/Lato-SemiboldItalic.ttf",
        "/usr/share/fonts/truetype/lato/Lato-BoldItalic.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans-BoldOblique.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-BoldItalic.ttf",
        "/usr/share/fonts/TTF/DejaVuSans-BoldOblique.ttf",
    };
    std::vector<unsigned char> file;
    for (size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); ++i) {
        if (candidates[i] && ReadNativeFile(candidates[i], &file)) break;
        file.clear();
    }
    if (file.empty()) return false;
    const int atlas = 512;
    std::vector<unsigned char> alpha(atlas * atlas);
    stbtt_pack_context context;
    if (!stbtt_PackBegin(&context, &alpha[0], atlas, atlas, 0, 1, 0)) return false;
    stbtt_packedchar packed[96];
    stbtt_PackFontRange(&context, &file[0], 0, bakedSize_, 32, 95, packed);
    stbtt_PackEnd(&context);
    for (int i = 0; i < 95; ++i) {
        Glyph &g = glyphs_[i];
        g.x0 = packed[i].xoff;
        g.y0 = packed[i].yoff;
        g.x1 = packed[i].xoff2;
        g.y1 = packed[i].yoff2;
        g.u0 = packed[i].x0 / (float)atlas;
        g.v0 = packed[i].y0 / (float)atlas;
        g.u1 = packed[i].x1 / (float)atlas;
        g.v1 = packed[i].y1 / (float)atlas;
        g.advance = packed[i].xadvance;
    }
    std::vector<unsigned char> rgba(atlas * atlas * 4);
    for (int i = 0; i < atlas * atlas; ++i) {
        rgba[i * 4] = rgba[i * 4 + 1] = rgba[i * 4 + 2] = 255;
        rgba[i * 4 + 3] = alpha[i];
    }
    glGenTextures(1, &texture_);
    glBindTexture(GL_TEXTURE_2D, texture_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, atlas, atlas, 0, GL_RGBA, GL_UNSIGNED_BYTE, &rgba[0]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    return true;
}

float NativeFont::Measure(const std::string &text) const {
    float width = 0;
    for (size_t i = 0; i < text.size(); ++i) {
        int c = (unsigned char)text[i] - 32;
        if (c >= 0 && c < 95) width += glyphs_[c].advance;
    }
    return width;
}

void NativeFont::Draw(const std::string &text, float x, float y, float scale, int justification) {
    // The text scale of an FEng string is relative to a ~32 unit glyph height; the baked face is
    // bakedSize_ high, so scale to 32/bakedSize_ per unit.
    float unit = scale * 32.0f / bakedSize_;
    float width = Measure(text) * unit;
    float left = x;
    // 'Sj' flags, read from retail string data: bit 0 centres, bit 1 right-aligns (bits 2/3
    // are the vertical equivalents; the baseline below approximates them).
    if (justification & 1) left = x - width * 0.5f;
    else if (justification & 2) left = x - width;
    glBindTexture(GL_TEXTURE_2D, texture_);
    glBegin(GL_QUADS);
    float pen = left;
    float baseline = y + bakedSize_ * unit * 0.35f;
    for (size_t i = 0; i < text.size(); ++i) {
        int c = (unsigned char)text[i] - 32;
        if (c < 0 || c >= 95) continue;
        const Glyph &g = glyphs_[c];
        glTexCoord2f(g.u0, g.v0); glVertex2f(pen + g.x0 * unit, baseline + g.y0 * unit);
        glTexCoord2f(g.u1, g.v0); glVertex2f(pen + g.x1 * unit, baseline + g.y0 * unit);
        glTexCoord2f(g.u1, g.v1); glVertex2f(pen + g.x1 * unit, baseline + g.y1 * unit);
        glTexCoord2f(g.u0, g.v1); glVertex2f(pen + g.x0 * unit, baseline + g.y1 * unit);
        pen += g.advance * unit;
    }
    glEnd();
}
