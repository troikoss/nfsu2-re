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

// 0x54d1d0: tagged records 'Mi' (new response), 'Ri' (new command), 'Ru', 'Rt'; 'MC' and 'Rs' unused.
void ReadResponses(const unsigned char *data, size_t size, std::vector<FengResponse> *responses) {
    for (size_t t = 0; t + 4 <= size;) {
        unsigned int tag = Rd16(data + t);
        unsigned int len = Rd16(data + t + 2);
        if (t + 4 + len > size) break;
        unsigned int value = len >= 4 ? Rd32(data + t + 4) : 0;
        if (tag == 0x694d) {
            FengResponse response;
            response.message = value;
            responses->push_back(response);
        } else if (!responses->empty()) {
            std::vector<FengResponseCommand> &commands = responses->back().commands;
            if (tag == 0x6952) {
                FengResponseCommand command = {value, 0, 0};
                commands.push_back(command);
            } else if (tag == 0x7552 && !commands.empty()) {
                commands.back().parameter = value;
            } else if (tag == 0x7452 && !commands.empty()) {
                commands.back().target = value;
            }
        }
        t += 4 + len;
    }
}

// Retained retail tracks use the -1 key as a baseline, and later keys as
// offsets (0x52bdc0). This bounded platform adapter supports message tracks
// of types 4/6, interpolation 1 with cycle 0/1, and constant tracks at time zero.
// Other animation forms remain explicit seams.
void ApplyTrack(const FengTrack &track, unsigned char *state, int stateBytes, int time) {
    if (track.keys.empty() || track.offset < 0 || track.offset * 4 + track.size > stateBytes) return;
    unsigned char *out = state + track.offset * 4;
    const unsigned char *base = &track.keys[0].second[0];
    if (track.keys.size() == 1) { memcpy(out, base, track.size); return; }
    if (track.interpolation != 1 || track.cycle > 1 || (track.type != 4 && track.type != 6)) {
        // The loader can still use a constant first key for unrecovered tracks.
        if (time == 0) { memcpy(out, base, track.size); return; }
        fprintf(stderr, "FEng track type=%d interpolation=%d cycle=%d duration=%d offset=%d time=%d\n", track.type, track.interpolation, track.cycle, track.duration, track.offset, time);
        NativeTrap("FEng message track interpolation/cycle");
    }
    // 0x52bdc0: cycle 1 joins the last key to the first over the remaining duration.
    if (track.cycle == 1 && track.duration > 0) time %= track.duration;
    size_t upper = 1;
    while (upper + 1 < track.keys.size() && track.keys[upper].first < time) ++upper;
    size_t lower = upper > 1 ? upper - 1 : upper;
    float fraction = 1.0f;
    if (track.cycle == 1) {
        int lowerTime, upperTime;
        if (time > track.keys[upper].first) {
            lower = upper; upper = 1;
            lowerTime = track.keys[lower].first;
            upperTime = track.duration + track.keys[upper].first;
        } else if (upper == 1 && time < track.keys[upper].first) {
            lower = track.keys.size() - 1;
            lowerTime = track.keys[lower].first - track.duration;
            upperTime = track.keys[upper].first;
        } else {
            lowerTime = track.keys[lower].first;
            upperTime = track.keys[upper].first;
        }
        fraction = upperTime > lowerTime ? (float)(time - lowerTime) / (upperTime - lowerTime) : 0;
    } else if (track.keys[upper].first > time) {
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
                } else if (id2 == 0x52676b50) {  // 'PkgR'
                    ReadResponses(data + q + 8, end2 - q - 8, &responses_);
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
                            object->leading = 0;
                            object->maxWidth = 0;
                            object->fontHash = 0;
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
                                                object->text += (char)(c < 256 ? c : '?');
                                            }
                                            break;
                                        }
                                        case 0x6a53: object->justification = (int)Rd32(pl); break;  // 'Sj'
                                        case 0x4853: object->font = Rd32(pl); break;                 // 'SH'
                                        case 0x6c53: object->leading = (int)Rd32(pl); break;         // 'Sl'
                                        case 0x7753: object->maxWidth = Rd32(pl); break;             // 'Sw'
                                        }
                                        t += 4 + len;
                                    }
                                } else if (id4 == 0x5267734d) {  // 'MsgR'
                                    ReadResponses(data + s + 8, end4 - s - 8, &object->responses);
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
                            if (object->resource >= 0 && object->resource < (int)resourceHashes.size()) {
                                if (object->type18 == 1) object->textureHash = resourceHashes[object->resource];
                                else if (object->type18 == 2) object->fontHash = resourceHashes[object->resource];
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

FengObject *FengPackage::FindGuid(unsigned int guid) {
    for (size_t i = 0; i < objects_.size(); ++i) {
        if (objects_[i]->guid == guid) return objects_[i];
    }
    return 0;
}

const FengResponse *FindFengResponse(const std::vector<FengResponse> &responses, unsigned int message) {
    for (size_t i = 0; i < responses.size(); ++i) {
        if (responses[i].message == message) return &responses[i];
    }
    return 0;
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
    if (!object) return;
    object->angle = degrees;
    float half = degrees * 3.14159265f / 360.0f;
    object->rotation[0] = object->rotation[1] = 0;
    object->rotation[2] = sinf(half);
    object->rotation[3] = cosf(half);
}

void FengPackage::PlayScript(FengObject *object, const char *name, bool restart) {
    if (object) PlayScriptHash(object, FengHash(name), restart);
}

bool FengPackage::PlayScriptHash(FengObject *object, unsigned int hash, bool restart) {
    if (!object) return false;
    for (size_t i = 0; i < object->scripts.size(); ++i) {
        if (object->scripts[i].hash != hash) continue;
        if (object->activeScript != (int)i || restart) object->scriptMilliseconds = 0;
        object->activeScript = (int)i;
        return true;
    }
    return false;
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
        bool cycling = false;
        for (size_t k = 0; k < script.tracks.size(); ++k) cycling |= script.tracks[k].cycle == 1;
        if (!cycling && object->scriptMilliseconds > script.duration)
            object->scriptMilliseconds = (float)script.duration + 1.0f;
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
    // Rotate about the FEng pivot relative to the object's centre. The north
    // marker's (0, 72) pivot puts its rotation centre at the minimap centre.
    // Positive z angles rotate clockwise in the screen's y-down coordinates.
    // The full quaternion is projected onto the screen plane: UI_Pause mirrors
    // frame corners and bar caps with 180-degree turns about x or y, which a
    // z-only angle drew as 180-degree spins (wrong corners).
    float qx = object->rotation[0], qy = object->rotation[1], qz = object->rotation[2], qw = object->rotation[3];
    float r00 = 1 - 2 * (qy * qy + qz * qz), r01 = 2 * (qx * qy - qz * qw);
    float r10 = 2 * (qx * qy + qz * qw), r11 = 1 - 2 * (qx * qx + qz * qz);
    float pivotX = object->pivot[0], pivotY = object->pivot[1];
    float px = object->position[0] + pivotX - r00 * pivotX - r01 * pivotY;
    float py = object->position[1] + pivotY - r10 * pivotX - r11 * pivotY;
    float m[6];
    m[0] = a * r00 + c * r10;
    m[1] = b * r00 + d * r10;
    m[2] = a * r01 + c * r11;
    m[3] = b * r01 + d * r11;
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
            // Native render seam: GL textures default to GL_REPEAT, so linear filtering at the
            // edge of a sprite blended in texels from the opposite side (bright seams at the
            // corners of the pause frame and icons). FEng sprites inside [0,1] are clamped.
            bool clamp = o->uv[0] >= 0 && o->uv[1] >= 0 && o->uv[2] >= 0 && o->uv[3] >= 0 &&
                         o->uv[0] <= 1 && o->uv[1] <= 1 && o->uv[2] <= 1 && o->uv[3] <= 1;
            if (tex) {
                glBindTexture(GL_TEXTURE_2D, tex);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, clamp ? GL_CLAMP_TO_EDGE : GL_REPEAT);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, clamp ? GL_CLAMP_TO_EDGE : GL_REPEAT);
            } else {
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
            font->DrawString(o);
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

namespace {

// FNTF header (0x30201 chunk data + 0x200): +10 glyph count, +0xc flags (0x40000: 16-byte glyph
// records, else 12), +0x14 glyph table offset, +0x18 kerning table offset, +0x24 line height.
// Glyph records: +0 code, +2 width, +3 height, +4 x, +6 y, +9 x offset, +0xa y offset,
// +0xb kerning pair count, +0xc first kerning pair, +0xe advance.
const unsigned int kFntfMagic = 0x46544e46;

int GlyphStride(const unsigned char *fntf) { return (Rd32(fntf + 0xc) & 0x40000) ? 0x10 : 0xc; }

// 0x6f45b0: binary search of the glyph table.
const unsigned char *SearchGlyph(unsigned int code, const unsigned char *table, unsigned int count, int stride) {
    while (count != 0) {
        const unsigned char *glyph = table + (count >> 1) * stride;
        unsigned int found = Rd16(glyph);
        if (code == found) return glyph;
        if ((int)(code - found) > 0) {
            table = glyph + stride;
            count -= 1;
        }
        count >>= 1;
    }
    return 0;
}

// 0x4fda70: direct index from 0x20, else the search.
const unsigned char *FindGlyph(const unsigned char *fntf, unsigned int code) {
    const unsigned char *table = fntf + Rd32(fntf + 0x14);
    unsigned int count = Rd16(fntf + 10);
    int stride = GlyphStride(fntf);
    int index = (int)(code & 0xffff) - 0x20;
    if (index >= 0 && index < (int)count && Rd16(table + index * stride) == (code & 0xffff))
        return table + index * stride;
    return SearchGlyph(code & 0xffff, table, count, stride);
}

// 0x6f45f0: kerning of `glyph` after `previous`.
int Kerning(const unsigned char *fntf, const unsigned char *glyph, unsigned short previous) {
    unsigned int pairs = glyph[0xb];
    if (pairs == 0) return 0;
    const unsigned char *kerning = fntf + Rd32(fntf + 0x18);
    if ((Rd32(fntf + 0xc) & 0x40000) == 0) {
        int count = (int)Rd32(kerning);
        for (int i = 0; i < count; ++i) {
            const unsigned char *pair = kerning + 4 + i * 4;
            if ((short)Rd16(pair) == (short)previous && pair[3] == Rd16(glyph)) return (signed char)pair[2];
        }
        return 0;
    }
    const unsigned char *pair = kerning + 4 + Rd16(glyph + 0xc) * 4;
    for (unsigned int i = 0; i < pairs; ++i)
        if ((short)Rd16(pair + i * 4) == (short)previous) return (signed char)pair[i * 4 + 2];
    return 0;
}

int GlyphAdvance(const unsigned char *glyph) { return (short)Rd16(glyph + 0xe); }

// Character remapping shared by the measuring and drawing loops.
unsigned short RemapCharacter(unsigned short c) {
    if (c > 0xff7f) c &= 0xff;
    if (c == 0x99) c = 0x2122;
    else if (c == 0x9c) c = 0x153;
    return c;
}

// 0x50ba70: "$name$" tokens are not drawn unless flags 0x820 are set.
bool IsToken(const unsigned short *p, unsigned int flags) {
    if (!p || (flags & 0x820) != 0 || *p != '$') return false;
    int length = 0;
    for (const unsigned short *q = p + 1; *q != 0 && *q != '$'; ++q) ++length;
    return length != 0;
}

// 0x50bac0: skips a token and its closing '$'. Native guard: stop at an unterminated token's end.
const unsigned short *SkipToken(const unsigned short *p, unsigned int flags) {
    if (!p || (flags & 0x820) != 0 || *p != '$') return p;
    ++p;
    if (*p != '$') {
        while (*p != 0 && *p != '$') ++p;
        if (*p != 0) ++p;
    }
    return p;
}

// 0x50bce0: width of the line starting at `p`, stopping before a line break or, with a wrap
// width, at the last space before the width is reached (`wrapped` is then set).
float LineWidth(const unsigned char *fntf, const unsigned short *p, unsigned int flags, unsigned int maxWidth,
                bool *wrapped) {
    if (!p) return 0.0f;
    unsigned short c = *p;
    bool reset = false;
    int count = 0;
    float width = 0.0f, lastSpace = 0.0f;
    if (c == 0) return width;
    for (;;) {
        float before = width;
        if (reset) {
            count = 0;
            reset = false;
        }
        if ((flags & 0x20) == 0 && (c == 10 || c == 0x5e)) return width;
        if (c == 0xd) {
            ++p;
        } else {
            if (c == 0x20) lastSpace = width;
            if ((flags & 0x80) != 0 && c == 0xa0) {
                count = 0;
                c = 0x20;
            }
            if (!IsToken(p, flags)) {
                c = RemapCharacter(c);
                const unsigned char *glyph = FindGlyph(fntf, c);
                if (glyph) {
                    if (count != 0) width += (float)Kerning(fntf, glyph, p[-1]);
                    width += (float)GlyphAdvance(glyph);
                }
                ++p;
            } else {
                p = SkipToken(p, flags);
                reset = true;
            }
            if (maxWidth != 0 && *p != 0 && (float)maxWidth <= width) {
                if ((flags & 0x200) != 0) lastSpace = before;
                if (lastSpace > 0.0f) width = lastSpace;
                if (wrapped) *wrapped = true;
                return width;
            }
        }
        c = *p;
        ++count;
        if (c == 0) return width;
    }
}

// 0x50bf40: height of all lines, `leading` added per line break.
float TextHeight(const unsigned char *fntf, float lineHeight, const unsigned short *p, int leading) {
    if (!p) return 0.0f;
    unsigned short c = *p;
    float height = 0.0f;
    bool glyphs = true;
    if (c != 0) {
        do {
            ++p;
            if (c == 10 || c == 0x5e) {
                glyphs = false;
                height = (float)leading + height + lineHeight;
            } else if (c != 0xd) {
                if (FindGlyph(fntf, c & 0xff)) glyphs = true;
            }
            c = *p;
        } while (c != 0);
        if (!glyphs) return height;
    }
    return height + lineHeight;
}

// 0x50c080: number of lines. A line break only counts once a space has been seen on its line.
int LineCount(const unsigned char *fntf, const unsigned short *p, unsigned int flags, unsigned int maxWidth) {
    if (!p) return 0;
    if ((flags & 0x20) != 0) return 1;
    unsigned short c = *p;
    const unsigned short *space = 0;
    bool reset = false;
    float width = 0.0f;
    int lines = 1, count = 0;
    for (;;) {
        if (c == 0) return lines;
        bool newLine = false;
        if ((c == 10 || c == 0x5e) && space) {
            p = space + 1;
            newLine = true;
        } else if (c == 0xd) {
            ++p;
        } else {
            if (reset) {
                count = 0;
                reset = false;
            }
            if (c == 0x20) space = p;
            if (!IsToken(p, flags)) {
                c = RemapCharacter(c);
                const unsigned char *glyph = FindGlyph(fntf, c);
                if (glyph) {
                    if (count != 0) width += (float)Kerning(fntf, glyph, p[-1]);
                    width += (float)GlyphAdvance(glyph);
                }
                ++p;
            } else {
                p = SkipToken(p, flags);
                reset = true;
            }
            if (maxWidth != 0 && *p != 0 && (float)maxWidth <= width && space) {
                p = space;
                while (*p == 0x20) ++p;
                newLine = true;
            }
        }
        if (newLine) {
            ++lines;
            width = 0.0f;
            space = 0;
            reset = true;
        }
        c = *p;
        ++count;
    }
}

// Horizontal start of a line for 'Sj' bits 0 (centre) and 1 (right).
float LineStart(int justification, float width) {
    if (justification & 1) return width * -0.5f;
    if (justification & 2) return -width;
    return 0.0f;
}

// 0x7f918c: per-font vertical offset and leading scale, keyed by name hash.
const struct { unsigned int hash; float offsetY, leadingScale; } kFontAdjustments[] = {
    {0xdca5485a, 18.0f, 2.0f},
    {0x833a8678, 21.5f, 2.0f},
    {0xf88a75f9, 18.0f, 1.0f},
    {0x71c777d7, 23.0f, 1.0f},
};
// 0x7f9184: substitute fonts, tried when a font is not loaded.
const unsigned int kFontSubstitutes[][2] = {
    {0x9583aa1a, 0x5b9d88b9},  // Font_ConduitMdITCTT38bi -> ConduitMdITC_TT21i
};

}  // namespace

NativeFont::NativeFont() : textures_(0), texture_(0), bakedSize_(48.0f) { memset(glyphs_, 0, sizeof(glyphs_)); }

// Platform seam for the 0x30201 handler 0x52cd00 and constructor 0x51bdc0: font name at +0,
// texture name at +0x100, FNTF at +0x200. Language-suffix stripping of the name is not needed for
// the English data loaded here.
void NativeFont::AddFonts(const std::vector<unsigned char> &bundle) {
    for (size_t offset = 0; offset + 8 <= bundle.size();) {
        unsigned int id = Rd32(&bundle[offset]);
        unsigned int length = Rd32(&bundle[offset + 4]);
        if (length > bundle.size() - offset - 8) break;
        const unsigned char *data = &bundle[offset + 8];
        if (id == 0x30201 && length > 0x228 && Rd32(data + 0x200) == kFntfMagic) {
            std::string name((const char *)data, strnlen((const char *)data, 0x100));
            std::string texture((const char *)data + 0x100, strnlen((const char *)data + 0x100, 0x100));
            FengFont font;
            font.hash = FengHash(name.c_str());
            font.textureHash = FengHash(texture.c_str());
            font.fntf.assign(data + 0x200, data + length);
            const unsigned char *fntf = &font.fntf[0];
            size_t glyphEnd = Rd32(fntf + 0x14) + (size_t)Rd16(fntf + 10) * GlyphStride(fntf);
            if (glyphEnd > font.fntf.size() || Rd32(fntf + 0x18) + 4 > font.fntf.size()) {
                offset += 8 + length;
                continue;
            }
            font.lineHeight = (float)(int)Rd32(fntf + 0x24);
            font.offsetY = 0.0f;
            font.leadingScale = 1.0f;
            for (size_t i = 0; i < sizeof(kFontAdjustments) / sizeof(kFontAdjustments[0]); ++i) {
                if (kFontAdjustments[i].hash == font.hash) {
                    font.offsetY = kFontAdjustments[i].offsetY;
                    font.leadingScale = kFontAdjustments[i].leadingScale;
                }
            }
            fonts_.push_back(font);
        }
        offset += 8 + length;
    }
}

const NativeFont::FengFont *NativeFont::Find(unsigned int hash) const {
    for (int guard = 0; guard < 4; ++guard) {
        for (size_t i = 0; i < fonts_.size(); ++i)
            if (fonts_[i].hash == hash) return &fonts_[i];
        size_t i = 0, n = sizeof(kFontSubstitutes) / sizeof(kFontSubstitutes[0]);
        while (i < n && kFontSubstitutes[i][0] != hash) ++i;
        if (i == n) return 0;
        hash = kFontSubstitutes[i][1];
    }
    return 0;
}

static bool LoadTrueType(const char *explicitPath, GLuint *texture, float bakedSize, NativeFont::Glyph *glyphs);

bool NativeFont::Load(const char *gameDirectory, NativeTextureCache *textures, const char *explicitPath) {
    textures_ = textures;
    std::string base = gameDirectory ? gameDirectory : ".";
    const char *bundles[][2] = {
        {"/GLOBAL/GLOBALA.BUN", 0},
        {"/LANGUAGES/English.bin", "/LANGUAGES/LanguageTextures.bin"},
    };
    if (!explicitPath) {
        for (size_t i = 0; i < sizeof(bundles) / sizeof(bundles[0]); ++i) {
            std::vector<unsigned char> bundle;
            if (!ReadNativeFile(base + bundles[i][0], &bundle) || bundle.empty()) continue;
            AddFonts(bundle);
            NativeImageMap images;
            if (bundles[i][1]) {
                std::vector<unsigned char> pack;
                if (ReadNativeFile(base + bundles[i][1], &pack) && !pack.empty())
                    LoadNativeTextures(&pack[0], pack.size(), &images);
            } else {
                LoadNativeTextures(&bundle[0], bundle.size(), &images);
            }
            if (textures) textures->Add(images);
        }
    }
    // The TrueType face draws strings whose font is missing, or all strings with `--hud-font`.
    bool trueType = LoadTrueType(explicitPath, &texture_, bakedSize_, glyphs_);
    return Ready() || trueType;
}

void NativeFont::DrawString(const FengObject *object) {
    const FengFont *font = Find(object->fontHash);
    if (!font) {
        if (texture_) DrawTrueType(object->text, object->size[0], object->justification);
        return;
    }
    // Native seam: the string's size scales its FEng units like the retail object matrix.
    glPushMatrix();
    glScalef(object->size[0], object->size[1], 1.0f);
    DrawRetail(*font, object);
    glPopMatrix();
}

// 0x51c110 layout with 0x51bfc0 glyph quads, in the string's local FEng units.
void NativeFont::DrawRetail(const FengFont &font, const FengObject *object) {
    const NativeImage *image = textures_ ? textures_->Find(font.textureHash) : 0;
    GLuint texture = textures_ ? textures_->Get(font.textureHash, 0) : 0;
    if (!image || !texture || image->width <= 0 || image->height <= 0) return;
    std::vector<unsigned short> text;
    for (size_t i = 0; i < object->text.size(); ++i) text.push_back((unsigned char)object->text[i]);
    text.push_back(0);
    const unsigned char *fntf = &font.fntf[0];
    const unsigned short *p = &text[0];
    int justification = object->justification;
    unsigned int flags = object->flags1C;
    unsigned int maxWidth = object->maxWidth;
    int leading = (int)((float)object->leading * font.leadingScale);
    float height;
    if ((justification & 5) == 0) height = TextHeight(fntf, font.lineHeight, p, leading);
    else height = ((float)object->leading * font.leadingScale + font.lineHeight) *
                  (float)LineCount(fntf, p, flags, maxWidth);
    float y = 0.0f;
    if (justification & 4) y = height * -0.5f;
    else if (justification & 8) y = -height;
    float lineStart = LineStart(justification, LineWidth(fntf, p, flags, maxWidth, 0));
    float pen = lineStart;
    bool wrapped = false;
    float lineWidth = LineWidth(fntf, p, flags, maxWidth, &wrapped);
    float textureWidth = (float)image->width, textureHeight = (float)image->height;
    bool reset = false;
    int count = 0;
    glBindTexture(GL_TEXTURE_2D, texture);
    glBegin(GL_QUADS);
    unsigned short c = *p;
    while (c != 0) {
        if (reset) {
            count = 0;
            reset = false;
        }
        bool newLine = false;
        if ((flags & 0x20) == 0 && (c == 10 || c == 0x5e)) {
            ++p;
            if (*p == 0) break;
            lineWidth = LineWidth(fntf, p, flags, maxWidth, &wrapped);
            lineStart = LineStart(justification, lineWidth);
            newLine = true;
        } else {
            if (IsToken(p, flags)) {
                p = SkipToken(p, flags);
                reset = true;
            } else {
                bool kern = true;
                if ((flags & 0x80) != 0 && c == 0xa0) {
                    kern = false;
                    c = 0x20;
                } else {
                    c = RemapCharacter(c);
                }
                const unsigned char *glyph = FindGlyph(fntf, c);
                if (glyph) {
                    if (count != 0 && kern) pen += (float)Kerning(fntf, glyph, p[-1]);
                    // 0x51bfc0: quad at least 4 units wide; UVs take the transparent texel ring
                    // around the glyph. GL seam: a half-texel inset keeps bilinear filtering from
                    // reaching the neighbouring glyphs of the packed texture when scaled up.
                    float x0 = (float)(signed char)glyph[9] + pen;
                    float y0 = (float)(signed char)glyph[10] + font.offsetY + y;
                    float w = glyph[2] < 4 ? 4.0f : (float)glyph[2];
                    float x1 = x0 + w, y1 = (float)glyph[3] + y0;
                    float u0 = ((float)((int)Rd16(glyph + 4) - 1) + 0.5f) / textureWidth;
                    float v0 = ((float)((int)Rd16(glyph + 6) - 1) + 0.5f) / textureHeight;
                    float u1 = ((float)(glyph[2] + 1 + Rd16(glyph + 4)) - 0.5f) / textureWidth;
                    float v1 = ((float)(glyph[3] + 1 + Rd16(glyph + 6)) - 0.5f) / textureHeight;
                    glTexCoord2f(u0, v0); glVertex2f(x0, y0);
                    glTexCoord2f(u1, v0); glVertex2f(x1, y0);
                    glTexCoord2f(u1, v1); glVertex2f(x1, y1);
                    glTexCoord2f(u0, v1); glVertex2f(x0, y1);
                    pen += (float)GlyphAdvance(glyph);
                }
                ++p;
            }
            if (maxWidth != 0 && wrapped && lineWidth <= pen - lineStart) {
                while (*p == 0x20) ++p;
                lineWidth = LineWidth(fntf, p, flags, maxWidth, &wrapped);
                lineStart = LineStart(justification, lineWidth);
                newLine = true;
            }
        }
        if (newLine) {
            y = (float)leading + font.lineHeight + y;
            pen = lineStart;
        }
        c = *p;
        ++count;
    }
    glEnd();
}

static bool LoadTrueType(const char *explicitPath, GLuint *texture, float bakedSize, NativeFont::Glyph *glyphs) {
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
    stbtt_PackFontRange(&context, &file[0], 0, bakedSize, 32, 95, packed);
    stbtt_PackEnd(&context);
    for (int i = 0; i < 95; ++i) {
        NativeFont::Glyph &g = glyphs[i];
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
    glGenTextures(1, texture);
    glBindTexture(GL_TEXTURE_2D, *texture);
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

void NativeFont::DrawTrueType(const std::string &text, float scale, int justification) {
    const float x = 0, y = 0;
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
