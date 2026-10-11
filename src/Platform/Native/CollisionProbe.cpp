// Analysis scaffolding: compare the recovered query with a brute-force polygon scan.
#include "NativeVehicle.h"
#include "NativeWorld.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    const char *game = "Need for Speed Underground 2";
    float x = argc > 1 ? atof(argv[1]) : -2997.3f, y = argc > 2 ? atof(argv[2]) : 2290.66f;
    char error[512];
    if (!InitialiseNativeRuntime(game, error, sizeof(error))) return 1;
    NativeTrack track;
    if (!LoadNativeTrack(game, "L4RA", &track, error, sizeof(error))) return 1;
    for (WCollisionLink *link = collisionManager.packs0.next; link != &collisionManager.packs0; link = link->next) {
        WCollisionPack *pack = reinterpret_cast<WCollisionPack *>(link);
        if (x < pack->minimum40[0] || x > pack->maximum50[0] || y < pack->minimum40[1] || y > pack->maximum50[1]) continue;
        printf("pack %d origin (%.1f %.1f) grid %dx%d bounds (%.1f %.1f)-(%.1f %.1f)\n", pack->section30,
               pack->originX38, pack->originY3C, pack->columns34, pack->rows36, pack->minimum40[0],
               pack->minimum40[1], pack->maximum50[0], pack->maximum50[1]);
        int cellX = (int)((x - pack->originX38) / 32), cellY = (int)((y - pack->originY3C) / 32);
        WCollisionCell *cell = &pack->cells20[pack->columns34 * cellY + cellX];
        printf("  cell %d,%d refs %d..%d\n", cellX, cellY, cell->start, cell->start + cell->count);
        for (int i = 0; i < pack->polygonCountC; ++i) {
            WCollisionPolygon *p = &pack->polygons8[i];
            float v[4][4];
            p->Decode(v[0]);
            float minx = 1e9, maxx = -1e9, miny = 1e9, maxy = -1e9;
            int n = (p->flags3 & 0x10) ? 4 : 3;
            for (int k = 0; k < n; ++k) {
                if (v[k][0] < minx) minx = v[k][0];
                if (v[k][0] > maxx) maxx = v[k][0];
                if (v[k][1] < miny) miny = v[k][1];
                if (v[k][1] > maxy) maxy = v[k][1];
            }
            if (x < minx || x > maxx || y < miny || y > maxy) continue;
            int inCell = -1;
            for (int r = cell->start; r < cell->start + cell->count; ++r)
                if (pack->refs18[r].polygon == i) inCell = r;
            printf("  poly %d flags %02x group %d surface %d n=%d z=%.2f..  refIndex %d mask %04x\n", i, p->flags3,
                   p->group0, p->surface2, n, v[0][2], inCell, inCell >= 0 ? pack->refs18[inCell].mask : 0);
        }
        for (int r = cell->start; r < cell->start + cell->count && r < cell->start + 12; ++r) {
            WCollisionPolygon *p = &pack->polygons8[pack->refs18[r].polygon];
            printf("    ref %d poly %d mask %04x flags %02x\n", r, pack->refs18[r].polygon, pack->refs18[r].mask, p->flags3);
        }
    }
    return 0;
}
