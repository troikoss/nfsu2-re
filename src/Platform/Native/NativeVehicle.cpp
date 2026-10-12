#include "NativeVehicle.h"
#ifdef UG2_ENABLE_MODDING
#include "NativeModBridge.h"
#endif
#include "../../Speed/Indep/Libs/bWare/Strings.h"
#include "../../Speed/Indep/Src/Physics/ForceFeedback.h"
#include "OriginalImage.h"
#include "../../Speed/Indep/Src/Physics/VehicleIntegrator.h"
#include "../../Speed/Indep/Src/Physics/VehicleAllocation.h"
#include "../../Speed/Indep/Src/Physics/PhysicsUpgradeSource.h"
#include "../../Speed/Indep/Src/World/WCollision.h"

#include <math.h>
#include <new>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <time.h>
#include <vector>

void RunPhysicsStaticInitializers();

void NativeTrap(const char *what) {
    fprintf(stderr, "ug2demo: reached unrecovered original code: %s\n", what);
    abort();
}

// ---------------------------------------------------------------------------
// Integration block vtables. The original classes install these tables:
// body 0x7a20c4, wheel 0x7a20e8, arm 0x7a210c, engine 0x7a235c, drivetrain 0x7a2508.
// Each native slot forwards to the recovered method; tiny slot bodies are noted.

extern unsigned char *carTypeInfoTable; // original 0x8a1ccc

namespace {

template <class Self>
class BlockBase : public IntegrationStateBlock {
protected:
    Self *self() { return reinterpret_cast<Self *>(this); }
public:
    void UnknownSlot0() { NativeTrap("integration block destructor"); }
    void UnknownSlot8() { NativeTrap("integration block state copy-out"); }
    void UnknownSlot16() { NativeTrap("integration block state copy-in"); }
    void UnknownSlot20() {}               // 0x4022c0: empty.
    void BeforeIntegrate() {}             // 0x50d510 returns 0; body 0x5a80d0 returns a static pointer.
};

class BodyBlock : public BlockBase<MoverBody> {
public:
    int GetStateCount() { return 32; }                               // 0x5a80b0
    float *GetStateValues() { return &self()->position20.x; }        // 0x5a80c0
    void WriteDerivatives(float *output) { self()->WriteDerivative(output); }
    void UpdateAfterIntegration() { self()->PostUpdate(); }
};

class WheelBlock : public BlockBase<DrivetrainWheel> {
public:
    int GetStateCount() { return 2; }                                // 0x408cd0
    float *GetStateValues() { return &self()->angle24; }             // 0x5a8230
    void WriteDerivatives(float *output) { self()->WriteBlockDerivative(output); }
    void UpdateAfterIntegration() { self()->UpdateAfterIntegration(); }
};

class ArmBlock : public BlockBase<MoverArm> {
public:
    int GetStateCount() { return 2; }                                // 0x408cd0
    float *GetStateValues() { return &self()->travel1C; }            // 0x5a8740
    void WriteDerivatives(float *output) { self()->WriteDerivative(output); }
    void UpdateAfterIntegration() { self()->UpdateAfterIntegration(); }
};

class EngineBlock : public BlockBase<Engine> {
public:
    int GetStateCount() { return 1; }                                // 0x40cb20
    float *GetStateValues() { return &self()->state14; }             // 0x5aa700
    void WriteDerivatives(float *output) { self()->WriteDerivative(output); }
    void UpdateAfterIntegration() { self()->UpdateAfterIntegration(); }
};

class DrivetrainBlock : public BlockBase<Drivetrain> {
public:
    int GetStateCount() { return 1; }                                // 0x40cb20
    float *GetStateValues() { return &self()->state14; }             // 0x5aa700
    void WriteDerivatives(float *output) { self()->WriteDerivative(output); }
    void UpdateAfterIntegration() { self()->UpdateAfterIntegration(); }
};

// PhysicsMover integrator subobject at mover +8, vtable 0x7a25d4.
class MoverIntegrator : public VehicleIntegrator {
    VehicleMover *mover() { return reinterpret_cast<VehicleMover *>(reinterpret_cast<char *>(this) - 8); }
public:
    void UnknownSlot0() { NativeTrap("PhysicsMover integrator slot 0 (0x5b04c0)"); }
    void AfterIntegrate() { mover()->FinishIntegration(); }          // 0x5b04d0
};

template <class Shim>
void *AllocateBlock(size_t size) {
    // Original slot pools (0x440e50) are not zero-filled; zeroing keeps runs repeatable.
    void *memory = calloc(1, size);
    new (memory) Shim;
    return memory;
}

int raceStatusScaffold[0x400];
// Scaffolds for objects created by race setup (0x60a6d4 / 0x5ff537): a context without
// an entry at +0xa0 (variations enabled) and a random source with a fixed seed.
DrivetrainContext contextScaffold;
DrivetrainRandom randomScaffold;

}  // namespace

