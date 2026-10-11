#include "NativeVehicle.h"
#include "OriginalImage.h"
#include "../../Speed/Indep/Src/Physics/VehicleIntegrator.h"
#include "../../Speed/Indep/Src/Physics/VehicleAllocation.h"
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
void PhysicsInfo::Compose(int, char, char) { NativeTrap("car record composer 0x5b4dc0"); }
void PhysicsInfo::Refresh(int, int, char) { NativeTrap("car record refresh 0x5b4fc0"); }
bool MoverContactBody::IsTouching() { NativeTrap("contact body query 0x5a01b0"); return false; }
MoverContactBody *MoverMode::GetBody(int) { NativeTrap("contact body lookup 0x5a0820"); return 0; }
bool ReplayRecorder::QueryState5EFE70() { NativeTrap("replay recorder 0x5efe70"); return false; }
bool ReplayRecorder::QueryState5F00F0() { NativeTrap("replay recorder 0x5f00f0"); return false; }
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
int NativeControls::GetGearRequest() { return requestedGear; }
int NativeControls::GetDirectGear() { return 0; }
void NativeControls::SetSelectedGear(int gear) { selectedGear = gear; }
void NativeControls::SetCurrentGear(int gear) { currentGear = gear; }
#define NATIVE_TRAP_SLOT(name) void NativeControls::name() { NativeTrap("controls " #name); }
NATIVE_TRAP_SLOT(UnknownSlot30) NATIVE_TRAP_SLOT(UnknownSlot34) NATIVE_TRAP_SLOT(UnknownSlot38)
NATIVE_TRAP_SLOT(UnknownSlot3C) NATIVE_TRAP_SLOT(UnknownSlot40) NATIVE_TRAP_SLOT(UnknownSlot44)
NATIVE_TRAP_SLOT(UnknownSlot48) NATIVE_TRAP_SLOT(UnknownSlot4C) NATIVE_TRAP_SLOT(UnknownSlot50)
NATIVE_TRAP_SLOT(UnknownSlot54) NATIVE_TRAP_SLOT(UnknownSlot58) NATIVE_TRAP_SLOT(UnknownSlot5C)
NATIVE_TRAP_SLOT(UnknownSlot60) NATIVE_TRAP_SLOT(UnknownSlot64) NATIVE_TRAP_SLOT(UnknownSlot68)
NATIVE_TRAP_SLOT(UnknownSlot6C) NATIVE_TRAP_SLOT(UnknownSlot70) NATIVE_TRAP_SLOT(UnknownSlot74)
NATIVE_TRAP_SLOT(UnknownSlot78) NATIVE_TRAP_SLOT(UnknownSlot7C) NATIVE_TRAP_SLOT(UnknownSlot80)
unsigned short NativeControls::GetGuideHeading() { return 0; }

// ---------------------------------------------------------------------------
// Runtime and vehicle assembly.

extern int *raceState890118;
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
// chunk loader and the upgrade composition (0x5ac4f0/0x5997e0) are not yet recovered;
// stock records are copied from CarTypeInfo +0x110 as 0x5ac4f0 does before upgrades.
bool LoadStockRecord(const char *gameDirectory, const char *carName, CarPhysicsRecord *record,
                     int *carIndex, char *error, int errorSize) {
    std::vector<unsigned char> bundle;
    std::string path = std::string(gameDirectory) + "/GLOBAL/GLOBALB.BUN";
    if (!ReadFile(path, &bundle)) {
        snprintf(error, errorSize, "cannot read %s", path.c_str());
        return false;
    }
    for (size_t offset = 0; offset + 8 <= bundle.size();) {
        unsigned int id = Read32(bundle, offset);
        unsigned int size = Read32(bundle, offset + 4);
        if (id == 0x34600) {
            size_t table = offset + 8 + 8;
            int count = (int)((size - 8) / 0x890);
            for (int i = 0; i < count; ++i) {
                const char *name = reinterpret_cast<const char *>(&bundle[table + i * 0x890]);
                if (strncmp(name, carName, 32) == 0) {
                    memcpy(record, &bundle[table + i * 0x890 + 0x110], sizeof(CarPhysicsRecord));
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

}  // namespace

bool InitialiseNativeRuntime(const char *gameDirectory, char *error, int errorSize) {
    std::string exe = std::string(gameDirectory) + "/speed2.exe";
    if (!MapOriginalImage(exe.c_str(), error, errorSize)) {
        return false;
    }
    RunPhysicsStaticInitializers();
    // Scaffold race-status object (constructed by 0x5fc730 with phase 0, free drive).
    raceState890118 = raceStatusScaffold;
    drivetrainContext = &contextScaffold;
    randomScaffold.stateF8 = 0x12345678u;
    drivetrainRandom = &randomScaffold;
    return true;
}

bool CreateNativeVehicle(NativeVehicle *vehicle, const char *gameDirectory, const char *carName,
                         float startX, float startY, float startZ, float heading, char *error,
                         int errorSize) {
    memset(vehicle, 0, sizeof(*vehicle));
    PhysicsInfo *info = static_cast<PhysicsInfo *>(calloc(1, 0x400));
    int carIndex = 0;
    if (!LoadStockRecord(gameDirectory, carName, &info->record30, &carIndex, error, errorSize)) {
        return false;
    }
    info->value10 = carIndex;
    info->index14 = 0;
    MoverModeOwner *owner = static_cast<MoverModeOwner *>(calloc(1, 0xa00));
    owner->physicsInfo10 = info;
    owner->context14 = static_cast<MoverContext *>(calloc(1, 0x910));
    owner->mode3C = static_cast<MoverMode *>(calloc(1, 0x200));
    owner->record20 = &info->record30;
    owner->moverType28 = 1;
    NativeControls *controls = static_cast<NativeControls *>(calloc(1, sizeof(NativeControls)));
    new (controls) NativeControls;
    controls->requestedGear = -1;
    owner->input30 = controls;
    // Car object defaults from its constructor 0x604260 that the mover reads.
    owner->gear434 = 1;
    owner->effectsEnabled550 = 1;
    owner->resetDuration978 = resetDurationDefault;
    DrivetrainAuxiliary *state = owner->State();
    state->record0 = &info->record30;
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
    MoverCarInfo *carInfo = static_cast<MoverCarInfo *>(calloc(1, 0x100));
    VehicleMover *mover = static_cast<VehicleMover *>(calloc(1, sizeof(VehicleMover)));
    new (mover->integrator8) MoverIntegrator;
    owner->mover34 = mover;
    mover->Construct(&info->record30, carInfo, controls, owner);
    vehicle->mover = mover;
    vehicle->owner = owner;
    vehicle->controls = controls;
    vehicle->info = info;
    vehicle->carInfo = carInfo;
    snprintf(vehicle->carName, sizeof(vehicle->carName), "%s", carName);
    return true;
}

void SetNativeControls(NativeVehicle *vehicle, float throttle, float brake, float handbrake, float steer) {
    NativeControls *controls = vehicle->controls;
    controls->throttle20C = throttle;
    controls->brake214 = brake;
    controls->handbrake210 = handbrake;
    // Scaffold: +/-30 degree road-wheel angle in 16-bit angle units.
    int units = (int)(steer * (30.0f / 360.0f) * 65536.0f);
    controls->steering1E0 = (unsigned short)units;
}

float StepNativeVehicle(NativeVehicle *vehicle, float duration) {
    return vehicle->mover->Update(duration);
}
