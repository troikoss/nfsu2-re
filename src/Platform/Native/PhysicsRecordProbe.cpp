#include "OriginalImage.h"
#include "../../Speed/Indep/Src/Physics/VehicleMover.h"
#include "../../Speed/Indep/Src/Physics/PhysicsUpgradeSource.h"
#include <stdio.h>
#include <string.h>

// Native analysis scaffolding: binary source rows in, constructed stock records
// out. This uses the same recovered library and FPU precision as the demo.
int main(int argc, char **argv) {
    if (argc != 4) {
        fprintf(stderr, "usage: ug2recordprobe speed2.exe source-rows.bin records.bin\n");
        return 2;
    }
    char error[256];
    if (!MapOriginalImage(argv[1], error, sizeof(error))) {
        fprintf(stderr, "%s\n", error);
        return 1;
    }
    FILE *input = fopen(argv[2], "rb");
    FILE *output = fopen(argv[3], "wb");
    if (!input || !output) {
        fprintf(stderr, "cannot open record fixtures\n");
        return 1;
    }
    unsigned short control = 0x027f;
    asm volatile("fldcw %0" : : "m"(control));
    int count = 0;
    for (;;) {
        PhysicsUpgradeSource source;
        size_t bytes = fread(&source, 1, sizeof(source), input);
        if (!bytes) break;
        if (bytes != sizeof(source)) return 1;
        PhysicsParameters parameters;
        memset(&parameters, 0, sizeof(parameters));
        parameters.Construct();
        source.BuildRecord(&parameters.record30, &parameters.selection2E0, 0);
        if (fwrite(&parameters.record30, 1, sizeof(parameters.record30), output) != sizeof(parameters.record30)
            || fwrite(&parameters.selection2E0, 1, sizeof(parameters.selection2E0), output) != sizeof(parameters.selection2E0)) return 1;
        ++count;
    }
    bool ok = !ferror(input) && fclose(input) == 0;
    ok = fclose(output) == 0 && ok;
    if (!ok) return 1;
    printf("constructed %d stock records\n", count);
    return 0;
}
