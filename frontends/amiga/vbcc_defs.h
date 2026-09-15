/* String defines that cannot go on the Amiga vc command line (colons look like volumes). */
#ifndef NETSURF_VBCC_DEFS_H
#define NETSURF_VBCC_DEFS_H

#ifndef NETSURF_HOMEPAGE
#define NETSURF_HOMEPAGE "about:welcome"
#endif

/*
 * Mozilla-compatible UA reflecting NetSurf's HTML/CSS level (roughly
 * Firefox ESR basics, no modern JS). Google and others reject bare
 * "NetSurf/x.y" as an obsolete browser.
 */
#ifndef NETSURF_UA_FORMAT_STRING
#define NETSURF_UA_FORMAT_STRING "Mozilla/5.0 (%s; rv:115.0) Gecko/20100101 Firefox/115.0 NetSurf/%d.%d"
#endif

/* Logging filters — also mirrored in vc-cflags.rsp for non-VMakefile builds */
#ifndef NETSURF_BUILTIN_LOG_FILTER
#define NETSURF_BUILTIN_LOG_FILTER "(level:WARNING || cat:jserrors)"
#endif

#ifndef NETSURF_BUILTIN_VERBOSE_FILTER
#define NETSURF_BUILTIN_VERBOSE_FILTER "(level:VERBOSE || cat:jserrors)"
#endif

/* SEEK_* come from vbcc <stdio.h> / zlib zconf (_AMIGA+__VBCC__) — do not redefine */

/* vbcc has no GCC builtins / some POSIX macros */
#ifndef __builtin_expect
#define __builtin_expect(expr, value) (expr)
#endif
/* ceilf: do not #define — PosixLib/math.h declares float ceilf(float); stub is in os3support.c */
#ifndef isascii
#define isascii(c) (((c) & ~0x7f) == 0)
#endif

#endif
