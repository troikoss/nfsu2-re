#ifndef UG2_NATIVE_FRONTEND_H
#define UG2_NATIVE_FRONTEND_H

// Platform scaffolding shared by the native pause and options seams (NativePause.cpp,
// NativePauseOptions.cpp). Not original code: FEng package ownership, focus and scripts.
#include "NativeFng.h"

// Package of a shown screen, or null.
FengPackage *NativeFrontendPackage(const char *package);
// 0x52b3a0 / 0x52ce40: the package's focus object receives pad input (0x54d4b0).
void NativeFrontendSetFocus(const char *package, FengObject *object);
// 0x5178b0: starts a script on an object of a shown screen (tracks and package events).
void NativeFrontendPlayScript(FengObject *object, unsigned int hash, bool restart);
std::string NativeFrontendString(unsigned int hash);

// Session-only settings initialization for the demo.
void NativePauseOptionsLoad(const char *gameDirectory);
void NativePauseOptionsUpdate();

#endif
