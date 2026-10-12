// Windowed driving demo for the native build. Everything in this file is platform
// scaffolding: window, input, camera and placeholder rendering. Vehicle behaviour comes
// from the recovered UG2 sources through NativeVehicle.
#include "NativeVehicle.h"
#include "NativeWorld.h"
#include "NativeScene.h"
#include "NativeHud.h"
#include "NativePause.h"
#ifdef UG2_ENABLE_MODDING
#include "NativeModBridge.h"
#endif
#include "../../Speed/Indep/Src/Camera/DriveCamera.h"

#include <SDL2/SDL.h>
#include "NativeUiInput.h"
#include "NativeControls.h"
#include <GL/gl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern float engineValueScale;
extern int drivetrainClock;

namespace {

struct Camera {
    float position[3];
    float target[3];
};

void LookAt(const float *eye, const float *centre, const float *up) {
    float f[3] = {centre[0] - eye[0], centre[1] - eye[1], centre[2] - eye[2]};
    float length = sqrtf(f[0] * f[0] + f[1] * f[1] + f[2] * f[2]);
    for (int i = 0; i < 3; ++i) f[i] /= length;
    float s[3] = {f[1] * up[2] - f[2] * up[1], f[2] * up[0] - f[0] * up[2], f[0] * up[1] - f[1] * up[0]};
    length = sqrtf(s[0] * s[0] + s[1] * s[1] + s[2] * s[2]);
    for (int i = 0; i < 3; ++i) s[i] /= length;
    float u[3] = {s[1] * f[2] - s[2] * f[1], s[2] * f[0] - s[0] * f[2], s[0] * f[1] - s[1] * f[0]};
    float m[16] = {s[0], u[0], -f[0], 0, s[1], u[1], -f[1], 0, s[2], u[2], -f[2], 0, 0, 0, 0, 1};
    glMultMatrixf(m);
    glTranslatef(-eye[0], -eye[1], -eye[2]);
}

// UG2 main view projection (0x5c7ec0): the half-angle tangents come from the camera's field of
// view in 1/65536 turn units. The original far plane for eView type 1 is the immediate 15000.
CameraProjection NativeProjection(unsigned short fov, float nearPlane, int width, int height) {
    CameraProjection projection;
    ComputeCameraProjection(&projection, fov, float(height) / width, nearPlane, 15000.0f);
#ifdef UG2_ENABLE_MODDING
    CameraProjection reference;
    ComputeCameraProjection(&reference, fov, 0.75f, nearPlane, 15000.0f);
    NativeModsProjection(&projection, reference, width, height);
#endif
    return projection;
}
void UgFrustum(const CameraProjection &projection) {
    float nearPlane = projection.nearPlane;
    glFrustum(-projection.tangentX * nearPlane, projection.tangentX * nearPlane,
              -projection.tangentY * nearPlane, projection.tangentY * nearPlane, nearPlane,
              projection.farPlane);
}

void DrawGround(float centreX, float centreY) {
    const float extent = 400.0f;
    float baseX = floorf(centreX / 10.0f) * 10.0f;
    float baseY = floorf(centreY / 10.0f) * 10.0f;
    glBegin(GL_QUADS);
    for (int i = -40; i < 40; ++i) {
        for (int j = -40; j < 40; ++j) {
            float x = baseX + i * 10.0f;
            float y = baseY + j * 10.0f;
            int checker = ((int)floorf(x / 10.0f) + (int)floorf(y / 10.0f)) & 1;
            float shade = checker ? 0.30f : 0.34f;
            glColor3f(shade, shade, shade * 1.05f);
            glVertex3f(x, y, 0.0f);
            glVertex3f(x + 10.0f, y, 0.0f);
            glVertex3f(x + 10.0f, y + 10.0f, 0.0f);
            glVertex3f(x, y + 10.0f, 0.0f);
        }
    }
    glEnd();
    glColor3f(0.85f, 0.8f, 0.2f);
    glBegin(GL_LINES);
    for (int i = -40; i <= 40; ++i) {
        float y = baseY + i * 10.0f;
        glVertex3f(baseX - extent, y, 0.01f);
        glVertex3f(baseX + extent, y, 0.01f);
    }
    glEnd();
}

// Placeholder world view: the recovered collision polygons, shaded by surface and slope.
GLuint BuildWorldList() {
    GLuint list = glGenLists(1);
    glNewList(list, GL_COMPILE);
    glBegin(GL_TRIANGLES);
    for (WCollisionLink *link = collisionManager.packs0.next; link != &collisionManager.packs0;
         link = link->next) {
        WCollisionPack *pack = reinterpret_cast<WCollisionPack *>(link);
        for (int i = 0; i < pack->polygonCountC; ++i) {
            WCollisionPolygon *polygon = &pack->polygons8[i];
            float v[4][4];
            polygon->Decode(v[0]);
            int count = (polygon->flags3 & 0x10) ? 4 : 3;
            if (polygon->flags3 & 4) {
                // Barrier: vertical wall along the polygon's first edge.
                float base = v[0][2];
                glColor3f(0.55f, 0.5f, 0.45f);
                glVertex3f(v[0][0], v[0][1], base);
                glVertex3f(v[1][0], v[1][1], base);
                glVertex3f(v[1][0], v[1][1], base + 2.0f);
                glVertex3f(v[0][0], v[0][1], base);
                glVertex3f(v[1][0], v[1][1], base + 2.0f);
                glVertex3f(v[0][0], v[0][1], base + 2.0f);
                continue;
            }
            float ax = v[1][0] - v[0][0], ay = v[1][1] - v[0][1], az = v[1][2] - v[0][2];
            float bx = v[2][0] - v[0][0], by = v[2][1] - v[0][1], bz = v[2][2] - v[0][2];
            float nx = ay * bz - az * by, ny = az * bx - ax * bz, nz = ax * by - ay * bx;
            float length = sqrtf(nx * nx + ny * ny + nz * nz);
            float light = length > 0 ? 0.55f + 0.45f * fabsf(nz / length * 0.8f + nx / length * 0.2f) : 0.6f;
            float r = 0.36f, g = 0.36f, b = 0.38f;
            switch (polygon->surface2) {
            case 1: case 2: case 3: case 7: r = 0.30f; g = 0.30f; b = 0.32f; break;
            case 8: case 9: r = 0.45f; g = 0.38f; b = 0.26f; break;
            default: r = 0.5f; g = 0.5f; b = 0.47f; break;
            }
            if (polygon->surface2 > 11) {
                r = 0.32f; g = 0.42f; b = 0.28f;
            }
            glColor3f(r * light, g * light, b * light);
            glVertex3fv(v[0]); glVertex3fv(v[1]); glVertex3fv(v[2]);
            if (count == 4) {
                glVertex3fv(v[0]); glVertex3fv(v[2]); glVertex3fv(v[3]);
            }
        }
    }
    glEnd();
    glEndList();
    return list;
}

void DrawSkidMarks() {
    int count = nativeSkidCount < NATIVE_SKID_CAPACITY ? nativeSkidCount : NATIVE_SKID_CAPACITY;
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glBegin(GL_QUADS);
    for (int i = 1; i < count; ++i) {
        int index = (nativeSkidCount - count + i) % NATIVE_SKID_CAPACITY;
        NativeSkidSegment *segment = &nativeSkidSegments[index];
        NativeSkidSegment *previous = &nativeSkidSegments[(index + NATIVE_SKID_CAPACITY - 1) % NATIVE_SKID_CAPACITY];
        if (!segment->connected) continue;
        float dx = segment->centre.x - previous->centre.x, dy = segment->centre.y - previous->centre.y;
        if (dx * dx + dy * dy > 9.0f) continue;
        glColor4f(0.05f, 0.05f, 0.05f, 0.55f * segment->intensity);
        glVertex3f(previous->centre.x - previous->across.x, previous->centre.y - previous->across.y, previous->centre.z + 0.03f);
        glVertex3f(previous->centre.x + previous->across.x, previous->centre.y + previous->across.y, previous->centre.z + 0.03f);
        glVertex3f(segment->centre.x + segment->across.x, segment->centre.y + segment->across.y, segment->centre.z + 0.03f);
        glVertex3f(segment->centre.x - segment->across.x, segment->centre.y - segment->across.y, segment->centre.z + 0.03f);
    }
    glEnd();
    glDisable(GL_BLEND);
}

void Box(float x0, float x1, float y0, float y1, float z0, float z1) {
    float v[8][3] = {{x0, y0, z0}, {x1, y0, z0}, {x1, y1, z0}, {x0, y1, z0},
                     {x0, y0, z1}, {x1, y0, z1}, {x1, y1, z1}, {x0, y1, z1}};
    int faces[6][4] = {{0, 1, 2, 3}, {4, 7, 6, 5}, {0, 4, 5, 1}, {1, 5, 6, 2}, {2, 6, 7, 3}, {3, 7, 4, 0}};
    float shades[6] = {0.5f, 1.0f, 0.8f, 0.9f, 0.7f, 0.75f};
    glBegin(GL_QUADS);
    for (int f = 0; f < 6; ++f) {
        float s = shades[f];
        glColor3f(0.75f * s, 0.12f * s, 0.10f * s);
        for (int k = 0; k < 4; ++k) glVertex3fv(v[faces[f][k]]);
    }
    glEnd();
}

void DrawBody(NativeVehicle *vehicle) {
    MoverBody *body = vehicle->mover->body20;
    CarPhysicsRecord *record = vehicle->mover->record54;
    float m[16];
    memcpy(m, body->matrixD0.elements, sizeof(m));
    glPushMatrix();
    glMultMatrixf(m);
    // Body-local coordinates are relative to the centre of mass.
    float cx = body->centre1F0.x, cz = body->centre1F0.z;
    float front = record->wheels10[0].position0.x + 0.85f - cx;
    float rear = record->wheels10[3].position0.x - 0.95f - cx;
    float half = record->wheels10[0].position0.y + 0.12f;
    float floorZ = record->wheels10[0].position0.z - cz + 0.02f;
    Box(rear, front, -half, half, floorZ, floorZ + 0.62f);
    Box(rear * 0.55f, front * 0.35f, -half * 0.85f, half * 0.85f, floorZ + 0.62f, floorZ + 1.05f);
    glPopMatrix();
}

void DrawWheel(DrivetrainWheel *wheel) {
    float radius = wheel->spec14->value10;
    float spin[16], world[16], combined[16];
    memcpy(spin, wheel->matrix30.elements, sizeof(spin));
    memcpy(world, wheel->matrix70.elements, sizeof(world));
    for (int r = 0; r < 4; ++r) {
        for (int c = 0; c < 4; ++c) {
            float sum = 0.0f;
            for (int k = 0; k < 4; ++k) sum += spin[r * 4 + k] * world[k * 4 + c];
            combined[r * 4 + c] = sum;
        }
    }
    glPushMatrix();
    glMultMatrixf(combined);
    const int segments = 16;
    float halfWidth = 0.11f;
    glBegin(GL_QUAD_STRIP);
    for (int i = 0; i <= segments; ++i) {
        float a = i * 6.2831853f / segments;
        float shade = (i & 1) ? 0.10f : 0.18f;
        glColor3f(shade, shade, shade);
        glVertex3f(cosf(a) * radius, -halfWidth, sinf(a) * radius);
        glVertex3f(cosf(a) * radius, halfWidth, sinf(a) * radius);
    }
    glEnd();
    for (int side = -1; side <= 1; side += 2) {
        glBegin(GL_TRIANGLE_FAN);
        glColor3f(0.65f, 0.65f, 0.7f);
        glVertex3f(0.0f, side * halfWidth, 0.0f);
        for (int i = 0; i <= segments; ++i) {
            float a = i * 6.2831853f / segments;
            float shade = (i % 4 == 0) ? 0.95f : 0.45f;
            glColor3f(shade, shade, shade);
            glVertex3f(cosf(a) * radius * 0.7f, side * halfWidth, sinf(a) * radius * 0.7f);
        }
        glEnd();
    }
    glPopMatrix();
}

// Render frames from the exported vehicle state: the car origin (0x592400) with the body
// orientation, and the wheel frames written by 0x5921b0.
void DrawCarModel(NativeCarModel *model, NativeVehicle *vehicle) {
    MoverBody *body = vehicle->mover->body20;
    CarPhysicsRecord *record = vehicle->mover->record54;
    DrivetrainAuxiliary *state = vehicle->owner->State();
    float frame[16];
    memcpy(frame, body->matrixD0.elements, sizeof(frame));
    frame[12] = state->origin10.x;
    frame[13] = state->origin10.y;
    frame[14] = state->origin10.z;
    float wheels[4][16], spins[4][16];
    bool mirrored[4];
    for (int i = 0; i < 4; ++i) {
        DrivetrainWheel *wheel = vehicle->mover->wheels38[i];
        float *spin = wheel->matrix30.elements, *world = wheel->matrix70.elements;
        memcpy(wheels[i], world, sizeof(wheels[i]));
        for (int r = 0; r < 4; ++r) {
            for (int c = 0; c < 4; ++c) {
                float sum = 0.0f;
                for (int k = 0; k < 4; ++k) sum += spin[r * 4 + k] * world[k * 4 + c];
                spins[i][r * 4 + c] = sum;
            }
        }
        mirrored[i] = record->wheels10[i].position0.y < 0.0f;
    }
    GLfloat light[4] = {0.35f, 0.25f, 0.9f, 0.0f};
    // Scaffold lighting roughly matching the night-time baked colours of the track.
    GLfloat ambient[4] = {0.22f, 0.22f, 0.27f, 1.0f};
    GLfloat diffuse[4] = {0.5f, 0.48f, 0.45f, 1.0f};
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glLightfv(GL_LIGHT0, GL_POSITION, light);
    glLightfv(GL_LIGHT0, GL_AMBIENT, ambient);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuse);
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glEnable(GL_NORMALIZE);
    glEnable(GL_ALPHA_TEST);
    glAlphaFunc(GL_GREATER, 0.02f);
    model->Draw(frame, wheels, spins, mirrored, false);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    model->Draw(frame, wheels, spins, mirrored, true);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glDisable(GL_ALPHA_TEST);
    glDisable(GL_LIGHTING);
    glDisable(GL_COLOR_MATERIAL);
}

