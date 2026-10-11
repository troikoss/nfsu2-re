#include "ModRuntime.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <dlfcn.h>
#include <fenv.h>
#include <cstdlib>
#include <set>
#include <unistd.h>
namespace ug2mod {
namespace {
// Preserve exception flags, x87 control, and MXCSR (where the CPU supplies SSE).
struct FloatEnvironment {
    fenv_t saved;
    FloatEnvironment() { fegetenv(&saved); }
    ~FloatEnvironment() { fesetenv(&saved); }
};
bool ValidId(const std::string &id) {
    if (id.empty() || id.size() > 80) return false;
    for (size_t i = 0; i < id.size(); ++i) {
        char c = id[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '_' || c == '-')) return false;
    }
    return true;
}
std::string Trim(std::string s) {
    size_t comment = s.find('#');
    if (comment != std::string::npos) s.resize(comment);
    size_t first = s.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    return s.substr(first, s.find_last_not_of(" \t\r\n") - first + 1);
}
}
struct Runtime::Plugin {
    Runtime *owner;
    void *library;
    const Ug2ModDescriptor *descriptor;
    Ug2HostApi api;
    Ug2ExpertApi expert;
    std::string id;
    std::vector<Subscription> subscriptions, pending;
    Ug2PhysicsProvider physics;
    bool initializing, inside, active, hasPhysics;
    uint32_t event;
    Plugin() : library(0), descriptor(0), initializing(false), inside(false), active(false), hasPhysics(false), event(0) {
        memset(&physics, 0, sizeof(physics));
    }
};
Runtime::Runtime(const char *buildId, const Adapter &adapter)
    : buildId_(buildId), adapter_(adapter), provider_(0), failed_(false), stopped_(false) {}
