/*
 * Copyright 2026 amigazen project
 *
 * This file is part of NetSurf / Tsunami (MUI frontend).
 *
 * Minimal AmigaOS 3 helpers for the Tsunami MUI frontend (vbcc + GCC).
 */

#ifndef MUI_OS3SUPPORT_H
#define MUI_OS3SUPPORT_H

#ifndef __amigaos4__

#include "mui/vbcc_defs.h"

#include <proto/exec.h>
#include <clib/alib_protos.h>

/* OS3/NDK and some MUI SDK protos use IPTR; define before any MUI include. */
#ifndef IPTR
#define IPTR ULONG
#endif
#ifndef SIPTR
#define SIPTR LONG
#endif

/*
 * MUI dispatchers / hooks: match Voyager vapor.h for vbcc.
 * vbcc uses __reg("dn") only — do not add __asm/__saveds (they break BOOPSI).
 */
#ifndef ASM
#ifdef __VBCC__
#define ASM
#elif defined(__GNUC__)
#define ASM
#else
#define ASM
#endif
#endif

#ifndef REG
#ifdef __VBCC__
#define REG(reg, arg) __reg(#reg) arg
#elif defined(__GNUC__)
#define REG(reg, arg) arg __asm(#reg)
#else
#define REG(reg, arg) arg
#endif
#endif

#ifndef SAVEDS
#ifdef __VBCC__
#define SAVEDS
#elif defined(__GNUC__)
#define SAVEDS __saveds
#else
#define SAVEDS
#endif
#endif

#ifndef MAKE_ID
#define MAKE_ID(a,b,c,d) \
	((ULONG)(a)<<24 | (ULONG)(b)<<16 | (ULONG)(c)<<8 | (ULONG)(d))
#endif

#ifndef MIN
#define MIN(a,b) (((a)<(b))?(a):(b))
#endif

#ifndef MAX
#define MAX(a,b) (((a)>(b))?(a):(b))
#endif

/* Match amiga/os3support.h — needed by amiga/misc.h prototypes. */
#ifndef MUI_OS3_INT_TYPES
#define MUI_OS3_INT_TYPES
typedef signed char int8;
typedef unsigned char uint8;
typedef short int16;
typedef unsigned short uint16;
typedef long int32;
typedef unsigned long uint32;
#endif

/* Classic Amiga dos.library has Open/Close, not FOpen/FClose. */
#ifndef FOpen
#define FOpen(A,B,C) Open(A,B)
#endif
#ifndef FClose
#define FClose(A) Close(A)
#endif

/*
 * Do not remap tv_sec/tv_usec — Tsunami schedule.c uses PosixLib
 * gettimeofday (+aos68k_posix) which needs the POSIX struct timeval names.
 */

#endif /* !__amigaos4__ */

#endif /* MUI_OS3SUPPORT_H */