MoverBody *NewMoverBody(MoverBodySpec *spec) {
    MoverBody *body = static_cast<MoverBody *>(AllocateBlock<BodyBlock>(0x210));
    body->Construct(spec);
    return body;
}

AckermannSteering *NewAckermannSteering(CarPhysicsRecord *record) {
    AckermannSteering *steering = static_cast<AckermannSteering *>(calloc(1, 0x94));
    steering->Construct(record);
    return steering;
}

DrivetrainWheel *NewDrivetrainWheel(DrivetrainWheelSpec *spec, WheelCorrectionSpec *correction,
                                    WheelPreparationSpec *preparation, DrivetrainAuxiliary *auxiliary) {
    DrivetrainWheel *wheel = static_cast<DrivetrainWheel *>(AllocateBlock<WheelBlock>(0x1a0));
    wheel->Construct(spec, correction, preparation, auxiliary);
    return wheel;
}

MoverArm *NewMoverArm(int index, MoverBody *body, DrivetrainWheel *wheel, AckermannSteering *steering,
                      SuspensionSpec *spec) {
    MoverArm *arm = static_cast<MoverArm *>(AllocateBlock<ArmBlock>(0xb8));
    arm->Construct(index, body, wheel, steering, spec);
    return arm;
}

Engine *NewEngine(EngineSpec *spec, EngineAttributeOwner *owner) {
    Engine *engine = static_cast<Engine *>(AllocateBlock<EngineBlock>(0xb0));
    engine->Construct(spec, owner);
    return engine;
}

Drivetrain *NewDrivetrain(DrivetrainSpec *spec, DrivetrainTuning *tuning, DrivetrainComponent *engine,
                          DrivetrainWheel **wheels, DrivetrainAuxiliary *auxiliary) {
    Drivetrain *drivetrain = static_cast<Drivetrain *>(AllocateBlock<DrivetrainBlock>(0xf8));
    drivetrain->Construct(spec, tuning, engine, wheels, auxiliary);
    return drivetrain;
}

MoverAero *NewMoverAero(MoverAeroSpec *spec, MoverBody *body) {
    MoverAero *aero = static_cast<MoverAero *>(calloc(1, 0x38));
    aero->Construct(spec, body);
    return aero;
}


// D3DX runtime library (statically linked in the original, dispatched via 0x803e74).
// Row-vector product: output = left * right; operands may alias the output.
extern "C" WheelMatrix *__stdcall D3DXMatrixMultiply(WheelMatrix *output, WheelMatrix *left,
                                                     WheelMatrix *right) {
    WheelMatrix result;
    for (int row = 0; row < 4; ++row) {
        for (int column = 0; column < 4; ++column) {
            float sum = 0.0f;
            for (int k = 0; k < 4; ++k) {
                sum += left->elements[row * 4 + k] * right->elements[k * 4 + column];
            }
            result.elements[row * 4 + column] = sum;
        }
    }
    *output = result;
    return output;
}