Runtime::~Runtime() { Shutdown(); }
Runtime::Plugin *Runtime::Token(void *token) { return static_cast<Plugin *>(token); }
void UG2_CALL Runtime::Log(void *token, const char *message) {
    fprintf(stderr, "mod[%s]: %s\n", Token(token)->id.c_str(), message ? message : "");
}
int32_t UG2_CALL Runtime::Subscribe(void *token, uint32_t event, Ug2Callback callback, void *userdata) {
    Plugin *p = Token(token);
    if ((!p->initializing && !p->inside) || !callback || event < UG2_HOST_READY || event > UG2_PRE_WORLD_PROJECTION) return UG2_ERROR;
    Subscription s = {event, callback, userdata};
    p->pending.push_back(s);
    return UG2_OK;
}
int32_t UG2_CALL Runtime::Vehicle(void *token, Ug2Handle h, Ug2VehicleState *state) {
    Plugin *p = Token(token);
    if (!p->inside || !state || state->struct_size != sizeof(*state) || !p->owner->adapter_.vehicle) return UG2_ERROR;
    return p->owner->adapter_.vehicle(h, state);
}
uint32_t UG2_CALL Runtime::HudCount(void *token) {
    Plugin *p = Token(token);
    return p->inside && p->owner->adapter_.hudCount ? p->owner->adapter_.hudCount() : 0;
}
int32_t UG2_CALL Runtime::HudObject(void *token, uint32_t index, Ug2HudObject *object) {
    Plugin *p = Token(token);
    if (!p->inside || !object || object->struct_size != sizeof(*object) || !p->owner->adapter_.hudObject) return UG2_ERROR;
    return p->owner->adapter_.hudObject(index, object);
}
Ug2Handle UG2_CALL Runtime::HudFind(void *token, uint32_t hash) {
    Plugin *p = Token(token);
    return p->inside && p->owner->adapter_.hudFind ? p->owner->adapter_.hudFind(hash) : 0;
}
int32_t UG2_CALL Runtime::HudTranslate(void *token, Ug2Handle object, float x, float y) {
    Plugin *p = Token(token);
    if (!p->inside || p->event != UG2_PRE_HUD_LAYOUT || !std::isfinite(x) || !std::isfinite(y) || !p->owner->adapter_.hudTranslate) return UG2_ERROR;
    return p->owner->adapter_.hudTranslate(object, x, y);
}
int32_t UG2_CALL Runtime::RegisterPhysics(void *token, const Ug2PhysicsProvider *physics) {
    Plugin *p = Token(token);
    if (!p->initializing || p->hasPhysics || !physics || physics->struct_size != sizeof(*physics) ||
        !(p->descriptor->capabilities & UG2_CAP_PHYSICS) || !physics->create || !physics->destroy || !physics->step || !physics->impulse || !physics->synchronize) return UG2_ERROR;
    p->physics = *physics;
    p->hasPhysics = true;
    return UG2_OK;
}
void *UG2_CALL Runtime::ExpertVehicle(void *token, Ug2Handle h) {
    Plugin *p = Token(token);
    return p->inside && p->api.expert && p->owner->adapter_.expertVehicle ? p->owner->adapter_.expertVehicle(h) : 0;
}
int32_t UG2_CALL Runtime::GetProjection(void *token, Ug2Projection *out) {
    Plugin *p = Token(token);
    if (!p->inside || p->event != UG2_PRE_WORLD_PROJECTION || !out || out->struct_size != sizeof(*out) || !p->owner->adapter_.getProjection) return UG2_ERROR;
    return p->owner->adapter_.getProjection(out);
}
int32_t UG2_CALL Runtime::SetProjection(void *token, const Ug2Projection *out) {
    Plugin *p = Token(token);
    if (!p->inside || p->event != UG2_PRE_WORLD_PROJECTION || !out || out->struct_size != sizeof(*out) ||
        !std::isfinite(out->tangent_x) || !std::isfinite(out->tangent_y) || out->tangent_x <= 0 || out->tangent_y <= 0 || !p->owner->adapter_.setProjection) return UG2_ERROR;
    return p->owner->adapter_.setProjection(out);
}
std::string Runtime::DefaultDirectory() {
    char path[4096];
    ssize_t n = readlink("/proc/self/exe", path, sizeof(path) - 1);
    if (n < 0) return "mods";
    path[n] = 0;
    std::string executable(path);
    return executable.substr(0, executable.rfind('/')) + "/mods";
}
void Runtime::Load(const std::string &directory) {
    FILE *order = fopen((directory + "/load_order.txt").c_str(), "r");
    if (!order) { fprintf(stderr, "mods: no load_order.txt in %s; no mods loaded\n", directory.c_str()); return; }
    std::set<std::string> seen;
    char *line = 0; size_t capacity = 0; ssize_t length;
    while ((length = getline(&line, &capacity, order)) >= 0) {
        std::string id = Trim(std::string(line, size_t(length)));
        if (id.empty()) continue;
        if (!ValidId(id) || !seen.insert(id).second) { fprintf(stderr, "mods: invalid/duplicate ID '%s'; skipped\n", id.c_str()); continue; }
        Plugin *p = new Plugin;
        p->owner = this; p->id = id;
        const std::string path = directory + "/" + id + "/plugin.so";
        { FloatEnvironment guard; p->library = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL); }
        if (!p->library) { fprintf(stderr, "mods: %s: %s; skipped\n", id.c_str(), dlerror()); delete p; continue; }
        Ug2ModQuery query = reinterpret_cast<Ug2ModQuery>(dlsym(p->library, "ug2_mod_query"));
        if (query) { FloatEnvironment guard; p->descriptor = query(); }
        const Ug2ModDescriptor *d = p->descriptor;
        bool valid = d && d->struct_size == sizeof(*d) && d->api_version == UG2_MOD_API_VERSION &&
                     d->id && id == d->id && d->version && d->initialize && d->shutdown &&
                     !(d->capabilities & ~(UG2_CAP_EXPERT | UG2_CAP_HUD | UG2_CAP_PHYSICS | UG2_CAP_PROJECTION));
        if (valid && (d->capabilities & UG2_CAP_EXPERT))
            valid = d->required_build_id && buildId_ == d->required_build_id;
        if (!valid) {
            fprintf(stderr, "mods: %s: incompatible descriptor/API/build; skipped\n", id.c_str());
            { FloatEnvironment guard; dlclose(p->library); } delete p; continue;
        }
        p->expert.struct_size = sizeof(p->expert); p->expert.vehicle_pointer = ExpertVehicle;
        p->api = {sizeof(Ug2HostApi), UG2_MOD_API_VERSION, UG2_CAP_EXPERT | UG2_CAP_HUD | UG2_CAP_PHYSICS | UG2_CAP_PROJECTION,
                  buildId_.c_str(), p, Log, Subscribe, Vehicle, HudCount, HudObject, HudFind, HudTranslate,
                  RegisterPhysics, (d->capabilities & UG2_CAP_EXPERT) ? &p->expert : 0, GetProjection, SetProjection};
        p->initializing = true;
        int result;
        { FloatEnvironment guard; result = d->initialize(&p->api); }
        p->initializing = false;
        if (result != UG2_OK) {
            Log(p, "initialization failed; registrations discarded");
            p->pending.clear(); p->hasPhysics = false;
            // Shutdown is also the rollback contract for partially initialized plugins.
            { FloatEnvironment guard; d->shutdown(); dlclose(p->library); }
            delete p; continue;
        }
        p->active = true; p->subscriptions.swap(p->pending);
        plugins_.push_back(p);
        if (p->hasPhysics) provider_ = p;
        Log(p, "loaded");
    }
    free(line); fclose(order);
    fprintf(stderr, "mods: physics provider: %s\n", ProviderId());
}
void Runtime::Dispatch(const Ug2Event &event) {
    for (size_t i = 0; i < plugins_.size(); ++i) {
        Plugin *p = plugins_[i];
        if (!p->active) continue;
        p->inside = true; p->event = event.type;
        for (size_t j = 0; j < p->subscriptions.size(); ++j) {
            const Subscription &s = p->subscriptions[j];
            if (s.event != event.type) continue;
            int result;
            { FloatEnvironment guard; result = s.callback(s.userdata, &event); }
            if (result != UG2_OK) {
                Log(p, "callback failed; ordinary callbacks disabled");
                p->active = false; p->pending.clear();
                if (p == provider_) Fail("active provider callback failed");
                break;
            }
        }
        p->inside = false; p->event = 0;
    }
    // A subscription cannot become visible partway through this event dispatch.
    for (size_t i = 0; i < plugins_.size(); ++i) {
        Plugin *p = plugins_[i];
        p->subscriptions.insert(p->subscriptions.end(), p->pending.begin(), p->pending.end());
        p->pending.clear();
    }
}
void Runtime::Shutdown() {
    if (stopped_) return;
    stopped_ = true;
    Ug2Event event = {sizeof(Ug2Event), UG2_SHUTDOWN, 0, 0, 0, 0};
    Dispatch(event);
    provider_ = 0;
    for (size_t i = plugins_.size(); i > 0; --i) {
        Plugin *p = plugins_[i - 1];
        p->subscriptions.clear(); p->pending.clear(); p->hasPhysics = false;
        { FloatEnvironment guard; p->descriptor->shutdown(); dlclose(p->library); }
        delete p;
    }
    plugins_.clear();
}
bool Runtime::HasProvider() const { return provider_ != 0; }
const char *Runtime::ProviderId() const { return provider_ ? provider_->id.c_str() : "recovered UG2"; }
void Runtime::Fail(const char *reason) {
    if (!failed_) fprintf(stderr, "mods: fatal provider error (%s): %s\n", ProviderId(), reason);
    failed_ = true;
}
bool Runtime::Create(Ug2Handle h, const Ug2VehicleState &state) {
    if (!provider_) return true;
    Plugin *p = provider_; p->inside = true;
    int result; { FloatEnvironment guard; result = p->physics.create(p->physics.userdata, h, &state); }
    p->inside = false;
    if (result != UG2_OK) Fail("vehicle creation failed");
    return !failed_;
}
void Runtime::Destroy(Ug2Handle h) {
    if (!provider_) return;
    Plugin *p = provider_; p->inside = true;
    { FloatEnvironment guard; p->physics.destroy(p->physics.userdata, h); }
    p->inside = false;
}
int Runtime::Step(Ug2Handle h, float dt, const Ug2Controls &controls, const Ug2VehicleState &previous, Ug2VehicleState *next) {
    if (!provider_) return UG2_PHYSICS_ORIGINAL;
    if (failed_) return UG2_ERROR;
    Plugin *p = provider_; p->inside = true;
    int result; { FloatEnvironment guard; result = p->physics.step(p->physics.userdata, h, dt, &controls, &previous, next); }
    p->inside = false;
    if ((result != UG2_PHYSICS_ORIGINAL && result != UG2_PHYSICS_CUSTOM) ||
        (result == UG2_PHYSICS_CUSTOM && !ValidateState(*next))) {
        Fail("step failed or returned invalid state"); return UG2_ERROR;
    }
    return result;
}
bool Runtime::Impulse(Ug2Handle h, const Ug2Vec3 &impulse, const Ug2Vec3 &point, Ug2VehicleState *state) {
    if (!provider_ || failed_) return false;
    Plugin *p = provider_; p->inside = true;
    int result; { FloatEnvironment guard; result = p->physics.impulse(p->physics.userdata, h, &impulse, &point, state); }
    p->inside = false;
    if (result != UG2_OK || !ValidateState(*state)) Fail("impulse callback failed or returned invalid state");
    return !failed_;
}
bool Runtime::Synchronize(Ug2Handle h, const Ug2VehicleState &state) {
    if (!provider_ || failed_) return !failed_;
    Plugin *p = provider_; p->inside = true;
    int result; { FloatEnvironment guard; result = p->physics.synchronize(p->physics.userdata, h, &state); }
    p->inside = false;
    if (result != UG2_OK) Fail("post-collision synchronization failed");
    return !failed_;
}
bool Runtime::ValidateState(const Ug2VehicleState &s) {
    if (s.struct_size != sizeof(s) || !std::isfinite(s.mass) || s.mass <= 0 || !std::isfinite(1.0f / s.mass) ||
        !std::isfinite(s.rpm) || s.rpm < 0 || !std::isfinite(s.turbo) || !std::isfinite(s.nitrous) || s.nitrous < 0 || s.nitrous >= 2147483648.0f || s.gear < 0 || s.gear > 7) return false;
    const Ug2Vec3 vectors[] = {s.position, s.velocity, s.angular_velocity};
    for (size_t v = 0; v < 3; ++v) if (!std::isfinite(vectors[v].x) || !std::isfinite(vectors[v].y) || !std::isfinite(vectors[v].z)) return false;
    float speedSquared = s.velocity.x * s.velocity.x + s.velocity.y * s.velocity.y + s.velocity.z * s.velocity.z;
    if (!std::isfinite(speedSquared)) return false;
    for (int i = 0; i < 16; ++i) if (!std::isfinite(s.orientation[i]) || !std::isfinite(s.inertia[i])) return false;
    const float *r = s.orientation;
    for (int i = 0; i < 3; ++i) for (int j = 0; j < 3; ++j) {
        float dot = 0; for (int k = 0; k < 3; ++k) dot += r[i * 4 + k] * r[j * 4 + k];
        if (fabsf(dot - (i == j ? 1.0f : 0.0f)) > 0.02f) return false;
    }
    float determinant = r[0] * (r[5] * r[10] - r[6] * r[9]) - r[1] * (r[4] * r[10] - r[6] * r[8]) + r[2] * (r[4] * r[9] - r[5] * r[8]);
    if (determinant < 0.98f) return false;
    for (int i = 0; i < 3; ++i) if (r[i * 4 + 3] != 0 || r[12 + i] != 0 || s.inertia[i * 4 + 3] != 0 || s.inertia[12 + i] != 0) return false;
    if (r[15] != 1 || s.inertia[15] != 1) return false;
    const float *a = s.inertia;
    for (int i = 0; i < 3; ++i) for (int j = 0; j < 3; ++j) if (fabsf(a[i * 4 + j] - a[j * 4 + i]) > 0.0001f) return false;
    // Positive definite symmetric inertia (Sylvester's criterion).
    double minor2 = double(a[0]) * a[5] - double(a[1]) * a[4];
    double det = double(a[0]) * (double(a[5]) * a[10] - double(a[6]) * a[9]) - double(a[1]) * (double(a[4]) * a[10] - double(a[6]) * a[8]) + double(a[2]) * (double(a[4]) * a[9] - double(a[5]) * a[8]);
    if (a[0] <= 0 || minor2 <= 0 || det <= 1e-12) return false;
    for (int w = 0; w < 4; ++w) {
        const Ug2WheelState &wheel = s.wheels[w];
        if (!std::isfinite(wheel.spin) || !std::isfinite(wheel.suspension) || !std::isfinite(wheel.angle) || wheel.contact > 1) return false;
        for (int i = 0; i < 16; ++i) if (!std::isfinite(wheel.local_matrix[i]) || !std::isfinite(wheel.world_matrix[i])) return false;
    }
    return true;
}
}