void SaveScreenshot(const char *path, int width, int height) {
    unsigned char *pixels = (unsigned char *)malloc(width * height * 3);
    glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels);
    FILE *file = fopen(path, "wb");
    if (file) {
        fprintf(file, "P6\n%d %d\n255\n", width, height);
        for (int y = height - 1; y >= 0; --y) fwrite(pixels + y * width * 3, 1, width * 3, file);
        fclose(file);
    }
    free(pixels);
}

}  // namespace

int main(int argc, char **argv) {
    const char *game = "Need for Speed Underground 2";
    const char *car = "SKYLINE";
    int frames = 0;
    const char *screenshot = 0;
    bool scripted = false;
    bool placeholder = false;
    float orbit = -1.0f;
    bool metric = false;
    bool noHud = false;
    int hudSkin = 0;
    const char *hudFont = 0;
    int width = 1280, height = 720;
#ifdef UG2_ENABLE_MODDING
    const char *modsDirectory = 0;
    bool noMods = false;
#endif
    int cameraOption = 3;       // profile drive camera option: 0 bumper, 1 hood, 2 close, 3 far, 4 super far
    int pauseAt = -1;           // scripted capture: open the pause menu at this frame
    const char *pauseKeys = ""; // scripted L/R/A/B/U/D/S, C=Controls, K=capture Z, M=mouse primary binding
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--game") && i + 1 < argc) game = argv[++i];
        else if (!strcmp(argv[i], "--car") && i + 1 < argc) car = argv[++i];
        else if (!strcmp(argv[i], "--frames") && i + 1 < argc) frames = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--screenshot") && i + 1 < argc) screenshot = argv[++i];
        else if (!strcmp(argv[i], "--scripted")) scripted = true;
        else if (!strcmp(argv[i], "--placeholder")) placeholder = true;
        else if (!strcmp(argv[i], "--orbit") && i + 1 < argc) orbit = (float)atof(argv[++i]);
        else if (!strcmp(argv[i], "--metric")) metric = true;
        else if (!strcmp(argv[i], "--no-hud")) noHud = true;
        else if (!strcmp(argv[i], "--hud-skin") && i + 1 < argc) hudSkin = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--hud-font") && i + 1 < argc) hudFont = argv[++i];
        else if (!strcmp(argv[i], "--width") && i + 1 < argc) width = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--height") && i + 1 < argc) height = atoi(argv[++i]);
#ifdef UG2_ENABLE_MODDING
        else if (!strcmp(argv[i], "--mods-dir") && i + 1 < argc) modsDirectory = argv[++i];
        else if (!strcmp(argv[i], "--no-mods")) noMods = true;
#endif
        else if (!strcmp(argv[i], "--camera") && i + 1 < argc) cameraOption = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--pause-at") && i + 1 < argc) pauseAt = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--pause-keys") && i + 1 < argc) pauseKeys = argv[++i];
    }
    if (width < 320 || height < 240 || width > 8192 || height > 8192) {
        fprintf(stderr, "ug2demo: invalid resolution (320x240 through 8192x8192)\n"); return 1;
    }
    unsigned short control = 0x027f;
    __asm__ volatile("fldcw %0" : : "m"(control));
    char error[512];
    if (!InitialiseNativeRuntime(game, error, sizeof(error))) {
        fprintf(stderr, "ug2demo: %s\n", error);
        return 1;
    }
