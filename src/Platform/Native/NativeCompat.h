#ifndef UG2_NATIVE_COMPAT_H
#define UG2_NATIVE_COMPAT_H

// Forced include for the native 32-bit GCC/Clang build of recovered MSVC sources.
#define __cdecl __attribute__((cdecl))
#define __stdcall __attribute__((stdcall))
#define __thiscall __attribute__((thiscall))

#endif
