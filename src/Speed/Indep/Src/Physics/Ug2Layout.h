#ifndef UG2_LAYOUT_H
#define UG2_LAYOUT_H

// Self-contained: verification compiles run MSVC 7.1 without system include paths.
#ifdef __GNUC__
#define UG2_OFFSETOF(type, member) __builtin_offsetof(type, member)
// MSVC's 64-bit integer keyword; i386 GCC also returns it in EDX:EAX.
#ifndef __int64
#define __int64 long long
#endif
#else
#define UG2_OFFSETOF(type, member) ((unsigned int)&(((type *)0)->member))
#endif

// Compile-time check that a partial layout keeps an observed original PC offset.
#define UG2_CHECK_OFFSET(type, member, offset) \
    typedef char type##_##member##_offset_check[(UG2_OFFSETOF(type, member) == (offset)) ? 1 : -1]
#define UG2_CHECK_SIZE(type, size) \
    typedef char type##_size_check[(sizeof(type) == (size)) ? 1 : -1]

#endif
