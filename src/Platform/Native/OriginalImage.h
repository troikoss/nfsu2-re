#ifndef UG2_ORIGINAL_IMAGE_H
#define UG2_ORIGINAL_IMAGE_H

// Maps the player's own speed2.exe .rdata/.data at their original addresses so the
// recovered sources can bind original globals (see OriginalSymbols.ld). No original
// code is executed; .bss is zero-filled as at process start.
bool MapOriginalImage(const char *path, char *error, int errorSize);

#endif
