// Optional mod scaffolding. Recovered routines and their fixed layouts are untouched.
#include "NativeModBridge.h"
#include "Ug2ModBuildId.h"
#include "../../Speed/Indep/Src/Physics/VehicleIntegrator.h"
#include <cmath>
#include <cstring>
#include <map>
extern float engineValueScale;
namespace {
ug2mod::Runtime *runtime;
struct VehicleEntry {
    NativeVehicle *vehicle;
    Ug2Handle handle;
    bool custom, stepped, providerCreated;
    Ug2VehicleState state;
    WheelMatrix inverseInertia;
};
std::map<MoverModeOwner *, VehicleEntry> vehicles;
Ug2Handle nextHandle = 1;
Ug2Projection *currentProjection;
uint32_t (*hudCount)();
int (*hudObject)(uint32_t, Ug2HudObject *);
Ug2Handle (*hudFind)(uint32_t);
int (*hudTranslate)(Ug2Handle, float, float);
VehicleEntry *Find(MoverModeOwner *owner) {
    std::map<MoverModeOwner *, VehicleEntry>::iterator i = vehicles.find(owner);
    return i == vehicles.end() ? 0 : &i->second;
}
VehicleEntry *FindHandle(Ug2Handle h) {
    for (std::map<MoverModeOwner *, VehicleEntry>::iterator i = vehicles.begin(); i != vehicles.end(); ++i)
        if (i->second.handle == h) return &i->second;
    return 0;
}
void Capture(NativeVehicle *v, Ug2VehicleState *s) {
    memset(s, 0, sizeof(*s)); s->struct_size = sizeof(*s);
    MoverBody *body = v->mover->body20;
    memcpy(&s->position, &body->position20, sizeof(Ug2Vec3));
    memcpy(&s->velocity, &body->vectorA0, sizeof(Ug2Vec3));
    memcpy(&s->angular_velocity, &body->vectorC0, sizeof(Ug2Vec3));
    memcpy(s->orientation, body->matrix30.elements, sizeof(s->orientation));
    memcpy(s->inertia, body->spec14->inertia10.elements, sizeof(s->inertia));
    s->mass = body->spec14->mass0;
    s->rpm = v->mover->engine48->state14 * engineValueScale;
    s->gear = v->mover->drivetrain4C->ratioIndex;
    s->turbo = v->owner->State()->value40C;
    s->nitrous = v->owner->State()->timer414;
    for (int i = 0; i < 4; ++i) {
        DrivetrainWheel *wheel = v->mover->wheels38[i];
        memcpy(s->wheels[i].local_matrix, wheel->matrix30.elements, 64);
        memcpy(s->wheels[i].world_matrix, wheel->matrix70.elements, 64);
        s->wheels[i].spin = wheel->value28;
        s->wheels[i].angle = wheel->angle24;
        s->wheels[i].suspension = v->mover->arms28[i]->travel1C;
        s->wheels[i].surface = wheel->surface114;
        s->wheels[i].contact = wheel->flag11C ? 1 : 0;
    }
}
// Copy published provider presentation into the native proxy objects. This is a
// platform conversion, not a dynamics solver; custom providers own their state.
void Publish(VehicleEntry &entry) {
    NativeVehicle *v = entry.vehicle;
    const Ug2VehicleState &s = entry.state;
    MoverBody *b = v->mover->body20;
    WheelMatrix tensor; memcpy(tensor.elements, s.inertia, 64);
    WheelMatrix inverse; WheelInvert(&inverse, &tensor);
    for (int i = 0; i < 16; ++i) if (!std::isfinite(inverse.elements[i])) {
        runtime->Fail("published inertia cannot be inverted by the host"); return;
    }
    // Convert body-local provider inertia to the world axes used by collision
    // lever arms, using the same recovered matrix composition as BodyPostUpdate.
    WheelMatrix rotation, transpose, first;
    memcpy(rotation.elements, s.orientation, 64);
    WheelTranspose(&transpose, &rotation);
    WheelMultiply(&first, &rotation, &inverse);
    WheelMultiply(&entry.inverseInertia, &first, &transpose);
    for (int i = 0; i < 16; ++i) if (!std::isfinite(entry.inverseInertia.elements[i])) {
        runtime->Fail("published world inertia is nonfinite"); return;
    }
    memcpy(&b->position20, &s.position, 12);
    memcpy(&b->vectorA0, &s.velocity, 12);
    memcpy(&b->vectorC0, &s.angular_velocity, 12);
    memcpy(b->matrix30.elements, s.orientation, 64);
    memcpy(b->matrixD0.elements, s.orientation, 64);
    b->matrixD0.elements[12] = s.position.x; b->matrixD0.elements[13] = s.position.y; b->matrixD0.elements[14] = s.position.z;
    for (int i = 0; i < 4; ++i) {
        DrivetrainWheel *wheel = v->mover->wheels38[i];
        memcpy(wheel->matrix30.elements, s.wheels[i].local_matrix, 64);
        memcpy(wheel->matrix70.elements, s.wheels[i].world_matrix, 64);
        wheel->value28 = s.wheels[i].spin; wheel->angle24 = s.wheels[i].angle;
        wheel->surface114 = s.wheels[i].surface; wheel->flag11C = (unsigned char)s.wheels[i].contact;
        v->mover->arms28[i]->travel1C = s.wheels[i].suspension;
    }
    v->mover->engine48->state14 = s.rpm / engineValueScale;
    v->mover->drivetrain4C->ratioIndex = s.gear;
    // Use the recovered presentation exporter; it does not integrate dynamics.
    v->mover->ExportState(0.0f);
    v->owner->State()->engineValue400 = s.rpm;
    v->owner->State()->value40C = s.turbo;
    v->owner->State()->timer414 = (int)s.nitrous;
}
int ReadVehicle(Ug2Handle h, Ug2VehicleState *s) {
    VehicleEntry *entry = FindHandle(h);
    if (!entry) return UG2_ERROR;
    if (entry->custom) *s = entry->state; else Capture(entry->vehicle, s);
    return UG2_OK;
}
void *ExpertVehicle(Ug2Handle h) { VehicleEntry *e = FindHandle(h); return e ? e->vehicle : 0; }
int GetProjection(Ug2Projection *out) { if (!currentProjection) return UG2_ERROR; *out = *currentProjection; return UG2_OK; }
int SetProjection(const Ug2Projection *out) {
    if (!currentProjection) return UG2_ERROR;
    currentProjection->tangent_x = out->tangent_x; currentProjection->tangent_y = out->tangent_y;
    return UG2_OK;
}
uint32_t HudCount() { return hudCount ? hudCount() : 0; }
int HudObject(uint32_t i, Ug2HudObject *o) { return hudObject ? hudObject(i, o) : UG2_ERROR; }
Ug2Handle HudFind(uint32_t hash) { return hudFind ? hudFind(hash) : 0; }
int HudTranslate(Ug2Handle h, float x, float y) { return hudTranslate ? hudTranslate(h, x, y) : UG2_ERROR; }
// No added data members: original pool allocation remains exactly 0x1a0 bytes.
class ModCollisionBody : public CarCollisionBody {
public:
    ModCollisionBody(MoverModeOwner *owner, int id) : CarCollisionBody(owner, id) {}
    bool Custom() { VehicleEntry *e = Find(car130); return e && e->custom; }
    void Advance(float duration) {
        VehicleEntry *e = Find(car130);
        if (!e || !runtime || !runtime->HasProvider()) { CarCollisionBody::Advance(duration); return; }
        if (runtime->Failed()) return;
        if (!e->custom) Capture(e->vehicle, &e->state);
        Ug2Controls controls = {e->vehicle->controls->throttle20C, e->vehicle->controls->brake214,
                               e->vehicle->controls->handbrake210, e->vehicle->controls->steering1E0,
                               e->vehicle->controls->GetGearRequest()};
        Ug2VehicleState next = e->state;
        int result = runtime->Step(e->handle, duration, controls, e->state, &next);
        if (result == UG2_ERROR) return;
        if (result == UG2_PHYSICS_ORIGINAL) {
            if (e->custom) { runtime->Fail("provider cannot switch from custom state to original dynamics"); return; }
            CarCollisionBody::Advance(duration);
        } else {
            if (e->stepped && !e->custom) { runtime->Fail("provider cannot switch solver mode during a vehicle lifetime"); return; }
            Snapshot(duration);
            e->custom = true; e->state = next;
            Publish(*e); RefreshBounds();
        }
        e->stepped = true;
    }
    void ApplyImpulse(WheelVector *impulse, WheelVector *point) {
        if (!Custom()) { CarCollisionBody::ApplyImpulse(impulse, point); return; }
        VehicleEntry *e = Find(car130);
        Ug2Vec3 i, p; memcpy(&i, impulse, 12); memcpy(&p, point, 12);
        Ug2VehicleState next = e->state;
        if (runtime->Impulse(e->handle, i, p, &next)) { e->state = next; Publish(*e); }
    }
    float GetMass() { return Custom() ? 1.0f / Find(car130)->state.mass : CarCollisionBody::GetMass(); }
    WheelMatrix *GetInverseInertia() {
        if (!Custom()) return CarCollisionBody::GetInverseInertia();
        inertia150 = Find(car130)->inverseInertia; return &inertia150;
    }
    void SetPosition(WheelVector *position) {
        if (!Custom()) { CarCollisionBody::SetPosition(position); return; }
        // Collision response separates overlapping bodies before applying impulses.
        // Preserve that host correction in the provider's authoritative snapshot.
        VehicleEntry *e = Find(car130);
        memcpy(&e->state.position, position, 12);
        Publish(*e);
    }
    void SyncMover() { if (Custom()) Publish(*Find(car130)); else CarCollisionBody::SyncMover(); }
    void Rewind(float time) {
        if (!Custom()) { CarCollisionBody::Rewind(time); return; }
        VehicleEntry *e = Find(car130);
        WheelVector pos, vel; WheelMatrix matrix;
        GetPoseAt(&pos, &matrix, time); GetRewoundPosition(&vel, time);
        memcpy(&e->state.position, &pos, 12); memcpy(&e->state.velocity, &vel, 12);
        memcpy(e->state.orientation, matrix.elements, 64);
        // Original collision rewind interpolates orientation matrices. Preserve its
        // intermediate matrix; the provider receives post-collision state to reconcile.
        Publish(*e);
    }
};
static_assert(sizeof(ModCollisionBody) == sizeof(CarCollisionBody), "original collision pool size");
}
void NativeModsStart(const char *directory, bool disabled) {
    if (disabled) return;
    ug2mod::Adapter adapter = {ReadVehicle, HudCount, HudObject, HudFind, HudTranslate, ExpertVehicle, GetProjection, SetProjection};
    runtime = new ug2mod::Runtime(UG2_MOD_BUILD_ID, adapter);
    runtime->Load(directory ? directory : ug2mod::Runtime::DefaultDirectory());
    NativeModsEvent(UG2_HOST_READY);
}
void NativeModsStop() {
    if (runtime) { runtime->Shutdown(); delete runtime; runtime = 0; }
    hudCount = 0; hudObject = 0; hudFind = 0; hudTranslate = 0;
}
bool NativeModsFailed() { return runtime && runtime->Failed(); }
CarCollisionBody *NativeModsCollisionBody(MoverModeOwner *owner, int id) {
    return runtime && runtime->HasProvider() ? new ModCollisionBody(owner, id) : new CarCollisionBody(owner, id);
}
void NativeModsVehicleCreated(NativeVehicle *vehicle) {
    if (!runtime) return;
    VehicleEntry e = {}; e.vehicle = vehicle; e.handle = nextHandle++;
    Capture(vehicle, &e.state); vehicles[vehicle->owner] = e;
    VehicleEntry &entry = vehicles[vehicle->owner];
    if (runtime->HasProvider()) { entry.providerCreated = true; runtime->Create(entry.handle, entry.state); }
    Ug2Event event = {sizeof(event), UG2_VEHICLE_CREATED, entry.handle, 0, 0, 0}; runtime->Dispatch(event);
}
void NativeModsVehicleDestroying(NativeVehicle *vehicle) {
    VehicleEntry *e = Find(vehicle->owner);
    if (!runtime || !e) return;
    Ug2Event event = {sizeof(event), UG2_VEHICLE_DESTROYING, e->handle, 0, 0, 0}; runtime->Dispatch(event);
    if (e->providerCreated) runtime->Destroy(e->handle);
    vehicles.erase(vehicle->owner);
}
void NativeModsBeforeStep(NativeVehicle *vehicle, float duration) {
    VehicleEntry *e = Find(vehicle->owner);
    if (runtime && e) { Ug2Event event = {sizeof(event), UG2_PRE_SIMULATION, e->handle, duration, 0, 0}; runtime->Dispatch(event); }
}
void NativeModsAfterStep(NativeVehicle *vehicle, float duration) {
    VehicleEntry *e = Find(vehicle->owner);
    if (!runtime || !e) return;
    if (e->custom) { Publish(*e); runtime->Synchronize(e->handle, e->state); }
    else if (runtime->HasProvider()) { Capture(vehicle, &e->state); runtime->Synchronize(e->handle, e->state); }
    Ug2Event event = {sizeof(event), UG2_POST_SIMULATION, e->handle, duration, 0, 0}; runtime->Dispatch(event);
}
void NativeModsEvent(uint32_t type, float duration, uint32_t width, uint32_t height) {
    if (runtime) { Ug2Event event = {sizeof(event), type, 0, duration, width, height}; runtime->Dispatch(event); }
}
void NativeModsHudAdapter(uint32_t (*count)(), int (*object)(uint32_t, Ug2HudObject *),
                          Ug2Handle (*find)(uint32_t), int (*translate)(Ug2Handle, float, float)) {
    hudCount = count; hudObject = object; hudFind = find; hudTranslate = translate;
}

void NativeModsProjection(CameraProjection *projection, const CameraProjection &reference, uint32_t width, uint32_t height) {
    if (!runtime) return;
    Ug2Projection value = {sizeof(value), projection->tangentX, projection->tangentY, reference.tangentX, reference.tangentY};
    currentProjection = &value;
    NativeModsEvent(UG2_PRE_WORLD_PROJECTION, 0, width, height);
    currentProjection = 0;
    projection->tangentX = value.tangent_x; projection->tangentY = value.tangent_y;
}
