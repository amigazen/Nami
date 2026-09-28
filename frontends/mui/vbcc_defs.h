/* String defines that cannot go on the Amiga vc command line (colons look like volumes). */
#ifndef TSUNAMI_VBCC_DEFS_H
#define TSUNAMI_VBCC_DEFS_H

#ifndef NETSURF_HOMEPAGE
#define NETSURF_HOMEPAGE "resource:welcome-tsunami.html"
#endif

#ifndef NETSURF_UA_FORMAT_STRING
#define NETSURF_UA_FORMAT_STRING "Mozilla/5.0 (%s) Tsunami/%d.%d"
#endif

#ifndef NETSURF_BUILTIN_LOG_FILTER
#define NETSURF_BUILTIN_LOG_FILTER "(level:WARNING || cat:jserrors)"
#endif

#ifndef NETSURF_BUILTIN_VERBOSE_FILTER
#define NETSURF_BUILTIN_VERBOSE_FILTER "(level:VERBOSE || cat:jserrors)"
#endif

#ifndef __builtin_expect
#define __builtin_expect(expr, value) (expr)
#endif

#ifndef isascii
#define isascii(c) (((c) & ~0x7f) == 0)
#endif

#endif
