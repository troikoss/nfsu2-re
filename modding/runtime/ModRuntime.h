#ifndef UG2_MOD_RUNTIME_H
#define UG2_MOD_RUNTIME_H
#include "../sdk/ug2_mod_api.h"
#include <string>
#include <vector>
namespace ug2mod {
/* Host adapter callbacks never cross the plugin ABI as C++ objects. */
struct Adapter {
    int (*vehicle)(Ug2Handle, Ug2VehicleState *);
    uint32_t (*hudCount)();
    int (*hudObject)(uint32_t, Ug2HudObject *);
    Ug2Handle (*hudFind)(uint32_t);
    int (*hudTranslate)(Ug2Handle, float, float);
    void *(*expertVehicle)(Ug2Handle);
    int (*getProjection)(Ug2Projection *);
    int (*setProjection)(const Ug2Projection *);
};
class Runtime {
public:
    Runtime(const char *buildId, const Adapter &adapter);
    ~Runtime();
    void Load(const std::string &directory);
    void Dispatch(const Ug2Event &event);
    void Shutdown();
    bool HasProvider() const;
    const char *ProviderId() const;
    bool Create(Ug2Handle handle, const Ug2VehicleState &state);
    void Destroy(Ug2Handle handle);
    int Step(Ug2Handle, float, const Ug2Controls &, const Ug2VehicleState &, Ug2VehicleState *);
    bool Impulse(Ug2Handle, const Ug2Vec3 &, const Ug2Vec3 &, Ug2VehicleState *);
    bool Synchronize(Ug2Handle, const Ug2VehicleState &);
    bool Failed() const { return failed_; }
    void Fail(const char *reason);
    static bool ValidateState(const Ug2VehicleState &);
    static std::string DefaultDirectory();
private:
    struct Plugin;
    struct Subscription { uint32_t event; Ug2Callback callback; void *userdata; };
    std::vector<Plugin *> plugins_;
    std::string buildId_;
    Adapter adapter_;
    Plugin *provider_;
    bool failed_, stopped_;
    static Plugin *Token(void *);
    static void UG2_CALL Log(void *, const char *);
    static int32_t UG2_CALL Subscribe(void *, uint32_t, Ug2Callback, void *);
    static int32_t UG2_CALL Vehicle(void *, Ug2Handle, Ug2VehicleState *);
    static uint32_t UG2_CALL HudCount(void *);
    static int32_t UG2_CALL HudObject(void *, uint32_t, Ug2HudObject *);
    static Ug2Handle UG2_CALL HudFind(void *, uint32_t);
    static int32_t UG2_CALL HudTranslate(void *, Ug2Handle, float, float);
    static int32_t UG2_CALL RegisterPhysics(void *, const Ug2PhysicsProvider *);
    static void *UG2_CALL ExpertVehicle(void *, Ug2Handle);
    static int32_t UG2_CALL GetProjection(void *, Ug2Projection *);
    static int32_t UG2_CALL SetProjection(void *, const Ug2Projection *);
};
}
#endif