unsigned int bGetTicker() {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (unsigned int)(now.tv_sec * 1000000u + now.tv_nsec / 1000u);
}

// Unrecovered original routines. The demo state never reaches them; if it does, stop.
bool ReplayRecorder::QueryState5EFE70() { NativeTrap("replay recorder 0x5efe70"); return false; }
void MoverOwnerObject::Reset5FC2A0(MoverModeOwner *) { NativeTrap("car race object reset 0x5fc2a0"); }
// Force-feedback device manager (*0x870760) methods are unrecovered DirectInput effects.
// Natively the manager pointer is null, so recovered 0x406460 never reaches them.
bool ForceFeedbackManager::HasDevice5BFD70(int) { NativeTrap("force feedback 0x5bfd70"); return false; }
bool ForceFeedbackManager::IsEffectActive5BFD90(int, int) { NativeTrap("force feedback 0x5bfd90"); return false; }
void ForceFeedbackManager::Stop5BFE00(int) { NativeTrap("force feedback 0x5bfe00"); }
void ForceFeedbackManager::Stop5BFE70(int) { NativeTrap("force feedback 0x5bfe70"); }
void ForceFeedbackManager::Play5C9150(int, int, int, int) { NativeTrap("force feedback 0x5c9150"); }
bool ForceFeedbackManager::IsEffectActive5C9040(int, int) { NativeTrap("force feedback 0x5c9040"); return false; }
void ForceFeedbackManager::SetConstant5C9550(int, int, int) { NativeTrap("force feedback 0x5c9550"); }
void ForceFeedbackManager::SetSpring5C9590(int, int) { NativeTrap("force feedback 0x5c9590"); }
void ForceFeedbackManager::PlayJolt5C9760(int, int) { NativeTrap("force feedback 0x5c9760"); }
void ForceFeedbackManager::SetRumble5C9D90(int, int) { NativeTrap("force feedback 0x5c9d90"); }
void ForceFeedbackManager::SetSurface5C9FD0(int, int, int, int) { NativeTrap("force feedback 0x5c9fd0"); }
void ForceFeedbackManager::Stop5CD850(int) { NativeTrap("force feedback 0x5cd850"); }
void ForceFeedbackManager::Stop5CD910(int) { NativeTrap("force feedback 0x5cd910"); }
void ForceFeedbackManager::Stop5CD9D0(int) { NativeTrap("force feedback 0x5cd9d0"); }
void ForceFeedbackManager::Start5CDA10(int) { NativeTrap("force feedback 0x5cda10"); }
void ForceFeedbackManager::Stop5CDBB0(int) { NativeTrap("force feedback 0x5cdbb0"); }
// Skid-mark trail methods belong to the skid-mark renderer (0x5e6e70/0x5d3230).
// Scaffold: record segments for the demo renderer instead of the original effect.
NativeSkidSegment nativeSkidSegments[NATIVE_SKID_CAPACITY];
int nativeSkidCount;

void SkidMarkTrail::AddSegment(MoverModeOwner *, WheelVector *centre, WheelVector *across, int surface,
                               float intensity) {
    NativeSkidSegment *segment = &nativeSkidSegments[nativeSkidCount % NATIVE_SKID_CAPACITY];
    segment->centre = *centre;
    segment->across = *across;
    segment->intensity = intensity;
    segment->surface = surface;
    segment->connected = trail != 0;
    trail = this;
    ++nativeSkidCount;
}

void SkidMarkTrail::End() {
    trail = 0;
}
void DraftTracker::Update(float, float) { NativeTrap("draft tracker 0x5fa540"); }


// ---------------------------------------------------------------------------
// Controls scaffolding.

