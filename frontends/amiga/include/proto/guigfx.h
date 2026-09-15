#ifndef PROTO_GUIGFX_H
#define PROTO_GUIGFX_H
/*
 * Override guigfxlib proto: vbcc needs inline/, not #pragma libcall.
 */

#ifndef EXEC_TYPES_H
#include <exec/types.h>
#endif

#ifndef __NOLIBBASE__
extern struct Library *GuiGFXBase;
#endif

#ifdef _NO_INLINE

#include <clib/guigfx_protos.h>

#else

#include <clib/guigfx_protos.h>

#if defined(LATTICE) || defined(__SASC) || defined(_DCC)
#include <pragmas/guigfx_pragmas.h>
#elif defined(__VBCC__)
#include <inline/guigfx_protos.h>
#else
#include <pragmas/guigfx_pragmas.h>
#endif

#endif /* _NO_INLINE */

#endif /* PROTO_GUIGFX_H */
