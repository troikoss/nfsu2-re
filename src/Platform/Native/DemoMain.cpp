// Windowed driving demo for the native build. Everything in this file is platform
// scaffolding: window, input, camera and placeholder rendering. Vehicle behaviour comes
// from the recovered UG2 sources through NativeVehicle.
#include "NativeVehicle.h"

#include <SDL2/SDL.h>
#include <GL/gl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern float engineValueScale;

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

void Perspective(float fovY, float aspect, float nearPlane, float farPlane) {
    float top = nearPlane * tanf(fovY * 0.5f * 3.14159265f / 180.0f);
    glFrustum(-top * aspect, top * aspect, -top, top, nearPlane, farPlane);
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
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--game") && i + 1 < argc) game = argv[++i];
        else if (!strcmp(argv[i], "--car") && i + 1 < argc) car = argv[++i];
        else if (!strcmp(argv[i], "--frames") && i + 1 < argc) frames = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--screenshot") && i + 1 < argc) screenshot = argv[++i];
        else if (!strcmp(argv[i], "--scripted")) scripted = true;
    }
    unsigned short control = 0x027f;
    __asm__ volatile("fldcw %0" : : "m"(control));
    char error[512];
    if (!InitialiseNativeRuntime(game, error, sizeof(error))) {
        fprintf(stderr, "ug2demo: %s\n", error);
        return 1;
    }
    NativeVehicle vehicle;
    if (!CreateNativeVehicle(&vehicle, game, car, 0.0f, 0.0f, 0.45f, error, sizeof(error))) {
        fprintf(stderr, "ug2demo: %s\n", error);
        return 1;
    }
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "ug2demo: %s\n", SDL_GetError());
        return 1;
    }
    const int width = 1280, height = 720;
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_Window *window = SDL_CreateWindow("UG2 recovered vehicle demo", SDL_WINDOWPOS_CENTERED,
                                          SDL_WINDOWPOS_CENTERED, width, height, SDL_WINDOW_OPENGL);
    if (!window) {
        fprintf(stderr, "ug2demo: %s\n", SDL_GetError());
        return 1;
    }
    SDL_GLContext context = SDL_GL_CreateContext(window);
    SDL_GL_SetSwapInterval(1);
    glEnable(GL_DEPTH_TEST);
    Camera camera = {{-8.0f, 0.0f, 3.0f}, {0.0f, 0.0f, 0.5f}};
    float steer = 0.0f;
    Uint64 previous = SDL_GetPerformanceCounter();
    bool running = true;
    for (int frame = 0; running && (frames == 0 || frame < frames); ++frame) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) running = false;
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) running = false;
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_r) {
                if (!CreateNativeVehicle(&vehicle, game, car, 0.0f, 0.0f, 0.45f, error, sizeof(error))) {
                    fprintf(stderr, "ug2demo: %s\n", error);
                    running = false;
                }
            }
        }
        Uint64 now = SDL_GetPerformanceCounter();
        float duration = (float)(now - previous) / (float)SDL_GetPerformanceFrequency();
        previous = now;
        if (frames || scripted) duration = 1.0f / 60.0f;
        if (duration > 0.05f) duration = 0.05f;
        const Uint8 *keys = SDL_GetKeyboardState(0);
        float throttle = (keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_UP]) ? 1.0f : 0.0f;
        float brake = (keys[SDL_SCANCODE_S] || keys[SDL_SCANCODE_DOWN]) ? 1.0f : 0.0f;
        float handbrake = keys[SDL_SCANCODE_SPACE] ? 1.0f : 0.0f;
        float target = 0.0f;
        if (keys[SDL_SCANCODE_A] || keys[SDL_SCANCODE_LEFT]) target += 1.0f;
        if (keys[SDL_SCANCODE_D] || keys[SDL_SCANCODE_RIGHT]) target -= 1.0f;
        if (scripted) {
            float t = frame / 60.0f;
            throttle = t > 0.5f && t < 9.0f ? 1.0f : 0.0f;
            target = t > 4.0f && t < 7.0f ? 0.5f : 0.0f;
            brake = t > 10.0f ? 1.0f : 0.0f;
        }
        float rate = 2.5f * duration;
        steer += (target - steer) > rate ? rate : ((target - steer) < -rate ? -rate : target - steer);
        SetNativeControls(&vehicle, throttle, brake, handbrake, steer);
        StepNativeVehicle(&vehicle, duration);

        MoverBody *body = vehicle.mover->body20;
        float *m = body->matrixD0.elements;
        float forward[2] = {m[0], m[1]};
        float length = sqrtf(forward[0] * forward[0] + forward[1] * forward[1]);
        if (length > 1e-3f) {
            forward[0] /= length;
            forward[1] /= length;
        }
        float desired[3] = {m[12] - forward[0] * 7.5f, m[13] - forward[1] * 7.5f, m[14] + 2.6f};
        float blend = 1.0f - expf(-duration * 4.0f);
        for (int i = 0; i < 3; ++i) camera.position[i] += (desired[i] - camera.position[i]) * blend;
        camera.target[0] = m[12];
        camera.target[1] = m[13];
        camera.target[2] = m[14] + 0.6f;

        glViewport(0, 0, width, height);
        glClearColor(0.45f, 0.62f, 0.85f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        Perspective(60.0f, (float)width / height, 0.2f, 1000.0f);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        float up[3] = {0.0f, 0.0f, 1.0f};
        LookAt(camera.position, camera.target, up);
        DrawGround(m[12], m[13]);
        DrawBody(&vehicle);
        for (int i = 0; i < 4; ++i) DrawWheel(vehicle.mover->wheels38[i]);

        DrivetrainAuxiliary *state = vehicle.owner->State();
        float rpm = vehicle.mover->engine48->state14 * engineValueScale;
        int gear = vehicle.mover->drivetrain4C->ratioIndex;
        char title[160];
        snprintf(title, sizeof(title), "UG2 recovered vehicle demo - %s  %5.1f km/h  %5.0f rpm  gear %s",
                 vehicle.carName, state->value3F0 * 3.6f, rpm,
                 gear == 0 ? "R" : (gear == 1 ? "N" : (char[]){(char)('0' + gear - 1), 0}));
        SDL_SetWindowTitle(window, title);
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(0, width, 0, height, -1, 1);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        glDisable(GL_DEPTH_TEST);
        float rpmFraction = rpm / 9000.0f;
        glBegin(GL_QUADS);
        glColor3f(0.1f, 0.1f, 0.1f);
        glVertex2f(20, 20); glVertex2f(420, 20); glVertex2f(420, 44); glVertex2f(20, 44);
        glColor3f(rpmFraction > 0.85f ? 0.9f : 0.2f, 0.8f, 0.2f);
        glVertex2f(24, 24); glVertex2f(24 + 392 * rpmFraction, 24);
        glVertex2f(24 + 392 * rpmFraction, 40); glVertex2f(24, 40);
        float speedFraction = state->value3F0 * 3.6f / 300.0f;
        glColor3f(0.1f, 0.1f, 0.1f);
        glVertex2f(20, 50); glVertex2f(420, 50); glVertex2f(420, 74); glVertex2f(20, 74);
        glColor3f(0.2f, 0.5f, 0.95f);
        glVertex2f(24, 54); glVertex2f(24 + 392 * speedFraction, 54);
        glVertex2f(24 + 392 * speedFraction, 70); glVertex2f(24, 70);
        glEnd();
        glEnable(GL_DEPTH_TEST);
        if (screenshot && frames && frame == frames - 1) SaveScreenshot(screenshot, width, height);
        SDL_GL_SwapWindow(window);
    }
    SDL_GL_DeleteContext(context);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
