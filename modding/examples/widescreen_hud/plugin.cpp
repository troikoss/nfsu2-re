#include "ug2_mod_api.h"
#include <vector>
#include <stddef.h>
namespace {
const Ug2HostApi *host;
struct Anchor { uint32_t hash; int side; };
// Observed HUD_SingleRace.fng hierarchy: tach root owns gauges/turbo/NOS;
// minimap images are independent roots. NOS_Mask is outside the tach root.
const Anchor anchors[] = {
    {0x5164d4eau, UG2_HUD_RIGHT}, {0x1149c7dau, UG2_HUD_RIGHT},
    {0x3f8644cau, UG2_HUD_RIGHT}, {0x30d59b88u, UG2_HUD_RIGHT},
    {0x90fe80f3u, UG2_HUD_LEFT}, {0x40cdc515u, UG2_HUD_LEFT},
    {0xf2a54430u, UG2_HUD_LEFT}, {0x382d2fc9u, UG2_HUD_LEFT},
    {0xc1347e2fu, UG2_HUD_LEFT}, {0x1b259972u, UG2_HUD_LEFT}
};
int32_t UG2_CALL Layout(void *, const Ug2Event *event) {
    if (!event->height) return UG2_OK;
    float extent = float(event->width) * 480.0f / float(event->height);
    float shift = extent > 640.0f ? (extent - 640.0f) * 0.5f : 0.0f;
    std::vector<Ug2HudObject> objects;
    for (uint32_t i = 0; i < host->hud_count(host->context); ++i) {
        Ug2HudObject o = {}; o.struct_size = sizeof(o);
        if (host->hud_object(host->context, i, &o) != UG2_OK) return UG2_ERROR;
        objects.push_back(o);
    }
    for (size_t a = 0; a < sizeof(anchors) / sizeof(anchors[0]); ++a) {
        Ug2Handle h = host->hud_find(host->context, anchors[a].hash);
        if (!h) continue;
        // Avoid translating a descendant again if a selected ancestor is present.
        Ug2Handle parent = 0;
        for (size_t i = 0; i < objects.size(); ++i) if (objects[i].handle == h) parent = objects[i].parent;
        bool inherited = false;
        for (size_t depth = 0; parent && depth < objects.size(); ++depth) {
            Ug2Handle next = 0;
            for (size_t i = 0; i < objects.size(); ++i) if (objects[i].handle == parent) {
                for (size_t b = 0; b < sizeof(anchors) / sizeof(anchors[0]); ++b)
                    if (objects[i].hash == anchors[b].hash) inherited = true;
                next = objects[i].parent;
            }
            parent = next;
        }
        if (!inherited && host->hud_translate(host->context, h, anchors[a].side * shift, 0) != UG2_OK) return UG2_ERROR;
    }
    return UG2_OK;
}
int32_t UG2_CALL Ready(void *, const Ug2Event *) {
    if (!host->hud_find(host->context, 0x5164d4eau) || !host->hud_find(host->context, 0x90fe80f3u) || !host->hud_find(host->context, 0x40cdc515u))
        host->log(host->context, "driving HUD anchors are missing; unavailable objects will be skipped");
    return UG2_OK;
}
int32_t UG2_CALL Projection(void *, const Ug2Event *event) {
    if (!event->height || float(event->width) / float(event->height) <= 4.0f / 3.0f) return UG2_OK;
    Ug2Projection projection = {}; projection.struct_size = sizeof(projection);
    if (host->get_projection(host->context, &projection) != UG2_OK) return UG2_ERROR;
    // Hor+: retain the recovered 4:3 vertical view and expand the horizontal
    // half-angle tangent. Camera/FOV interpolation stays in the recovered code.
    projection.tangent_x = projection.reference_tangent_x * (float(event->width) / float(event->height)) / (4.0f / 3.0f);
    projection.tangent_y = projection.reference_tangent_y;
    return host->set_projection(host->context, &projection);
}
int32_t UG2_CALL Initialize(const Ug2HostApi *api) {
    if (api->api_version != UG2_MOD_API_VERSION || (api->capabilities & (UG2_CAP_HUD | UG2_CAP_PROJECTION)) != (UG2_CAP_HUD | UG2_CAP_PROJECTION)) return UG2_ERROR;
    host = api;
    if (host->subscribe(host->context, UG2_HUD_READY, Ready, 0) != UG2_OK) return UG2_ERROR;
    if (host->subscribe(host->context, UG2_PRE_HUD_LAYOUT, Layout, 0) != UG2_OK) return UG2_ERROR;
    return host->subscribe(host->context, UG2_PRE_WORLD_PROJECTION, Projection, 0);
}
void UG2_CALL Shutdown() { host = 0; }
const Ug2ModDescriptor descriptor = {sizeof(descriptor), UG2_MOD_API_VERSION, UG2_CAP_HUD | UG2_CAP_PROJECTION,
                                    "widescreen_hud", "1.0", 0, Initialize, Shutdown};
}
extern "C" UG2_EXPORT const Ug2ModDescriptor *UG2_CALL ug2_mod_query() { return &descriptor; }
