#ifndef _VBCCINLINE_GUIGFX_H
#define _VBCCINLINE_GUIGFX_H
/*
 * vbcc inlines for guigfx.library (offsets from guigfx_pragmas.h)
 */

#ifndef EXEC_TYPES_H
#include <exec/types.h>
#endif
#ifndef UTILITY_TAGITEM_H
#include <utility/tagitem.h>
#endif
#ifndef GRAPHICS_GFX_H
#include <graphics/gfx.h>
#endif
#ifndef GRAPHICS_VIEW_H
#include <graphics/view.h>
#endif

APTR __MakePictureA(__reg("a6") void *, __reg("a0") APTR array,
	__reg("d0") UWORD width, __reg("d1") UWORD height,
	__reg("a1") struct TagItem *tags) = "\tjsr\t-30(a6)";
#define MakePictureA(array, width, height, tags) \
	__MakePictureA(GuiGFXBase, (array), (width), (height), (tags))

#if !defined(NO_INLINE_STDARG) && (__STDC__ == 1L) && (__STDC_VERSION__ >= 199901L)
APTR __MakePicture(__reg("a6") void *, __reg("a0") APTR array,
	__reg("d0") UWORD width, __reg("d1") UWORD height, ...) =
	"\tmove.l\ta1,-(a7)\n\tlea\t4(a7),a1\n\tjsr\t-30(a6)\n\tmovea.l\t(a7)+,a1";
#define MakePicture(...) __MakePicture(GuiGFXBase, __VA_ARGS__)
#endif

void __DeletePicture(__reg("a6") void *, __reg("a0") APTR pic) = "\tjsr\t-54(a6)";
#define DeletePicture(pic) __DeletePicture(GuiGFXBase, (pic))

/* UpdatePicture occupies -60; offsets below match fd/guigfx_lib.fd */
APTR __AddPictureA(__reg("a6") void *, __reg("a0") APTR psm,
	__reg("a1") APTR pic, __reg("a2") struct TagItem *tags) = "\tjsr\t-66(a6)";
#define AddPictureA(psm, pic, tags) __AddPictureA(GuiGFXBase, (psm), (pic), (tags))

#if !defined(NO_INLINE_STDARG) && (__STDC__ == 1L) && (__STDC_VERSION__ >= 199901L)
APTR __AddPicture(__reg("a6") void *, __reg("a0") APTR psm,
	__reg("a1") APTR pic, ...) =
	"\tmove.l\ta2,-(a7)\n\tlea\t4(a7),a2\n\tjsr\t-66(a6)\n\tmovea.l\t(a7)+,a2";
#define AddPicture(...) __AddPicture(GuiGFXBase, __VA_ARGS__)
#endif

void __RemColorHandle(__reg("a6") void *, __reg("a0") APTR colorhandle) =
	"\tjsr\t-84(a6)";
#define RemColorHandle(colorhandle) __RemColorHandle(GuiGFXBase, (colorhandle))

APTR __CreatePenShareMapA(__reg("a6") void *,
	__reg("a0") struct TagItem *tags) = "\tjsr\t-90(a6)";
#define CreatePenShareMapA(tags) __CreatePenShareMapA(GuiGFXBase, (tags))

#if !defined(NO_INLINE_STDARG) && (__STDC__ == 1L) && (__STDC_VERSION__ >= 199901L)
APTR __CreatePenShareMap(__reg("a6") void *, ...) =
	"\tmove.l\ta0,-(a7)\n\tlea\t8(a7),a0\n\tjsr\t-90(a6)\n\tmovea.l\t(a7)+,a0";
#define CreatePenShareMap(...) __CreatePenShareMap(GuiGFXBase, __VA_ARGS__)
#endif

void __DeletePenShareMap(__reg("a6") void *, __reg("a0") APTR psm) =
	"\tjsr\t-96(a6)";
#define DeletePenShareMap(psm) __DeletePenShareMap(GuiGFXBase, (psm))

APTR __ObtainDrawHandleA(__reg("a6") void *, __reg("a0") APTR psm,
	__reg("a1") struct RastPort *rp, __reg("a2") struct ColorMap *cm,
	__reg("a3") struct TagItem *tags) = "\tjsr\t-102(a6)";
#define ObtainDrawHandleA(psm, rp, cm, tags) \
	__ObtainDrawHandleA(GuiGFXBase, (psm), (rp), (cm), (tags))

#if !defined(NO_INLINE_STDARG) && (__STDC__ == 1L) && (__STDC_VERSION__ >= 199901L)
APTR __ObtainDrawHandle(__reg("a6") void *, __reg("a0") APTR psm,
	__reg("a1") struct RastPort *rp, __reg("a2") struct ColorMap *cm, ...) =
	"\tmove.l\ta3,-(a7)\n\tlea\t4(a7),a3\n\tjsr\t-102(a6)\n\tmovea.l\t(a7)+,a3";
#define ObtainDrawHandle(...) __ObtainDrawHandle(GuiGFXBase, __VA_ARGS__)
#endif

void __ReleaseDrawHandle(__reg("a6") void *, __reg("a0") APTR drawhandle) =
	"\tjsr\t-108(a6)";
#define ReleaseDrawHandle(drawhandle) __ReleaseDrawHandle(GuiGFXBase, (drawhandle))

void __DrawPictureA(__reg("a6") void *, __reg("a0") APTR drawhandle,
	__reg("a1") APTR pic, __reg("d0") UWORD x, __reg("d1") UWORD y,
	__reg("a2") struct TagItem *tags) = "\tjsr\t-114(a6)";
#define DrawPictureA(drawhandle, pic, x, y, tags) \
	__DrawPictureA(GuiGFXBase, (drawhandle), (pic), (x), (y), (tags))

#if !defined(NO_INLINE_STDARG) && (__STDC__ == 1L) && (__STDC_VERSION__ >= 199901L)
void __DrawPicture(__reg("a6") void *, __reg("a0") APTR drawhandle,
	__reg("a1") APTR pic, __reg("d0") UWORD x, __reg("d1") UWORD y, ...) =
	"\tmove.l\ta2,-(a7)\n\tlea\t4(a7),a2\n\tjsr\t-114(a6)\n\tmovea.l\t(a7)+,a2";
#define DrawPicture(...) __DrawPicture(GuiGFXBase, __VA_ARGS__)
#endif

ULONG __DoPictureMethodA(__reg("a6") void *, __reg("a0") APTR pic,
	__reg("d0") ULONG method, __reg("a1") struct TagItem *tags) =
	"\tjsr\t-138(a6)";
#define DoPictureMethodA(pic, method, tags) \
	__DoPictureMethodA(GuiGFXBase, (pic), (method), (tags))

#if !defined(NO_INLINE_STDARG) && (__STDC__ == 1L) && (__STDC_VERSION__ >= 199901L)
ULONG __DoPictureMethod(__reg("a6") void *, __reg("a0") APTR pic,
	__reg("d0") ULONG method, ...) =
	"\tmove.l\ta1,-(a7)\n\tlea\t4(a7),a1\n\tjsr\t-138(a6)\n\tmovea.l\t(a7)+,a1";
#define DoPictureMethod(...) __DoPictureMethod(GuiGFXBase, __VA_ARGS__)
#endif

#endif /* _VBCCINLINE_GUIGFX_H */