#ifdef UG2_ENABLE_MODDING
    NativeModSession mods(modsDirectory, noMods);
#endif
    NativeTrack track;
    if (!LoadNativeTrack(game, "L4RA", &track, error, sizeof(error))) {
        fprintf(stderr, "ug2demo: %s\n", error);
        return 1;
    }
    float spawn[4] = {track.spawn[0], track.spawn[1], track.spawn[2] + 0.45f, track.heading};
    static NativeVehicle vehicle;
#ifdef UG2_ENABLE_MODDING
    mods.vehicle = &vehicle;
#endif
    if (!CreateNativeVehicle(&vehicle, game, car, spawn[0], spawn[1], spawn[2], spawn[3], error, sizeof(error))) {
        fprintf(stderr, "ug2demo: %s\n", error);
        return 1;
    }
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) != 0) {
        fprintf(stderr, "ug2demo: %s\n", SDL_GetError());
        return 1;
    }

    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_Window *window = SDL_CreateWindow("UG2 recovered vehicle demo", SDL_WINDOWPOS_CENTERED,
                                          SDL_WINDOWPOS_CENTERED, width, height, SDL_WINDOW_OPENGL);
    if (!window) {
        fprintf(stderr, "ug2demo: %s\n", SDL_GetError());
        return 1;
    }
    SDL_GLContext context = SDL_GL_CreateContext(window);
    if (!context) { fprintf(stderr, "ug2demo: %s\n", SDL_GetError()); SDL_DestroyWindow(window); SDL_Quit(); return 1; }
    SDL_GL_GetDrawableSize(window, &width, &height);
    SDL_GL_SetSwapInterval(frames ? 0 : 1);
    glEnable(GL_DEPTH_TEST);
    GLuint worldList = BuildWorldList();
    NativeTextureCache textures;
    NativeTrackModel trackModel;
    bool haveTrack = false;
    if (!placeholder) {
        haveTrack = trackModel.Load(game, "L4RA", &textures, error, sizeof(error));
        if (!haveTrack) fprintf(stderr, "ug2demo: %s; drawing collision polygons\n", error);
        else fprintf(stderr, "ug2demo: L4RA scenery: %d instances of %d solids\n", trackModel.InstanceCount(),
                     trackModel.SolidCount());
        if (haveTrack && getenv("UG2_DEBUG_AT")) {
            float x = spawn[0], y = spawn[1];
            sscanf(getenv("UG2_DEBUG_AT"), "%f %f", &x, &y);
            fprintf(stderr, "ug2demo: instances at %.1f %.1f (spawn z %.1f)\n", x, y, spawn[2]);
            trackModel.DebugAt(x, y);
        }
    }
    NativeCarModel carModel;
    bool haveModel = false;
    if (!placeholder) {
        haveModel = carModel.Load(game, car, &textures, error, sizeof(error));
        if (!haveModel) fprintf(stderr, "ug2demo: %s; drawing placeholder car\n", error);
        else fprintf(stderr, "ug2demo: %s model: %d solids, %d of %d parts textured, %d textures\n", car,
                     carModel.SolidCount(), carModel.TexturedParts(), carModel.Parts(), (int)textures.Count());
    }
    NativeHud hud;
    bool haveHud = false;
    if (!placeholder && !noHud) {
        haveHud = hud.Load(game, &textures, hudFont, error, sizeof(error));
        if (!haveHud) fprintf(stderr, "ug2demo: %s; HUD disabled\n", error);
        if (haveHud && getenv("UG2_DEBUG_HUD")) hud.Describe();
        if (haveHud) hud.Configure(&vehicle, hudSkin);
    }
    NativePause pause;
    bool havePause = false;
    if (!placeholder) {
        havePause = pause.Load(game, &textures, hudFont, error, sizeof(error));
        if (!havePause) fprintf(stderr, "ug2demo: %s; pause menu disabled, Esc quits\n", error);
    }
    float lastGood[4] = {spawn[0], spawn[1], spawn[2], spawn[3]};
    int airborneFrames = 0;
    Camera camera = {{-8.0f, 0.0f, 3.0f}, {0.0f, 0.0f, 0.5f}};
    InitialiseDriveCameraStatics();
    DriveCameraMover driveCamera;
    DriveCameraView cameraView;
    memset(&cameraView, 0, sizeof(cameraView));
    bool lookBack = false;
    if (cameraOption < 0 || cameraOption > 4) cameraOption = 3;
    if (havePause) pause.SetViewSettings(metric, cameraOption);
    // Player::SetCameraState (0x5f1fd0) builds the mover for the car with the option's type.
    driveCamera.Construct(vehicle.owner, &vehicle.cameraSettings, 1, DriveCameraTypeForOption(cameraOption),
                          false, false, false);
    float steer = 0.0f;
    Uint64 previous = SDL_GetPerformanceCounter();
    NativeUiInput uiInput;
    bool running = true;
    const bool scriptedPauseCapture = frames > 0 && pauseAt >= 0 && pauseKeys[0] != 0;
    for (int frame = 0; running && (frames == 0 || frame < frames); ++frame) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) running = false;
            // Finite UI captures receive their scripted actions without desktop/device interference.
            if (scriptedPauseCapture && (event.type == SDL_MOUSEMOTION || event.type == SDL_MOUSEBUTTONDOWN ||
                event.type == SDL_MOUSEBUTTONUP || event.type == SDL_MOUSEWHEEL || event.type == SDL_KEYDOWN ||
                event.type == SDL_KEYUP || event.type == SDL_CONTROLLERBUTTONDOWN || event.type == SDL_CONTROLLERBUTTONUP ||
                event.type == SDL_CONTROLLERAXISMOTION)) continue;
            if (havePause && uiInput.Owns(event) && NativeControlsCapturePad(event)) continue;
            bool padGameplay = uiInput.Owns(event) && event.type == SDL_CONTROLLERBUTTONDOWN && !(havePause && pause.Active());
            if (havePause && !padGameplay && uiInput.Event(event, window, pause)) continue;
            if (event.type == SDL_KEYDOWN && havePause && pause.Active()) {
                // Scaffold key mapping for the FEng pad messages while the menu is up.
                if (pause.CaptureKey(event.key.keysym.scancode)) continue;
                SDL_Keycode key = event.key.keysym.sym;
                if (key == SDLK_TAB) { pause.Key(kPauseKeyDevice); continue; }
                if (key == SDLK_F2) { pause.OpenControls(); continue; }
                if (key == SDLK_LEFT || key == SDLK_a) pause.Key(kPauseKeyLeft);
                else if (key == SDLK_RIGHT || key == SDLK_d) pause.Key(kPauseKeyRight);
                else if (key == SDLK_UP || key == SDLK_w) pause.Key(kPauseKeyUp);
                else if (key == SDLK_DOWN || key == SDLK_s) pause.Key(kPauseKeyDown);
                else if (key == SDLK_RETURN || key == SDLK_KP_ENTER || key == SDLK_SPACE) pause.Key(kPauseKeyAccept);
                else if (key == SDLK_p || key == SDLK_PAUSE) pause.Key(kPauseKeyStart);
                else if (key == SDLK_ESCAPE || key == SDLK_BACKSPACE) pause.Key(kPauseKeyBack);
                continue;
            }
            if ((event.type == SDL_KEYDOWN && NativeControlKey(12, event.key.keysym.scancode) && !event.key.repeat) ||
                (padGameplay && NativeGamepadBindings::Get().Button(12, event.cbutton.button))) {
                // JOY_EVENT_PAUSE seam: the game flow that pushes UI_Pause.fng is not recovered.
                if (havePause) pause.Open();
                else running = false;
            }
            if ((event.type == SDL_KEYDOWN && NativeControlKey(14, event.key.keysym.scancode) && !event.key.repeat) ||
                (padGameplay && NativeGamepadBindings::Get().Button(14, event.cbutton.button))) {
                DestroyNativeVehicle(&vehicle);
                if (!CreateNativeVehicle(&vehicle, game, car, spawn[0], spawn[1], spawn[2], spawn[3], error,
                                         sizeof(error))) {
                    fprintf(stderr, "ug2demo: %s\n", error);
                    running = false;
                } else {
                    driveCamera.Destroy();
                    driveCamera.Construct(vehicle.owner, &vehicle.cameraSettings, 1,
                                          DriveCameraTypeForOption(cameraOption), false, lookBack, false);
                    if (haveHud) hud.Configure(&vehicle, hudSkin);
                }
            }
            if ((event.type == SDL_KEYDOWN && NativeControlKey(10, event.key.keysym.scancode) && !event.key.repeat) ||
                (padGameplay && NativeGamepadBindings::Get().Button(10, event.cbutton.button))) {
                // JOY_EVENT_CAMERA_POV_CHANGE: far -> close -> hood -> bumper -> far.
                cameraOption = (cameraOption + 3) % 4;
                driveCamera.SetType(DriveCameraTypeForOption(cameraOption));
                if (havePause) pause.SetViewSettings(metric, cameraOption);
            }
        }
        if (havePause && !scriptedPauseCapture) uiInput.Update(pause);
        Uint64 now = SDL_GetPerformanceCounter();
        float duration = (float)(now - previous) / (float)SDL_GetPerformanceFrequency();
        previous = now;
        if (frames || scripted) duration = 1.0f / 60.0f;
        if (duration > 0.05f) duration = 0.05f;
        if (havePause) {
            if (frame == pauseAt) pause.Open();
            if (pauseAt >= 0 && frame > pauseAt && (frame - pauseAt) % 30 == 0) {
                size_t index = (frame - pauseAt) / 30 - 1;
                if (index < strlen(pauseKeys)) {
                    char c = pauseKeys[index];
                    if (c == 'L') pause.Key(kPauseKeyLeft);
                    else if (c == 'R') pause.Key(kPauseKeyRight);
                    else if (c == 'U') pause.Key(kPauseKeyUp);
                    else if (c == 'D') pause.Key(kPauseKeyDown);
                    else if (c == 'S') pause.Key(kPauseKeyStart);
                    else if (c == 'A') pause.Key(kPauseKeyAccept);
                    else if (c == 'B') pause.Key(kPauseKeyBack);
                    else if (c == 'C') pause.OpenControls();
                    else if (c == 'G') pause.Key(kPauseKeyDevice);
                    else if (c == 'H') { SDL_Event pad={};pad.type=SDL_CONTROLLERBUTTONDOWN;pad.cbutton.button=SDL_CONTROLLER_BUTTON_X;NativeControlsCapturePad(pad); }
                    else if (c == 'J') { SDL_Event pad={};pad.type=SDL_CONTROLLERAXISMOTION;pad.caxis.axis=SDL_CONTROLLER_AXIS_LEFTX;pad.caxis.value=-32768;NativeControlsCapturePad(pad); }
                    else if (c == 'K') pause.CaptureKey(SDL_SCANCODE_Z);
                    else if (c == 'M' || c == 'T' || c == 'V') {
                        int w, h; SDL_GetWindowSize(window, &w, &h);
                        // Scripted input is a fixture independent of desktop window focus.
                        SDL_Event focus={};focus.type=SDL_WINDOWEVENT;focus.window.event=SDL_WINDOWEVENT_FOCUS_GAINED;
                        uiInput.Event(focus,window,pause);
                        SDL_Event pointer = {}; pointer.type = c == 'V' ? SDL_MOUSEMOTION : SDL_MOUSEBUTTONDOWN;
                        if(c=='V') pointer.motion.state=SDL_BUTTON_LMASK;
                        pointer.button.button = SDL_BUTTON_LEFT;
                        pointer.button.x = (int)(w * .5f + (c=='M' ? -30 : 125) * h / 480.0f);
                        pointer.button.y = (int)(h * .5f + (c=='M' ? -90 : -125) * h / 480.0f);
                        uiInput.Event(pointer, window, pause);
                    }
                }
            }
            pause.Update(duration);
            metric = pause.Metric();
            if (cameraOption != pause.CameraOption()) {
                cameraOption = pause.CameraOption();
                driveCamera.SetType(DriveCameraTypeForOption(cameraOption));
            }
            NativePauseRequest request = pause.TakeRequest();
            if (request == kPauseRequestQuit) {
                running = false;
            } else if (request == kPauseRequestRestart) {
                DestroyNativeVehicle(&vehicle);
                if (!CreateNativeVehicle(&vehicle, game, car, spawn[0], spawn[1], spawn[2], spawn[3], error,
                                         sizeof(error))) {
                    fprintf(stderr, "ug2demo: %s\n", error);
                    running = false;
                } else {
                    driveCamera.Destroy();
                    driveCamera.Construct(vehicle.owner, &vehicle.cameraSettings, 1,
                                          DriveCameraTypeForOption(cameraOption), false, lookBack, false);
                    if (haveHud) hud.Configure(&vehicle, hudSkin);
                }
            }
            if (!running) break;
        }
        bool paused = havePause && pause.Active();
        const Uint8 *keys = SDL_GetKeyboardState(0);
        float throttle = NativeControlPressed(1, keys) ? 1.0f : 0.0f;
        float brake = NativeControlPressed(2, keys) ? 1.0f : 0.0f;
        float handbrake = NativeControlPressed(5, keys) ? 1.0f : 0.0f;
        throttle = fmaxf(throttle, uiInput.Action(1));
        brake = fmaxf(brake, uiInput.Action(2));
        handbrake = fmaxf(handbrake, uiInput.Action(5));
        float target = uiInput.Action(3) - uiInput.Action(4);
        if (NativeControlPressed(3, keys)) target += 1.0f;
        if (NativeControlPressed(4, keys)) target -= 1.0f;
        target = fmaxf(-1.0f, fminf(1.0f, target));
        if (scripted) {
            float t = frame / 60.0f;
            throttle = t > 0.5f && t < 9.0f ? 1.0f : 0.0f;
            target = t > 4.0f && t < 7.0f ? 0.5f : 0.0f;
            brake = t > 10.0f ? 1.0f : 0.0f;
            if (getenv("UG2_STEER")) {
                // Scaffold test input: constant steering from t = 2 s (same as ug2sim).
                target = t > 2.0f ? (float)atof(getenv("UG2_STEER")) : 0.0f;
            }
        }
        steer = target;
        if (!paused) {
            SetNativeControls(&vehicle, throttle, brake, handbrake, steer);
            StepNativeVehicle(&vehicle, duration);
        }