void NativeControls::UnknownSlot00() { NativeTrap("controls slot 0x00"); }
void NativeControls::UnknownSlot04() { NativeTrap("controls slot 0x04"); }
void NativeControls::UnknownSlot08() { NativeTrap("controls slot 0x08"); }
void NativeControls::UnknownSlot0C() { NativeTrap("controls slot 0x0c"); }
void NativeControls::UnknownSlot10() { NativeTrap("controls slot 0x10"); }
void NativeControls::UnknownSlot14() { NativeTrap("controls slot 0x14"); }
void NativeControls::UnknownSlot18() { NativeTrap("controls slot 0x18"); }
void NativeControls::UnknownSlot1C() { NativeTrap("controls slot 0x1c"); }
int NativeControls::GetGearRequest() { return ReadGearRequest(); }
int NativeControls::GetDirectGear() { return ReadDirectGear(); }
void NativeControls::SetSelectedGear(int gear) { StoreGearRequest(gear); }
void NativeControls::SetCurrentGear(int gear) { StoreDirectGear(gear); }
void NativeControls::UnknownSlot30() { SelectForward(); }
#define NATIVE_TRAP_SLOT(name) void NativeControls::name() { NativeTrap("controls " #name); }
NATIVE_TRAP_SLOT(UnknownSlot34) NATIVE_TRAP_SLOT(UnknownSlot38)
NATIVE_TRAP_SLOT(UnknownSlot3C) NATIVE_TRAP_SLOT(UnknownSlot40) NATIVE_TRAP_SLOT(UnknownSlot44)
NATIVE_TRAP_SLOT(UnknownSlot48) NATIVE_TRAP_SLOT(UnknownSlot4C) NATIVE_TRAP_SLOT(UnknownSlot50)
NATIVE_TRAP_SLOT(UnknownSlot54) NATIVE_TRAP_SLOT(UnknownSlot58) NATIVE_TRAP_SLOT(UnknownSlot5C)
NATIVE_TRAP_SLOT(UnknownSlot60) NATIVE_TRAP_SLOT(UnknownSlot64) NATIVE_TRAP_SLOT(UnknownSlot68)
NATIVE_TRAP_SLOT(UnknownSlot6C) NATIVE_TRAP_SLOT(UnknownSlot70) NATIVE_TRAP_SLOT(UnknownSlot74)
NATIVE_TRAP_SLOT(UnknownSlot78) NATIVE_TRAP_SLOT(UnknownSlot7C) NATIVE_TRAP_SLOT(UnknownSlot80)
unsigned short NativeControls::GetGuideHeading() { return 0; }

// 0x578e90 event dispatch remains unrecovered. Native intentionally drops the
// event; the recovered caller retains its original receiver and arguments.
void DrivetrainContext::ControlEvent(float, const char *) {}

// ---------------------------------------------------------------------------
// Runtime and vehicle assembly.

extern int *raceState890118;
extern unsigned int nativeCollisionHullKey;
void AdvanceWorldClock();
extern float frameDuration865154;
extern float resetDurationDefault;
extern DrivetrainContext *drivetrainContext;
extern DrivetrainRandom *drivetrainRandom;

