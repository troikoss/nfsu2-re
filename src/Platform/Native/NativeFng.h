#ifndef UG2_NATIVE_FNG_H
#define UG2_NATIVE_FNG_H

// Platform scaffolding: reads UG2's FEng package (.fng) data and draws it with OpenGL. The chunk
// layout follows the original loader (package reader 0x5554a0, object reader 0x555140, script
// reader 0x5515e0, resource table 0x518380); the FEng object model and renderer themselves are not
// recovered, so objects here are plain structs holding the observed per-object state block.
//
// Coordinates are the 640x480 FEng screen with its origin at the screen centre, y down.
#include "NativeScene.h"
#include "../../Speed/Indep/Src/Hud/HudVisibility.h"

#include <GL/gl.h>
#include <string>
#include <vector>
#include <map>

// Original 0x505450: case-insensitive h = h * 33 + c from 0xffffffff.
unsigned int FengHash(const char *text);

// Platform ownership of the retail script data. Arithmetic primitives are recovered.
struct FengTrack {
    int type, size, interpolation, cycle, duration, offset;
    std::vector<std::pair<int, std::vector<unsigned char> > > keys;
};
struct FengScript {
    unsigned int hash;
    int duration;
    std::vector<FengTrack> tracks;
};

struct FengObject : HudVisibilityObject {
    // Recovered prefix: type18 (1 image, 2 string, 5 group), flags1C, child list.
    unsigned int hash;       // ObjD 'Oh'
    unsigned int guid;       // ObjD 'OP' +0
    unsigned int parentGuid; // ObjD 'PA'
    int resource;            // ObjD 'OP' +0xc (index into the resource table)
    unsigned int textureHash;  // 0 for untextured images
    unsigned int colour[4];  // state block dwords 0..3: B, G, R, A
    float pivot[3];          // 4..6: rotation pivot relative to the object centre
    float position[3];       // 7..9
    float rotation[4];       // 10..13 quaternion
    float size[3];           // 14..16
    float uv[4];             // 17..20 (images)
    // Strings
    std::string text;
    int justification;       // 'Sj'
    unsigned int font;       // 'SH'
    // Set by FengPackage
    FengObject *parent;
    FengObject *imageMask;    // native render seam: alpha silhouette used to clip this image
    std::vector<FengObject *> children;
    float angle;             // rotation about z in degrees (radians * 180 / pi), screen y down
    float depth;             // accumulated z for sorting

    std::vector<FengScript> scripts;
    int activeScript;
    float scriptMilliseconds;

    bool Hidden() const { return (flags1C & 1) != 0; }
};

class NativeFont;

class FengPackage {
public:
    FengPackage();
    ~FengPackage();
    // `offset` is the position of the 0x30203 chunk in `data`.
    bool Load(const unsigned char *data, size_t size, size_t offset, std::string *error);
    FengObject *Find(unsigned int hash);
    FengObject *Find(const char *name) { return Find(FengHash(name)); }
    std::vector<FengObject *> &Objects() { return objects_; }
    // Names from the executable are not needed to draw; `Describe` lists the package for debugging.
    void Describe() const;
    void ClearModTranslations() { modTranslations_.clear(); }
    void SetModTranslation(FengObject *object, float x, float y) {
        modTranslations_[object] = std::make_pair(x, y);
    }
    // Draws visible objects in a 640x480 virtual screen fitted to the viewport by height.
    void Draw(NativeTextureCache *textures, NativeFont *font, int width, int height);

    // The operations the original HUD features perform on objects.
    static void Hide(FengObject *object);          // 0x50ca00
    static void Show(FengObject *object);          // 0x50ca50
    static void SetRotation(FengObject *object, float degrees);  // 0x51ced0
    static void SetColour(FengObject *object, unsigned int argb);  // 0x51d7f0
    static void SetTexture(FengObject *object, unsigned int hash); // 0x50c990
    static void SetPosition(FengObject *object, float x, float y);
    static void GetPosition(FengObject *object, float *x, float *y);
    static void GetSize(FengObject *object, float *x, float *y);
    static void SetState(FengObject *object, int key, const float *value);
    static void PlayScript(FengObject *object, const char *name, bool restart);
    void AdvanceScripts(float seconds);
    static void SetText(FengObject *object, const char *text);     // 0x537be0

private:
    struct DrawItem {
        FengObject *object;
        float matrix[6];  // a, b, c, d, tx, ty (column form: x' = a*x + c*y + tx)
        float depth;
        float alpha;
    };
    void Gather(FengObject *object, const float *parent, float parentAlpha, std::vector<DrawItem> *items);
    std::vector<FengObject *> objects_;
    std::vector<FengObject *> roots_;
    std::map<FengObject *, std::pair<float, float> > modTranslations_;
};

// Text through the stb_truetype rasteriser. The original draws UI text with GDI fonts
// (CreateFontA/ExtTextOutA) into textures; this substitutes a system italic sans face.
class NativeFont {
public:
    NativeFont();
    bool Load(const char *explicitPath);
    bool Ready() const { return texture_ != 0; }
    // Width of `text` in pixels at font size 1.0 (the baked size).
    float Measure(const std::string &text) const;
    // Draws `text` with the baseline origin centred/left/right (justification 0/1/2 = left,
    // right, centre as read from 'Sj') at (x, y) in the current matrix, scaled by `scale`.
    void Draw(const std::string &text, float x, float y, float scale, int justification);
    float LineHeight() const { return bakedSize_; }

private:
    GLuint texture_;
    float bakedSize_;
    struct Glyph { float x0, y0, x1, y1, u0, v0, u1, v1, advance; };
    Glyph glyphs_[96];
};

#endif