#ifdef UG2_ENABLE_MODDING
        if (NativeModsFailed()) break;
#endif
        {
            // Scaffold respawn: a car that still leaves the collision coverage (e.g. over a
            // barrier the track data does not close) is put back where it last had all four
            // wheels down.
            DrivetrainAuxiliary *state = vehicle.owner->State();
            MoverBody *b = vehicle.mover->body20;
            if (state->value3E4 == 4 && state->value3F0 > 2.0f) {
                if (frame % 30 == 0) {
                    lastGood[0] = b->position20.x;
                    lastGood[1] = b->position20.y;
                    lastGood[2] = b->position20.z + 0.3f;
                    lastGood[3] = atan2f(b->matrix30.elements[1], b->matrix30.elements[0]);
                }
                airborneFrames = 0;
            } else if (state->value3E4 == 0) {
                ++airborneFrames;
            }
            if (airborneFrames > 150 && !paused) {
                DestroyNativeVehicle(&vehicle);
                CreateNativeVehicle(&vehicle, game, car, lastGood[0], lastGood[1], lastGood[2], lastGood[3], error,
                                    sizeof(error));
                driveCamera.Destroy();
                driveCamera.Construct(vehicle.owner, &vehicle.cameraSettings, 1,
                                      DriveCameraTypeForOption(cameraOption), false, lookBack, false);
                if (haveHud) hud.Configure(&vehicle, hudSkin);
                airborneFrames = 0;
            }
        }

