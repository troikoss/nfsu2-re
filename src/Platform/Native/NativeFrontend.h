#ifndef UG2_NATIVE_FRONTEND_H
#define UG2_NATIVE_FRONTEND_H

// Platform scaffolding shared by native pause and options (NativePause.cpp,
// NativePauseOptions.cpp): package ownership and scripts; focus calls recovered FEng code.
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
void NativeFrontendOpenControls();
void NativeFrontendCloseControls();
bool NativePauseOptionsPointer(void *owner, float x, float y, bool click, bool drag);

#endif