namespace {

bool ReadFile(const std::string &path, std::vector<unsigned char> *data) {
    FILE *file = fopen(path.c_str(), "rb");
    if (!file) {
        return false;
    }
    unsigned char buffer[65536];
    size_t count;
    while ((count = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        data->insert(data->end(), buffer, buffer + count);
    }
    fclose(file);
    return true;
}

unsigned int Read32(const std::vector<unsigned char> &data, size_t offset) {
    return data[offset] | data[offset + 1] << 8 | data[offset + 2] << 16 | (unsigned int)data[offset + 3] << 24;
}

// Platform scaffolding for the GLOBALB.BUN CarTypeInfo chunk (0x34600). The original
// chunk loader remains unrecovered; this seam keeps the chunk's 0x890-byte rows resident
// and installs them as the original table pointer 0x8a1ccc, which recovered composer
// 0x5b4dc0 and the tuning helpers index by car (declared at file scope above).

bool LoadCarTypeInfo(const char *gameDirectory, const char *carName,
                     CameraSettingsTable *cameraSettings, int *carIndex, char *modelName, char *error,
                     int errorSize) {
    static std::vector<unsigned char> bundle;
    if (bundle.empty()) {
        std::string path = std::string(gameDirectory) + "/GLOBAL/GLOBALB.BUN";
        if (!ReadFile(path, &bundle)) {
            snprintf(error, errorSize, "cannot read %s", path.c_str());
            return false;
        }
    }
    for (size_t offset = 0; offset + 8 <= bundle.size();) {
        unsigned int id = Read32(bundle, offset);
        unsigned int size = Read32(bundle, offset + 4);
        if (id == 0x34600) {
            size_t table = offset + 8 + 8;
            int count = (int)((size - 8) / 0x890);
            carTypeInfoTable = &bundle[table];
            for (int i = 0; i < count; ++i) {
                const char *name = reinterpret_cast<const char *>(&bundle[table + i * 0x890]);
                if (strncmp(name, carName, 32) == 0) {
                    memcpy(cameraSettings, &bundle[table + i * 0x890 + 0x720], sizeof(CameraSettingsTable));
                    memcpy(modelName, &bundle[table + i * 0x890 + 0x20], 32);
                    modelName[31] = 0;
                    *carIndex = i;
                    return true;
                }
            }
            snprintf(error, errorSize, "car %s not found in CarTypeInfo", carName);
            return false;
        }
        offset += 8 + size;
    }
    snprintf(error, errorSize, "CarTypeInfo chunk 0x34600 not found");
    return false;
}

// Platform scaffolding for the car geometry loader: read the car's collision hull chunk
// (0x80034020) once per car and hand it to the recovered hull loader 0x5a1750. The hull
// key is the CarTypeInfo model name (+0x20) hashed with "_CV" (0x61c320, stock parts).
bool LoadCarHulls(const char *gameDirectory, const char *carName, const char *modelName, char *error,
                  int errorSize) {
    static std::vector<std::string> loaded;
    nativeCollisionHullKey = cshashWithStartValue("_CV", cshash(modelName));
    for (size_t i = 0; i < loaded.size(); ++i) {
        if (loaded[i] == carName) {
            return true;
        }
    }
    std::vector<unsigned char> geometry;
    std::string path = std::string(gameDirectory) + "/CARS/" + carName + "/GEOMETRY.BIN";
    if (!ReadFile(path, &geometry)) {
        snprintf(error, errorSize, "cannot read %s", path.c_str());
        return false;
    }
    for (size_t offset = 0; offset + 8 <= geometry.size();) {
        unsigned int id = Read32(geometry, offset);
        unsigned int size = Read32(geometry, offset + 4);
        if (id == 0x80034020) {
            // Keep the chunk's 16-byte alignment phase; the loader aligns inside it.
            unsigned char *raw = static_cast<unsigned char *>(malloc(size + 8 + 32));
            unsigned char *chunk = reinterpret_cast<unsigned char *>(
                ((reinterpret_cast<size_t>(raw) + 15) & ~static_cast<size_t>(15)) + (offset & 15));
            memcpy(chunk, &geometry[offset], size + 8);
            LoadCollisionHulls(reinterpret_cast<int *>(chunk));
            break;
        }
        offset += 8 + size;
    }
    loaded.push_back(carName);
    return true;
}

}  // namespace

void DestroyNativeVehicle(NativeVehicle *vehicle) {
#ifdef UG2_ENABLE_MODDING
    NativeModsVehicleDestroying(vehicle);
#endif
    if (vehicle->owner && vehicle->owner->collisionBody3C) {
        delete vehicle->owner->collisionBody3C;
        vehicle->owner->collisionBody3C = 0;
    }
    if (vehicle->controls) {
        vehicle->controls->steering220.Destroy();
        vehicle->controls = 0;
        if (vehicle->owner) vehicle->owner->input30 = 0;
    }
}

bool InitialiseNativeRuntime(const char *gameDirectory, char *error, int errorSize) {
    std::string exe = std::string(gameDirectory) + "/speed2.exe";
    if (!MapOriginalImage(exe.c_str(), error, errorSize)) {
        return false;
    }
    RunPhysicsStaticInitializers();
    // Collision-body world: static list heads, then the pools created at race start
    // (0x5a0200, 0x593db0, 0x592b00).
    RunCollisionStaticInitializers();
    InitialiseCollisionPools();
    InitialiseContactPool();
    InitialiseCollisionEventPool();
    // Scaffold race-status object (constructed by 0x5fc730 with phase 0, free drive).
    raceState890118 = raceStatusScaffold;
    drivetrainContext = &contextScaffold;
    randomScaffold.stateF8 = 0x12345678u;
    drivetrainRandom = &randomScaffold;
    // The same world object (0x890080) holds the collision event lists at +0 and +8.
    CollisionEventOwner *events = reinterpret_cast<CollisionEventOwner *>(&randomScaffold);
    events->unidentified0.next = events->unidentified0.previous = &events->unidentified0;
    events->events8.next = events->events8.previous = &events->events8;
    return true;
}

bool CreateNativeVehicle(NativeVehicle *vehicle, const char *gameDirectory, const char *carName,
                         float startX, float startY, float startZ, float heading, char *error,
                         int errorSize) {
    memset(vehicle, 0, sizeof(*vehicle));
    PhysicsInfo *info = static_cast<PhysicsInfo *>(calloc(1, sizeof(PhysicsInfo)));
    info->parameters10.Construct();
    int carIndex = 0;
    char carTypeName[32];
    if (!LoadCarTypeInfo(gameDirectory, carName, &vehicle->cameraSettings, &carIndex, carTypeName, error, errorSize)) {
        return false;
    }
    PhysicsParameters *parameters = &info->parameters10;
    parameters->value0 = carIndex;
    // Native parameter entry seam: the original caller that binds the tuning
    // object to its car, selection and record is unrecovered. Bind them here,
    // then run recovered composer 0x5b4dc0 (stock build, extras, tuning,
    // drive curve, class index) in free-drive mode without the performance run.
    parameters->tuning374.value0 = carIndex;
    parameters->tuning374.selection8 = &parameters->selection2E0;
    parameters->tuning374.recordC = &parameters->record30;
    parameters->Compose(0, 0, 0);
    MoverModeOwner *owner = static_cast<MoverModeOwner *>(calloc(1, 0xa00));
    owner->physicsInfo10 = info;
    owner->context14 = static_cast<MoverContext *>(calloc(1, 0x910));
    owner->record20 = &info->parameters10.record30;
    owner->moverType28 = 1;
    NativeControls *controls = static_cast<NativeControls *>(calloc(1, sizeof(NativeControls)));
    new (controls) NativeControls;
    // Recovered human input constructor 0x425fc0 (with base 0x423330 and route
    // reset 0x41d160); C++ placement construction above supplies the native
    // vtable in place of the original identity writes, and SEH is not modelled.
    controls->ConstructHuman(owner);
    // Native device seam: the original profile/controller setup assigns a
    // device slot; slot 0 is assigned in the retail image (0x864788).
    controls->device2AC = 0;
    // Native free-drive entry seam: the original session entry is unresolved.
    // It releases the pre-race brake hold (+0x2b4) and selects drive through
    // the recovered human slot instead of the constructor's neutral request.
    controls->flag2B4 = 0;
    controls->SelectForward();
    owner->input30 = controls;
    // Car object defaults from its constructor 0x604260 that the mover reads.
    owner->gear434 = 1;
    owner->effectsEnabled550 = 1;
    owner->resetDuration978 = resetDurationDefault;
    DrivetrainAuxiliary *state = owner->State();
    state->record0 = &info->parameters10.record30;
    // Car constructor 0x604260 builds the collider at +0x5c4; state +4 refers to it.
    WCollider *collider = reinterpret_cast<WCollider *>(reinterpret_cast<char *>(owner) + 0x5c4);
    collider->Construct();
    state->collider4 = collider;
    state->Reset();
    state->position20.x = startX;
    state->position20.y = startY;
    state->position20.z = startZ;
    // Scaffold placement: yaw about +Z (rows are the body axes in world space).
    float c = cosf(heading), s = sinf(heading);
    state->matrix50.elements[0] = c;
    state->matrix50.elements[1] = s;
    state->matrix50.elements[4] = -s;
    state->matrix50.elements[5] = c;
    WheelIdentity(&state->matrix90);
    // Car constructor 0x604260 creates the CarCollisionBody (0x5aec20) at +0x3c from the
    // car's collision hull (CARS/<car>/GEOMETRY.BIN chunk 0x80034020).
    if (!LoadCarHulls(gameDirectory, carName, carTypeName, error, errorSize)) {
        return false;
    }
#ifdef UG2_ENABLE_MODDING
    owner->collisionBody3C = NativeModsCollisionBody(owner, 0);
#else
    owner->collisionBody3C = new CarCollisionBody(owner, 0);
#endif
    MoverCarInfo *carInfo = static_cast<MoverCarInfo *>(calloc(1, 0x100));
    VehicleMover *mover = static_cast<VehicleMover *>(calloc(1, sizeof(VehicleMover)));
    new (mover->integrator8) MoverIntegrator;
    owner->mover34 = mover;
    mover->Construct(&info->parameters10.record30, carInfo, controls, owner);
    // Mover factory 0x5f46a0 marks the body as mover-driven and attaches it (0x5938c0).
    owner->collisionBody3C->hasMover5C = 1;
    owner->collisionBody3C->AttachMover();
    vehicle->mover = mover;
    vehicle->owner = owner;
    vehicle->controls = controls;
    vehicle->info = info;
    vehicle->carInfo = carInfo;
    snprintf(vehicle->carName, sizeof(vehicle->carName), "%s", carName);
#ifdef UG2_ENABLE_MODDING
    NativeModsVehicleCreated(vehicle);
    if (NativeModsFailed()) { snprintf(error, errorSize, "mod provider initialization failed"); return false; }
#endif
    return true;
}

void SetNativeControls(NativeVehicle *vehicle, float throttle, float brake, float handbrake, float steer) {
    NativeControls *controls = vehicle->controls;
    // SDL capture seam: writes the action fields the original input event
    // handlers write (pedals and the raw -1..1 steering axis at +0x2c8).
    // Recovered human update 0x41e2c0 turns them into pedals and the angle.
    controls->throttle2D4 = throttle;
    controls->brake2DC = brake;
    controls->handbrake2EC = handbrake;
    controls->steering2C8 = steer;
}

float StepNativeVehicle(NativeVehicle *vehicle, float duration) {
    // World update order from 0x609ce0: advance every dynamic body (0x5f0f00; the car body's
    // Advance runs the car update, i.e. the mover step) and then resolve collisions
    // (0x5ffa50). Scaffold: the world clock (0x57a0e0) is advanced after the step so the
    // mover keeps reading a zero frame duration as before.
    vehicle->controls->Update(duration);
    vehicle->owner->SnapshotFrameKinematics(duration);
#ifdef UG2_ENABLE_MODDING
    NativeModsBeforeStep(vehicle, duration);
    if (NativeModsFailed()) return 0.0f;
#endif
    AdvanceCollisionBodies(duration);
#ifdef UG2_ENABLE_MODDING
    if (NativeModsFailed()) return 0.0f;
#endif
    UpdateCollisionWorld(collisionEventOwner890080, duration);
    vehicle->owner->UpdateFrameAcceleration(duration);
    frameDuration865154 = duration;
    AdvanceWorldClock();
#ifdef UG2_ENABLE_MODDING
    NativeModsAfterStep(vehicle, duration);
#endif
    float result = 0.0f;
    return result;
}