#ifdef UG2_ENABLE_MODDING
        if (NativeModsFailed()) break;
#endif
        SDL_GL_GetDrawableSize(window, &width, &height);
        MoverBody *body = vehicle.mover->body20;
        float *m = body->matrixD0.elements;
        float forward[2] = {m[0], m[1]};
        float length = sqrtf(forward[0] * forward[0] + forward[1] * forward[1]);
        if (length > 1e-3f) {
            forward[0] /= length;
            forward[1] /= length;
        }
        {
            bool wantLookBack = !paused && (NativeControlPressed(9, keys) || uiInput.Action(9) > .5f);
            if (wantLookBack != lookBack) {
                lookBack = wantLookBack;
                driveCamera.SetLookBack(lookBack);
            }
        }
        if (!paused) driveCamera.Update(duration, drivetrainClock, &cameraView);
        if (getenv("UG2_DEBUG_CAMERA") && frame % 30 == 0) {
            DrivetrainAuxiliary *cs = vehicle.owner->State();
            fprintf(stderr, "cam f%d speed %.1f accel %.2f %.2f %.2f eyeLocal %.2f %.2f %.2f fov %u\n", frame,
                    cs->value3F0 * 3.6f, cs->value40.x, cs->value40.y, cs->value40.z,
                    driveCamera.eye38->channels0[0].value0, driveCamera.eye38->channels0[1].value0,
                    driveCamera.eye38->channels0[2].value0, (unsigned)cameraView.fov);
        }
        float up[3] = {cameraView.up[0], cameraView.up[1], cameraView.up[2]};
        if (orbit >= 0.0f) {
            // Inspection camera for screenshots: fixed angle around the car.
            float a = atan2f(forward[1], forward[0]) + orbit * 3.14159265f / 180.0f;
            camera.position[0] = m[12] + cosf(a) * 5.0f;
            camera.position[1] = m[13] + sinf(a) * 5.0f;
            camera.position[2] = m[14] + 1.6f;
            camera.target[0] = m[12];
            camera.target[1] = m[13];
            camera.target[2] = m[14] + 0.6f;
            up[0] = 0.0f; up[1] = 0.0f; up[2] = 1.0f;
        } else {
            memcpy(camera.position, cameraView.eye, sizeof(camera.position));
            memcpy(camera.target, cameraView.target, sizeof(camera.target));
        }

        CameraProjection projection = NativeProjection(cameraView.fov, cameraView.nearPlane, width, height);
