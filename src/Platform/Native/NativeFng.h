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
#include "../../Speed/Indep/Src/Frontend/FEngCoordinates.h"

#include <GL/gl.h>
#include <string>
#include <vector>
#include <map>

// Original 0x505450: case-insensitive h = h * 33 + c from 0xffffffff.
unsigned int FengHash(const char *text);

// Platform ownership of the retail script data. Arithmetic primitives are recovered.
struct FengTrack {
    int type, size, interpolation, cycle, duration, offset;
    signed char state7;
    unsigned char flags3;
    std::vector<unsigned char> baseline;
    std::vector<std::pair<int, std::vector<unsigned char> > > keys;
};
// Script event ('EV' records of 12 bytes): message hash, target and time in milliseconds.
// Target 0xffffffff is read as the owning package (EXIT_COMPLETE in UI_Pause.fng).
struct FengEvent {
    unsigned int message;
    unsigned int target;
    int time;
};
struct FengScript {
    FEScript header;  // original prefix: links, lengthC (duration), time10, hash30
    unsigned int hash;
    int duration;
    std::vector<FengTrack> tracks;
    std::vector<FEScriptTrack> originalTracks;
    std::vector<FengEvent> events;
};

// Message responses: 'MsgR' per object and 'PkgR' per package, read like 0x54d1d0. Each response
// names a message ('Mi') and lists commands of type 'Ri', parameter 'Ru' and target 'Rt'; 0x52c7c0
// runs them (0 script, 1 message to an object, 2 message to the game, 0x300/0x301/0x500/0x501 if/else).
struct FengResponseCommand {
    unsigned int type;
    unsigned int parameter;
    unsigned int target;
};
struct FengResponse {
    unsigned int message;
    std::vector<FengResponseCommand> commands;
};
const FengResponse *FindFengResponse(const std::vector<FengResponse> &responses, unsigned int message);

struct FengObject : FEStringObject {
    // Original prefix (HudVisibilityObject): guidC ('OP' +0), hash10 ('Oh'), type18 (1 image,
    // 2 string, 5 group), flags1C, texture24 (image resource hash), data2C (-> colour below),
    // script list scripts4C / currentScript54 (FEScript headers of `scripts`), child list.
    unsigned int parentGuid; // ObjD 'PA'
    int resource;            // ObjD 'OP' +0xc (index into the resource table)
    unsigned int colour[4];  // state block dwords 0..3: B, G, R, A (data2C points here)
    float pivot[3];          // 4..6: rotation pivot relative to the object centre
    float position[3];       // 7..9
    float rotation[4];       // 10..13 quaternion
    float size[3];           // 14..16
    float uv[4];             // 17..20 (images)
    // Strings
    std::string text;
    std::vector<unsigned short> originalText;
    unsigned int font;       // 'SH': string label hash (0x534fc0 hands it to 0x4fe940), not a font
    unsigned int fontHash;   // resource hash of the string's .ffn font (object +0x24)
    // Set by FengPackage
    FengObject *parent;
    FengObject *imageMask;    // native render seam: alpha silhouette used to clip this image
    std::vector<FengObject *> children;
    float angle;             // rotation about z in degrees (radians * 180 / pi), screen y down
    float depth;             // accumulated z for sorting

    std::vector<FengScript> scripts;
    std::vector<FengResponse> responses;
    float scriptRemainder;   // native frame-time fraction below currentScript54->time10

    bool Hidden() const { return (flags1C & 1) != 0; }
};

UG2_CHECK_OFFSET(FengObject, justification6C, 0x6c);
UG2_CHECK_OFFSET(FengObject, leading70, 0x70);
typedef char FengObject_position_state_check[(UG2_OFFSETOF(FengObject, position) - UG2_OFFSETOF(FengObject, colour) == 0x1c) ? 1 : -1];
typedef char FengObject_size_state_check[(UG2_OFFSETOF(FengObject, size) - UG2_OFFSETOF(FengObject, colour) == 0x38) ? 1 : -1];

class NativeFont;

class FengPackage {
public:
    FengPackage();
    ~FengPackage();
    // `offset` is the position of the 0x30203 chunk in `data`.
    bool Load(const unsigned char *data, size_t size, size_t offset, std::string *error);
    FengObject *Find(unsigned int hash);
    FengObject *Find(const char *name) { return Find(FengHash(name)); }
    FengObject *FindGuid(unsigned int guid);
    const std::vector<FengResponse> &Responses() const { return responses_; }
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
    static bool PlayScriptHash(FengObject *object, unsigned int hash, bool restart);  // 0x5178b0
    void AdvanceScripts(float seconds);
    // Native pointer seam, using the same composed transforms as the renderer.
    bool Contains(FengObject *object, float x, float y);
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
    std::vector<FengResponse> responses_;
    std::map<FengObject *, std::pair<float, float> > modTranslations_;
};

// UI text. Retail FEng strings use the bitmap fonts of 0x30201 chunks ("FNTF" glyph tables with a
// texture of the same name): the font list is built by 0x52cd00/0x51bdc0, looked up by 0x51bd70, and
// strings are laid out by 0x51c110 (glyph quads 0x51bfc0). Loading, the font list and GL drawing are
// platform scaffolding; the layout arithmetic follows those routines but is not byte-verified.
// When a font is missing, or `--hud-font` names a TrueType file, stb_truetype draws a system face.
class NativeFont {
public:
    NativeFont();
    ~NativeFont();
    bool Load(const char *gameDirectory, NativeTextureCache *textures, const char *explicitPath);
    FEngFont *FindOriginalFont(unsigned int hash) const;
    bool Ready() const { return texture_ != 0 || !fonts_.empty(); }
    // Draws a string object in its local frame (the current GL matrix, FEng units).
    void DrawString(const FengObject *object);
    struct Glyph { float x0, y0, x1, y1, u0, v0, u1, v1, advance; };

private:
    struct FengFont : FEngFont {
        unsigned int hash;         // +0x14: name hash
        unsigned int textureHash;  // +0x18: texture name hash
        std::vector<unsigned char> fntf;
    };
    const FengFont *Find(unsigned int hash) const;  // 0x51bd70
    void AddFonts(const std::vector<unsigned char> &bundle);
    void DrawRetail(const FengFont &font, const FengObject *object);
    void DrawTrueType(const std::string &text, float scale, int justification);
    float Measure(const std::string &text) const;

    NativeTextureCache *textures_;
    std::vector<FengFont> fonts_;
    GLuint texture_;
    float bakedSize_;
    Glyph glyphs_[96];
};

#endif