#ifdef UG2_ENABLE_MODDING
        if (NativeModsFailed()) break;
#endif
        glViewport(0, 0, width, height);
        // Scaffold horizon colour sampled from the SKY_SUNRISE dome; the time-of-day clear and
        // fog colours are not recovered.
        glClearColor(haveTrack ? 0.15f : 0.45f, haveTrack ? 0.12f : 0.62f, haveTrack ? 0.10f : 0.85f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        UgFrustum(projection);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        LookAt(camera.position, camera.target, up);
        if (haveTrack) {
            float view[3] = {camera.target[0] - camera.position[0], camera.target[1] - camera.position[1],
                             camera.target[2] - camera.position[2]};
            float norm = sqrtf(view[0] * view[0] + view[1] * view[1] + view[2] * view[2]);
            for (int i = 0; i < 3; ++i) view[i] /= norm > 1e-3f ? norm : 1.0f;
            trackModel.Draw(camera.position, view, 2800.0f);
        }
        else glCallList(worldList);
        DrawSkidMarks();
        if (cameraView.attachedToCar && orbit < 0.0f) {
            // Camera +0x280 holds the car for the bumper and driver types: the car is not drawn.
        } else if (haveModel) {
            DrawCarModel(&carModel, &vehicle);
        } else {
            DrawBody(&vehicle);
            for (int i = 0; i < 4; ++i) DrawWheel(vehicle.mover->wheels38[i]);
        }

#ifdef UG2_ENABLE_MODDING
        NativeModsEvent(UG2_POST_WORLD_DRAW, duration, width, height);
#endif
        DrivetrainAuxiliary *state = vehicle.owner->State();
        float rpm = vehicle.mover->engine48->state14 * engineValueScale;
        int gear = vehicle.mover->drivetrain4C->ratioIndex;
        char title[160];
        snprintf(title, sizeof(title), "UG2 recovered vehicle demo - %s  %5.1f km/h  %5.0f rpm  gear %s",
                 vehicle.carName, state->value3F0 * 3.6f, rpm,
                 gear == 0 ? "R" : (gear == 1 ? "N" : (char[]){(char)('0' + gear - 1), 0}));
        SDL_SetWindowTitle(window, title);
        if (haveHud) {
            hud.Update(&vehicle, metric, paused ? 0.0f : duration);
            hud.Draw(width, height);
        }
        if (havePause) pause.Draw(width, height);
#ifdef UG2_ENABLE_MODDING
        NativeModsEvent(UG2_POST_HUD_DRAW, duration, width, height);
        if (NativeModsFailed()) break;
#endif
        glEnable(GL_DEPTH_TEST);
        if (screenshot && frames && frame == frames - 1) SaveScreenshot(screenshot, width, height);
        if (haveTrack && frames && frame == frames - 1 && getenv("UG2_DEBUG_PICK")) {
            // "x y" in screenshot pixels, top-left origin.
            int px = 0, py = 0;
            sscanf(getenv("UG2_DEBUG_PICK"), "%d %d", &px, &py);
            glMatrixMode(GL_PROJECTION);
            glLoadIdentity();
            UgFrustum(projection);
            glMatrixMode(GL_MODELVIEW);
            glLoadIdentity();
            LookAt(camera.position, camera.target, up);
            float view[3] = {camera.target[0] - camera.position[0], camera.target[1] - camera.position[1],
                             camera.target[2] - camera.position[2]};
            float norm = sqrtf(view[0] * view[0] + view[1] * view[1] + view[2] * view[2]);
            for (int i = 0; i < 3; ++i) view[i] /= norm > 1e-3f ? norm : 1.0f;
            trackModel.DebugPick(camera.position, view, 2800.0f, px, height - 1 - py);
        }
        SDL_GL_SwapWindow(window);
    }
#ifdef UG2_ENABLE_MODDING
    bool modFailure = NativeModsFailed();
    mods.Finish();
#else
    bool modFailure = false;
    DestroyNativeVehicle(&vehicle);
#endif
    driveCamera.Destroy();
    uiInput.Close();
    SDL_GL_DeleteContext(context);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return modFailure ? 3 : 0;
}
