/*
 * Copyright 2008-2025 Chris Young <chris@unsatisfactorysoftware.co.uk>
 *
 * This file is part of NetSurf, http://www.netsurf-browser.org/
 *
 * NetSurf is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * NetSurf is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "amiga/os3support.h"

#include <stdlib.h>
#include <string.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#ifdef __amigaos4__
#include <graphics/blitattr.h>
#include <graphics/composite.h>
#endif
#include <graphics/gfxbase.h>
#include <proto/datatypes.h>
#include <datatypes/datatypesclass.h>
#include <datatypes/pictureclass.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/utility.h>
#include <graphics/view.h>
#include <graphics/gfxmacros.h>
#include <graphics/scale.h>

#ifndef PDTA_ScaleQuality
#define PDTA_ScaleQuality	TAG_IGNORE
#endif
#ifndef PDTA_DitherQuality
#define PDTA_DitherQuality	(DTA_Dummy + 222)
#endif
#ifndef PDTA_MaxDitherPens
#define PDTA_MaxDitherPens	(DTA_Dummy + 221)
#endif
#ifndef PDTA_AlphaChannel
#define PDTA_AlphaChannel	(DTA_Dummy + 256)
#endif
#ifndef PDTA_MaskPlane
#define PDTA_MaskPlane		(DTA_Dummy + 258)
#endif
#ifndef PDTM_SCALE
#define PDTM_SCALE		(PDTM_Dummy + 2)
struct pdtScale {
	ULONG MethodID;
	ULONG ps_NewWidth;
	ULONG ps_NewHeight;
	ULONG ps_Flags;
};
#endif
#ifndef PDTA_ObtainPixelBuffer
#define PDTA_ObtainPixelBuffer	TAG_IGNORE
struct pdtBlitPixelArray {
	ULONG MethodID;
	APTR pbpa_PixelData;
	ULONG pbpa_PixelFormat;
	ULONG pbpa_PixelArrayMod;
	ULONG pbpa_Left;
	ULONG pbpa_Top;
	ULONG pbpa_Width;
	ULONG pbpa_Height;
};
#endif
#ifdef __amigaos4__
#include <proto/guigfx.h>
#include <guigfx/guigfx.h>
#include <render/render.h>
#endif

#ifdef __amigaos4__
#include <exec/extmem.h>
#include <sys/param.h>
#endif
#include "assert.h"

#include "utils/log.h"
#include "utils/nsoption.h"
#include "utils/nsurl.h"
#include "utils/messages.h"
#include "netsurf/bitmap.h"
#include "netsurf/content.h"

#include "amiga/gui.h"
#include "amiga/bitmap.h"
#include "amiga/plotters.h"
#include "amiga/memory.h"
#include "amiga/misc.h"
#include "amiga/rtg.h"
#include "amiga/schedule.h"

// disable use of "triangle mode" for scaling
#ifdef AMI_NS_TRIANGLE_SCALING
#undef AMI_NS_TRIANGLE_SCALING
#endif

struct bitmap {
	int width;
	int height;
	UBYTE *pixdata;
	struct ExtMemIFace *iextmem;
	uint32 size;
	bool opaque;
	int native;
	struct BitMap *nativebm;
	int nativebmwidth;
	int nativebmheight;
	PLANEPTR native_mask;
	ULONG native_mask_width; /* AllocRaster width — must match FreeRaster */
	Object *dto;
	struct nsurl *url;   /* temporary storage space */
	char *title; /* temporary storage space */
	ULONG *icondata; /* for appicons */
	colour bg; /* alpha blended */
	APTR drawhandle; /* guigfx */
	APTR pensharemap; /* guigfx; owned while drawhandle is live */
};

enum {
	AMI_NSBM_NONE = 0,
	AMI_NSBM_TRUECOLOUR,
	AMI_NSBM_PALETTEMAPPED 
};

struct vertex {
	float x, y;
	float s, t, w;
};

#define VTX(I,X,Y,S,T) vtx[I].x = X; vtx[I].y = Y; vtx[I].s = S; vtx[I].t = T; vtx[I].w = 1.0f; 
#define VTX_RECT(SX,SY,SW,SH,DX,DY,DW,DH) \
		VTX(0, DX,      DY,      SX,      SY); \
		VTX(1, DX + DW, DY,      SX + SW, SY); \
		VTX(2, DX,      DY + DH, SX,      SY + SH); \
		VTX(3, DX + DW, DY,      SX + SW, SY); \
		VTX(4, DX,      DY + DH, SX,      SY + SH); \
		VTX(5, DX + DW, DY + DH, SX + SW, SY + SH);

static APTR pool_bitmap = NULL;
static bool guigfx_warned = false;

/* exported — documented in amiga/bitmap.h */
ULONG ami_dt_precision(void)
{
	int q;

	q = nsoption_int(dither_quality);
	if (q <= 0) {
		return (ULONG)PRECISION_GUI;
	}
	if (q == 1) {
		return (ULONG)PRECISION_ICON;
	}
	return (ULONG)PRECISION_IMAGE;
}

/* exported — documented in amiga/bitmap.h */
ULONG ami_dt_dither_quality(void)
{
	int q;

	q = nsoption_int(dither_quality);
	if (q <= 0) {
		return 0UL;
	}
	if (q == 1) {
		return 1UL;
	}
	return 2UL;
}

/* exported — documented in amiga/bitmap.h */
ULONG ami_dt_max_dither_pens(void)
{
	int q;

	q = nsoption_int(dither_quality);
	if (q <= 0) {
		return 16UL;
	}
	if (q == 1) {
		return 32UL;
	}
	return 125UL; /* picture.datatype default */
}

/* exported — documented in amiga/bitmap.h */
ULONG ami_dt_scale_quality(void)
{
	return nsoption_bool(scale_quality) ? 1UL : 0UL;
}

/* exported function documented in amiga/bitmap.h */
void *amiga_bitmap_create(int width, int height, enum gui_bitmap_flags flags)
{
	struct bitmap *bitmap;

	if (width <= 0 || height <= 0) {
		return NULL;
	}

#ifndef __amigaos4__
	if (ami_memory_under_pressure()) {
		ami_memory_try_purge();
	}
	/* Hard ceiling — refuse absurd RGBA buffers even if dims uncapped. */
	if ((ULONG)width * (ULONG)height > 640UL * 480UL) {
		NSLOG(netsurf, WARNING,
		      "amiga_bitmap_create: refuse %dx%d (>%lu pixels)",
		      width, height, (unsigned long)(640UL * 480UL));
		return NULL;
	}
#endif

	if(pool_bitmap == NULL) pool_bitmap = ami_memory_itempool_create(sizeof(struct bitmap));

	bitmap = ami_memory_itempool_alloc(pool_bitmap, sizeof(struct bitmap));
	if(bitmap == NULL) return NULL;

	bitmap->size = width * height * 4;

#ifdef __amigaos4__
	if(nsoption_bool(use_extmem) == true) {
		uint64 size64 = bitmap->size;
		bitmap->iextmem = AllocSysObjectTags(ASOT_EXTMEM,
								ASOEXTMEM_Size, &size64,
								ASOEXTMEM_AllocationPolicy, EXTMEMPOLICY_IMMEDIATE,
								TAG_END);

		bitmap->pixdata = NULL;
		UBYTE *pixdata = amiga_bitmap_get_buffer(bitmap);
		memset(pixdata, 0xff, bitmap->size);
	} else
#endif
	{
		bitmap->pixdata = ami_memory_clear_alloc(bitmap->size, 0xff);
		if (bitmap->pixdata == NULL) {
#ifndef __amigaos4__
			ami_memory_try_purge();
			bitmap->pixdata = ami_memory_clear_alloc(bitmap->size, 0xff);
#endif
			if (bitmap->pixdata == NULL) {
				ami_memory_itempool_free(pool_bitmap, bitmap,
						sizeof(struct bitmap));
				NSLOG(netsurf, WARNING,
				      "amiga_bitmap_create: no memory for %dx%d RGBA",
				      width, height);
				return NULL;
			}
		}
	}

	bitmap->width = width;
	bitmap->height = height;

	bitmap->opaque = (flags & BITMAP_OPAQUE) == BITMAP_OPAQUE;

	bitmap->nativebm = NULL;
	bitmap->nativebmwidth = 0;
	bitmap->nativebmheight = 0;
	bitmap->native_mask = NULL;
	bitmap->native_mask_width = 0;
	bitmap->url = NULL;
	bitmap->title = NULL;
	bitmap->icondata = NULL;
	bitmap->native = AMI_NSBM_NONE;
	bitmap->drawhandle = NULL;
	bitmap->pensharemap = NULL;
	bitmap->dto = NULL;
	bitmap->bg = NS_TRANSPARENT;

	return bitmap;
}

static void amiga_bitmap_unmap_buffer(void *p)
{
#ifdef __amigaos4__
	struct bitmap *bm = p;

	if((nsoption_bool(use_extmem) == true) && (bm->pixdata != NULL)) {
		NSLOG(netsurf, INFO,
		      "Unmapping ExtMem object %p for bitmap %p",
		      bm->iextmem,
		      bm);
		bm->iextmem->Unmap(bm->pixdata, bm->size);
		bm->pixdata = NULL;
	}
#endif
}

/* exported function documented in amiga/bitmap.h */
unsigned char *amiga_bitmap_get_buffer(void *bitmap)
{
	struct bitmap *bm = bitmap;

#ifdef __amigaos4__
	if(nsoption_bool(use_extmem) == true) {
		if(bm->pixdata == NULL) {
			NSLOG(netsurf, INFO,
			      "Mapping ExtMem object %p for bitmap %p",
			      bm->iextmem,
			      bm);
			bm->pixdata = bm->iextmem->Map(NULL, bm->size, 0LL, 0);
		}

		/* unmap the buffer after one second */
		ami_schedule(1000, amiga_bitmap_unmap_buffer, bm);
	}
#endif

	return bm->pixdata;
}

/* exported function documented in amiga/bitmap.h */
size_t amiga_bitmap_get_rowstride(void *bitmap)
{
	struct bitmap *bm = bitmap;

	if(bm)
	{
		return ((bm->width)*4);
	}
	else
	{
		return 0;
	}
}


/* exported function documented in amiga/bitmap.h */
void amiga_bitmap_destroy(void *bitmap)
{
	struct bitmap *bm = bitmap;

	if(bm)
	{
		/* Dispose DT before FreeBitMap — picture.datatype owns Remap
		 * planes; free our AllocBitMap copy only after DT is gone. */
		if(bm->dto != NULL) {
			DisposeDTObject(bm->dto);
			bm->dto = NULL;
		}

		if(bm->nativebm) {
			ami_rtg_freebitmap(bm->nativebm);
			bm->nativebm = NULL;
		}

		if(bm->native_mask) {
			/* FreeRaster width must be the AllocRaster width (BMA_WIDTH),
			 * not the logical pixel width — mismatch corrupts Exec's MemList. */
			FreeRaster(bm->native_mask, bm->native_mask_width, bm->height);
			bm->native_mask = NULL;
			bm->native_mask_width = 0;
		}

#ifdef __amigaos4__
		if(bm->drawhandle) ReleaseDrawHandle(bm->drawhandle);
		bm->drawhandle = NULL;
		if(bm->pensharemap != NULL) {
			DeletePenShareMap(bm->pensharemap);
			bm->pensharemap = NULL;
		}
#endif

#ifdef __amigaos4__
		if(nsoption_bool(use_extmem) == true) {
			ami_schedule(-1, amiga_bitmap_unmap_buffer, bm);
			amiga_bitmap_unmap_buffer(bm);
			FreeSysObject(ASOT_EXTMEM, bm->iextmem);
			bm->iextmem = NULL;
		} else
#endif
		{
			ami_memory_clear_free(bm->pixdata);
		}

		if(bm->url) nsurl_unref(bm->url);
		if(bm->title) free(bm->title);

		bm->pixdata = NULL;
		bm->url = NULL;
		bm->title = NULL;

		ami_memory_itempool_free(pool_bitmap, bm, sizeof(struct bitmap));
		bm = NULL;
	}
}


/* exported function documented in amiga/bitmap.h */
bool amiga_bitmap_save(void *bitmap, const char *path, unsigned flags)
{
	int err = 0;
	Object *dto = NULL;

	if((dto = ami_datatype_object_from_bitmap(bitmap)))
	{
		if (flags & AMI_BITMAP_SCALE_ICON) {
			IDoMethod(dto, PDTM_SCALE, 16, 16, 0);
		
			if((DoDTMethod(dto, 0, 0, DTM_PROCLAYOUT, 0, 1)) == 0) {
				return false;
			}
		}

		err = SaveDTObjectA(dto, NULL, NULL, path, DTWM_IFF, FALSE, NULL);
		DisposeDTObject(dto);
	}

	if(err == 0) return false;
		else return true;
}


/* exported function documented in amiga/bitmap.h */
void amiga_bitmap_modified(void *bitmap)
{
	struct bitmap *bm = bitmap;

#ifdef __amigaos4__
		/* unmap the buffer after 0.5s - we might need it imminently */
		ami_schedule(500, amiga_bitmap_unmap_buffer, bm);
#endif

	/* DT first, then our AllocBitMap copy */
	if(bm->dto != NULL) {
		DisposeDTObject(bm->dto);
		bm->dto = NULL;
	}
	if(bm->nativebm) ami_rtg_freebitmap(bm->nativebm);
	if(bm->native_mask) {
		FreeRaster(bm->native_mask, bm->native_mask_width, bm->height);
	}
#ifdef __amigaos4__
	if(bm->drawhandle != NULL) {
		ReleaseDrawHandle(bm->drawhandle);
		bm->drawhandle = NULL;
	}
	if(bm->pensharemap != NULL) {
		DeletePenShareMap(bm->pensharemap);
		bm->pensharemap = NULL;
	}
#endif
	bm->nativebm = NULL;
	bm->native_mask = NULL;
	bm->native_mask_width = 0;
	bm->native = AMI_NSBM_NONE;
}


/* exported function documented in amiga/bitmap.h */
void amiga_bitmap_set_opaque(void *bitmap, bool opaque)
{
	struct bitmap *bm = bitmap;
	assert(bitmap);
	bm->opaque = opaque;
}


/* exported function documented in amiga/bitmap.h */
bool amiga_bitmap_get_opaque(void *bitmap)
{
	struct bitmap *bm = bitmap;
	assert(bitmap);
	return bm->opaque;
}

/* exported function documented in amiga/bitmap.h */
void amiga_bitmap_normalize_alpha(void *bitmap)
{
	struct bitmap *bm = bitmap;
	ULONG *px;
	ULONG npix;
	ULONG i;
	ULONG a;
	ULONG rgb;
	ULONG true_holes;
	ULONG promoted;

	assert(bitmap);
	px = (ULONG *)amiga_bitmap_get_buffer(bm);
	if (px == NULL || bm->width < 1 || bm->height < 1) {
		return;
	}

	npix = (ULONG)bm->width * (ULONG)bm->height;
	true_holes = 0;
	promoted = 0;

	/*
	 * a==0 with rgb==0 (or magenta colour-key) is transparency.
	 * Other zero/partial alpha with RGB is usually picture.datatype
	 * noise on opaque PNGs — promote so Remap does not invent holes.
	 */
	for (i = 0; i < npix; i++) {
		a = (px[i] >> 24) & 0xffUL;
		rgb = px[i] & 0x00ffffffUL;
		if (a == 0xffUL) {
			continue;
		}
		if (a == 0 && (rgb == 0 || rgb == 0x00ff00ffUL)) {
			px[i] = 0;
			true_holes++;
			continue;
		}
		px[i] |= 0xff000000UL;
		promoted++;
	}

	if (true_holes == 0) {
		bm->opaque = true;
		if (promoted != 0) {
			NSLOG(netsurf, INFO,
			      "amiga_bitmap_normalize_alpha: forced opaque "
			      "(%lux%lu, promoted=%lu)",
			      (unsigned long)bm->width,
			      (unsigned long)bm->height,
			      (unsigned long)promoted);
		}
		return;
	}

	bm->opaque = false;
	NSLOG(netsurf, INFO,
	      "amiga_bitmap_normalize_alpha: keep alpha "
	      "(%lux%lu, holes=%lu promoted=%lu)",
	      (unsigned long)bm->width, (unsigned long)bm->height,
	      (unsigned long)true_holes, (unsigned long)promoted);
}

/**
 * get width of a bitmap.
 */
int bitmap_get_width(void *bitmap)
{
	struct bitmap *bm = bitmap;

	if(bm)
	{
		return(bm->width);
	}
	else
	{
		return 0;
	}
}

/**
 * get height of a bitmap.
 */
int bitmap_get_height(void *bitmap)
{
	struct bitmap *bm = bitmap;

	if(bm)
	{
		return(bm->height);
	}
	else
	{
		return 0;
	}
}

#ifdef BITMAP_DUMP
void bitmap_dump(struct bitmap *bitmap)
{
	int x,y;
	ULONG *bm = (ULONG *)amiga_bitmap_get_buffer(bitmap);

	printf("Width=%ld, Height=%ld, Opaque=%s\nnativebm=%lx, width=%ld, height=%ld\n",
		bitmap->width, bitmap->height, bitmap->opaque ? "true" : "false",
		bitmap->nativebm, bitmap->nativebmwidth, bitmap->nativebmheight);
	
	for(y = 0; y < bitmap->height; y++) {
		for(x = 0; x < bitmap->width; x++) {
			printf("%lx ", bm[(y*bitmap->width) + x]);
		}
		printf("\n");
	}
}
#endif

Object *ami_datatype_object_from_bitmap(struct bitmap *bitmap)
{
	Object *dto;
	struct BitMapHeader *bmhd;
	struct pdtBlitPixelArray pbpa;
	UBYTE *src;
	ULONG width, height, stride;
	BOOL wrote_direct;

	width = (ULONG)bitmap_get_width(bitmap);
	height = (ULONG)bitmap_get_height(bitmap);
	stride = (ULONG)amiga_bitmap_get_rowstride(bitmap);
	src = amiga_bitmap_get_buffer(bitmap);
	wrote_direct = FALSE;

	if((dto = NewDTObject(NULL,
					DTA_SourceType,DTST_RAM,
					DTA_GroupID,GID_PICTURE,
					PDTA_DestMode,PMODE_V43,
					PDTA_ScaleQuality, ami_dt_scale_quality(),
					TAG_DONE)))
	{
		if(GetDTAttrs(dto,PDTA_BitMapHeader,&bmhd,TAG_DONE))
		{
			bmhd->bmh_Width = (UWORD)width;
			bmhd->bmh_Height = (UWORD)height;
			bmhd->bmh_Depth = (UBYTE)32;
			if(!amiga_bitmap_get_opaque(bitmap)) bmhd->bmh_Masking = mskHasAlpha;
		}

		memset(&pbpa, 0, sizeof(pbpa));

		/* V47: ask picture.datatype for a direct write buffer when
		 * available; fall back to PDTM_WRITEPIXELARRAY otherwise.
		 */
		SetDTAttrs(dto,NULL,NULL,
					DTA_ObjName, bitmap->url ? nsurl_access(bitmap->url) : "",
					DTA_ObjAnnotation,bitmap->title,
					DTA_ObjAuthor,messages_get("NetSurf"),
					DTA_NominalHoriz, width,
					DTA_NominalVert, height,
					PDTA_SourceMode,PMODE_V43,
					PDTA_ObtainPixelBuffer, &pbpa,
					TAG_DONE);

		if(pbpa.pbpa_PixelData != NULL &&
		   (pbpa.pbpa_PixelFormat == PBPAFMT_ARGB ||
		    pbpa.pbpa_PixelFormat == PBPAFMT_RGBA)) {
			ULONG y;
			UBYTE *dst = (UBYTE *)pbpa.pbpa_PixelData;
			ULONG dst_mod = pbpa.pbpa_PixelArrayMod;

			if(pbpa.pbpa_PixelFormat == PBPAFMT_ARGB &&
			   dst_mod == stride) {
				memcpy(dst, src, (size_t)stride * (size_t)height);
				wrote_direct = TRUE;
			} else if(pbpa.pbpa_PixelFormat == PBPAFMT_ARGB) {
				for(y = 0; y < height; y++) {
					memcpy(dst + (y * dst_mod),
					       src + (y * stride),
					       (size_t)width * 4UL);
				}
				wrote_direct = TRUE;
			}
			/* RGBA buffer: keep WRITEPIXELARRAY path below */
		}

		if(wrote_direct == FALSE) {
			IDoMethod(dto, PDTM_WRITEPIXELARRAY, src,
					PBPAFMT_ARGB, stride, 0, 0,
					width, height);
		}
	}

	return dto;
}

/* Quick way to get an object on disk into a struct bitmap */
struct bitmap *ami_bitmap_from_datatype(char *filename)
{
	Object *dto;
	struct bitmap *bm = NULL;

	if(filename == NULL || filename[0] == '\0')
		return NULL;

	if((dto = NewDTObject(filename,
					DTA_GroupID, GID_PICTURE,
					PDTA_DestMode, PMODE_V43,
					PDTA_PromoteMask, TRUE,
					PDTA_ScaleQuality, ami_dt_scale_quality(),
					TAG_DONE))) {
		struct BitMapHeader *bmh;
		BOOL has_alpha = FALSE;

		if(GetDTAttrs(dto, PDTA_BitMapHeader, &bmh, TAG_DONE))
		{
			bm = amiga_bitmap_create(bmh->bmh_Width, bmh->bmh_Height, 0);

			IDoMethod(dto, PDTM_READPIXELARRAY, amiga_bitmap_get_buffer(bm),
				PBPAFMT_ARGB, amiga_bitmap_get_rowstride(bm), 0, 0,
				bmh->bmh_Width, bmh->bmh_Height);

			if(GetDTAttrs(dto, PDTA_AlphaChannel, &has_alpha, TAG_DONE) == 1) {
				amiga_bitmap_set_opaque(bm, has_alpha == FALSE);
			} else {
				amiga_bitmap_set_opaque(bm, bitmap_test_opaque(bm));
			}
		}
		DisposeDTObject(dto);
	}

	return bm;
}

/**
 * Blend soft AARRGGBB onto a NetSurf AABBGGRR background → opaque ARGB.
 *
 * Soft buffers are PMA, but some picture.datatype READPIXELARRAY results
 * arrive as straight RGB with alpha 0 (amiga.com PNGs). Treating those as
 * transparent and flattening onto white Remapped to pen 2 — white boxes.
 */
static void ami_bitmap_preblend_argb(ULONG *dst, const ULONG *src,
		ULONG npix, colour bg)
{
	ULONG i;
	ULONG a, r, g, b;
	ULONG br, bgc, bb;
	ULONG inv;
	ULONG out_r, out_g, out_b;

	br = (ULONG)red_from_colour(bg);
	bgc = (ULONG)green_from_colour(bg);
	bb = (ULONG)blue_from_colour(bg);

	for (i = 0; i < npix; i++) {
		a = (src[i] >> 24) & 0xffUL;
		r = (src[i] >> 16) & 0xffUL;
		g = (src[i] >> 8) & 0xffUL;
		b = src[i] & 0xffUL;
		if (a == 0xffUL) {
			dst[i] = src[i] | 0xff000000UL;
		} else if (a == 0) {
			if ((r | g | b) != 0) {
				/* Empty alpha but non-zero RGB — keep colour. */
				dst[i] = 0xff000000UL | (r << 16) | (g << 8) | b;
			} else {
				dst[i] = 0xff000000UL | (br << 16) | (bgc << 8) | bb;
			}
		} else {
			/* PMA: colour planes are already scaled by a. */
			inv = 255UL - a;
			out_r = r + (br * inv) / 255UL;
			out_g = g + (bgc * inv) / 255UL;
			out_b = b + (bb * inv) / 255UL;
			if (out_r > 255UL) {
				out_r = 255UL;
			}
			if (out_g > 255UL) {
				out_g = 255UL;
			}
			if (out_b > 255UL) {
				out_b = 255UL;
			}
			dst[i] = 0xff000000UL | (out_r << 16) | (out_g << 8) | out_b;
		}
	}
}

/**
 * Soft ARGB → native BitMap via picture.datatype Remap against the screen
 * colormap. Precision / dither / pens follow Preferences Dither Quality;
 * Scale Quality drives PDTA_ScaleQuality.
 * Keep dto on the bitmap so allocated pens stay owned while nativebm lives.
 *
 * Order matters on OS3: do not attach PDTA_Screen until after the ARGB
 * source exists. Creating a RAM object with Remap+Screen (and friend BM)
 * before WRITEPIXELARRAY hangs during throbber init.
 */
static inline struct BitMap *ami_bitmap_get_picturedt(struct bitmap *bitmap,
			int width, int height, struct BitMap *restrict friendbm,
			int type, colour bg)
{
	struct Screen *scrn = ami_gui_get_screen();
	Object *dto;
	struct BitMapHeader *bmhd;
	struct BitMap *destbm;
	struct BitMap *owned;
	UBYTE *src;
	ULONG *blend;
	ULONG npix;
	ULONG sw, sh, stride;
	ULONG depth;
	ULONG ok;
	ULONG ditherq;

	dto = NULL;
	destbm = NULL;
	owned = NULL;
	blend = NULL;
	bmhd = NULL;

	sw = (ULONG)bitmap->width;
	sh = (ULONG)bitmap->height;
	stride = (ULONG)amiga_bitmap_get_rowstride(bitmap);
	src = amiga_bitmap_get_buffer(bitmap);
	npix = sw * sh;

	if (scrn == NULL || src == NULL || width < 1 || height < 1) {
		return NULL;
	}

	/* Re-normalize in case the bitmap came from a path that skipped it. */
	amiga_bitmap_normalize_alpha(bitmap);

	/* Prefer a standard friend for AllocBitMap when the caller supplied one. */
	if (friendbm != NULL &&
	    (GetBitMapAttr(friendbm, BMA_FLAGS) & BMF_STANDARD) == 0) {
		friendbm = NULL;
	}
	if (type != AMI_NSBM_PALETTEMAPPED && type != AMI_NSBM_TRUECOLOUR) {
		type = AMI_NSBM_PALETTEMAPPED;
	}

	if (bitmap->dto != NULL) {
		DisposeDTObject(bitmap->dto);
		bitmap->dto = NULL;
	}
	if (bitmap->nativebm != NULL) {
		ami_rtg_freebitmap(bitmap->nativebm);
		bitmap->nativebm = NULL;
		bitmap->native = AMI_NSBM_NONE;
	}
	if (bitmap->native_mask != NULL) {
		/* Must free with the AllocRaster width; stale masks MemList-corrupt. */
		FreeRaster(bitmap->native_mask, bitmap->native_mask_width,
				bitmap->height);
		bitmap->native_mask = NULL;
		bitmap->native_mask_width = 0;
	}

	if ((!bitmap->opaque) && (bg != NS_TRANSPARENT)) {
		/* Bake page background into Remap colours — opaque blit later. */
		blend = (ULONG *)malloc((size_t)npix * sizeof(ULONG));
		if (blend != NULL) {
			ami_bitmap_preblend_argb(blend, (const ULONG *)src,
					npix, bg);
			src = (UBYTE *)blend;
			stride = sw * 4UL;
		}
	} else if (bitmap->opaque == false) {
		/*
		 * Real alpha, no plot bg: Remap opaque colour placeholders;
		 * punch holes with a soft mask at blit time (no white bake).
		 */
		blend = (ULONG *)malloc((size_t)npix * sizeof(ULONG));
		if (blend != NULL) {
			const ULONG *sp;
			ULONG i;
			ULONG a, r, g, b;

			sp = (const ULONG *)src;
			for (i = 0; i < npix; i++) {
				a = (sp[i] >> 24) & 0xffUL;
				r = (sp[i] >> 16) & 0xffUL;
				g = (sp[i] >> 8) & 0xffUL;
				b = sp[i] & 0xffUL;
				/* Transparent placeholders Remap as black; mask punches holes */
				if (a == 0 && ((r | g | b) == 0 ||
				    (r == 0xffUL && g == 0 && b == 0xffUL))) {
					blend[i] = 0xff000000UL;
				} else {
					blend[i] = 0xff000000UL |
						(r << 16) | (g << 8) | b;
				}
			}
			src = (UBYTE *)blend;
			stride = sw * 4UL;
		}
	} else {
		/* Opaque: ensure alpha byte is 0xff for WRITEPIXELARRAY. */
		blend = (ULONG *)malloc((size_t)npix * sizeof(ULONG));
		if (blend != NULL) {
			const ULONG *sp;
			ULONG i;

			sp = (const ULONG *)src;
			for (i = 0; i < npix; i++) {
				blend[i] = sp[i] | 0xff000000UL;
			}
			src = (UBYTE *)blend;
			stride = sw * 4UL;
		}
	}

	/* Log soft centre pixel so Remap-vs-decode failures are obvious. */
	{
		const ULONG *px;
		ULONG mid;

		px = (const ULONG *)src;
		mid = 0;
		if (npix > 0) {
			mid = px[(npix / 2UL)];
		}
		NSLOG(netsurf, DEBUG,
		      "ami_bitmap_get_picturedt: soft mid=0x%08lx opaque=%d",
		      (unsigned long)mid,
		      bitmap->opaque ? 1 : 0);
	}

	ditherq = ami_dt_dither_quality();

	NSLOG(netsurf, DEBUG,
	      "ami_bitmap_get_picturedt: NewDTObject %lux%lu -> %dx%d "
	      "(dither=%lu pens=%lu prec=%lu scaleq=%lu)",
	      (unsigned long)sw, (unsigned long)sh, width, height,
	      (unsigned long)ditherq,
	      (unsigned long)ami_dt_max_dither_pens(),
	      (unsigned long)ami_dt_precision(),
	      (unsigned long)ami_dt_scale_quality());

	/* Same create tags as the soft-buffer path; Remap is Init-only (I).
	 * Screen / dither applied after pixels exist (see autodoc note on
	 * passing PDTA_Screen before or with DTM_PROCLAYOUT).
	 * PDTM_SCALE runs before PROCLAYOUT (required by picture.datatype).
	 * Quality follows Preferences Dither / Scale Quality.
	 */
	dto = NewDTObject(NULL,
			DTA_SourceType, DTST_RAM,
			DTA_GroupID, GID_PICTURE,
			PDTA_DestMode, PMODE_V42,
			PDTA_Remap, TRUE,
			/* WRITEPIXELARRAY copies into DT-owned planes; do not
			 * FreeSourceBitMap or DisposeDTObject can FreeMem our
			 * blend buffer / wrong pointer (MemList corrupt). */
			PDTA_FreeSourceBitMap, FALSE,
			PDTA_UseFriendBitMap, FALSE,
			OBP_Precision, ami_dt_precision(),
			PDTA_ScaleQuality, ami_dt_scale_quality(),
			PDTA_MaxDitherPens, ami_dt_max_dither_pens(),
			TAG_DONE);

	if (dto == NULL) {
		NSLOG(netsurf, WARNING,
		      "ami_bitmap_get_picturedt: NewDTObject failed IoErr=%ld",
		      (long)IoErr());
		free(blend);
		return NULL;
	}

	if (GetDTAttrs(dto, PDTA_BitMapHeader, &bmhd, TAG_DONE) &&
	    bmhd != NULL) {
		bmhd->bmh_Width = (UWORD)sw;
		bmhd->bmh_Height = (UWORD)sh;
		bmhd->bmh_Depth = 32;
		/* Colours are always opaque for Remap; soft mask punches holes. */
		bmhd->bmh_Masking = mskNone;
	}

	SetDTAttrs(dto, NULL, NULL,
			DTA_NominalHoriz, sw,
			DTA_NominalVert, sh,
			PDTA_SourceMode, PMODE_V43,
			TAG_DONE);

	NSLOG(netsurf, DEBUG,
	      "ami_bitmap_get_picturedt: WRITEPIXELARRAY");
	ok = IDoMethod(dto, PDTM_WRITEPIXELARRAY, src,
			PBPAFMT_ARGB, stride, 0, 0, sw, sh);
	if (ok == 0) {
		NSLOG(netsurf, WARNING,
		      "ami_bitmap_get_picturedt: WRITEPIXELARRAY failed");
		DisposeDTObject(dto);
		free(blend);
		return NULL;
	}

	if ((ULONG)width != sw || (ULONG)height != sh) {
		NSLOG(netsurf, DEBUG,
		      "ami_bitmap_get_picturedt: PDTM_SCALE %dx%d",
		      width, height);
		ok = IDoMethod(dto, PDTM_SCALE, (ULONG)width, (ULONG)height, 0);
		if (ok == 0) {
			NSLOG(netsurf, WARNING,
			      "ami_bitmap_get_picturedt: PDTM_SCALE failed");
			DisposeDTObject(dto);
			free(blend);
			return NULL;
		}
	}

	/* Attach screen + dither only once the source is ready to remap. */
	SetDTAttrs(dto, NULL, NULL,
			PDTA_Screen, scrn,
			PDTA_DitherQuality, ditherq,
			PDTA_MaxDitherPens, ami_dt_max_dither_pens(),
			TAG_DONE);

	NSLOG(netsurf, DEBUG,
	      "ami_bitmap_get_picturedt: PROCLAYOUT Remap");
	ok = DoMethod(dto, DTM_PROCLAYOUT, NULL, 1);
	if (ok == 0) {
		NSLOG(netsurf, WARNING,
		      "ami_bitmap_get_picturedt: PROCLAYOUT failed IoErr=%ld",
		      (long)IoErr());
		DisposeDTObject(dto);
		free(blend);
		return NULL;
	}

	destbm = NULL;
	GetDTAttrs(dto, PDTA_DestBitMap, &destbm, TAG_DONE);
	if (destbm == NULL) {
		GetDTAttrs(dto, PDTA_BitMap, &destbm, TAG_DONE);
	}
	if (destbm == NULL) {
		NSLOG(netsurf, WARNING,
		      "ami_bitmap_get_picturedt: no DestBitMap after layout");
		DisposeDTObject(dto);
		free(blend);
		return NULL;
	}

	depth = GetBitMapAttr(destbm, BMA_DEPTH);
	/* Match plot buffer: STANDARD planar friend when available. */
	owned = AllocBitMap((ULONG)width, (ULONG)height, depth,
			BMF_CLEAR | BMF_STANDARD,
			friendbm != NULL ? friendbm : destbm);
	if (owned == NULL) {
		owned = AllocBitMap((ULONG)width, (ULONG)height, depth,
				BMF_CLEAR, destbm);
	}
	if (owned == NULL) {
		NSLOG(netsurf, WARNING,
		      "ami_bitmap_get_picturedt: AllocBitMap %dx%d failed",
		      width, height);
		DisposeDTObject(dto);
		free(blend);
		return NULL;
	}

	BltBitMap(destbm, 0, 0, owned, 0, 0, width, height, 0xC0, 0xFF, NULL);
	WaitBlit();

	{
		struct RastPort trp;
		LONG sample;

		InitRastPort(&trp);
		trp.BitMap = owned;
		sample = ReadPixel(&trp, width / 2, height / 2);
		NSLOG(netsurf, DEBUG,
		      "ami_bitmap_get_picturedt: ok %ldx%ld -> %dx%d depth=%lu "
		      "sample=%ld",
		      (long)sw, (long)sh, width, height, (unsigned long)depth,
		      (long)sample);
	}

	free(blend);

	/*
	 * Always pin Remap at the size we just built. Callers that need a
	 * different size BitScale from this cache (see ami_bitmap_get_generic);
	 * do not Remap again at thumbnail sizes.
	 */
	bitmap->dto = dto;
	bitmap->nativebm = owned;
	bitmap->nativebmwidth = width;
	bitmap->nativebmheight = height;
	bitmap->native = type;
	bitmap->bg = bg;

	return owned;
}

#ifdef __amigaos4__
static inline struct BitMap *ami_bitmap_get_guigfx(struct bitmap *bitmap,
			int width, int height, struct BitMap *restrict friendbm, int type, colour bg)
{
	struct BitMap *restrict tbm = NULL;
	struct Screen *scrn = ami_gui_get_screen();
	struct RastPort rp;
	ULONG dithermode;
	APTR picture;
	APTR drawhandle;
	ULONG *src;
	ULONG *tmp;
	ULONG npix;
	ULONG i;
	UBYTE *bmbuffer;
	BOOL stripped;
	struct TagItem mp_tags[5];
	struct TagItem odh_tags[4];
	struct TagItem draw_tags[3];

	tmp = NULL;
	picture = NULL;
	drawhandle = NULL;
	stripped = FALSE;

	if(type == AMI_NSBM_TRUECOLOUR) {
		tbm = ami_rtg_allocbitmap(width, height, 32, 0,
			friendbm, AMI_BITMAP_FORMAT);
		if(tbm == NULL) return NULL;
	} else {
		tbm = ami_rtg_allocbitmap(width, height,
			8, 0, friendbm, 0);
		if(tbm == NULL) return NULL;
	}
	
	if(GuiGFXBase != NULL) {
		InitRastPort(&rp);
		rp.BitMap = tbm;

		/* Palette: dither by default; NULL psm uses a coarse colour cube. */
		dithermode = DITHERMODE_NONE;
		if (type == AMI_NSBM_PALETTEMAPPED) {
			dithermode = DITHERMODE_EDD;
		}
		if(nsoption_int(dither_quality) == 1) {
			dithermode = DITHERMODE_EDD;
		} else if(nsoption_int(dither_quality) == 2) {
			dithermode = DITHERMODE_FS;
		} else if(nsoption_int(dither_quality) == 0 &&
				type != AMI_NSBM_PALETTEMAPPED) {
			dithermode = DITHERMODE_NONE;
		}

		if((!bitmap->opaque) && nsoption_bool(invert_alpha)) {
			bmbuffer = amiga_bitmap_get_buffer(bitmap);
			for(i = 0; i < (ULONG)(bitmap->width * bitmap->height * 4); i += 4) {
				bmbuffer[i] = 255 - bmbuffer[i];
			}
		}

		/*
		 * Soft bitmaps are AARRGGBB. guigfx PIXFMT_0RGB_32 wants
		 * 0x00RRGGBB. Scale in DrawPicture, not MakePicture.
		 */
		src = (ULONG *)amiga_bitmap_get_buffer(bitmap);
		npix = (ULONG)bitmap->width * (ULONG)bitmap->height;
		tmp = (ULONG *)malloc((size_t)npix * sizeof(ULONG));

		mp_tags[0].ti_Tag = GGFX_PixelFormat;
		mp_tags[0].ti_Data = PIXFMT_0RGB_32;
		mp_tags[1].ti_Tag = GGFX_Independent;
		mp_tags[1].ti_Data = TRUE;
		mp_tags[2].ti_Tag = GGFX_AlphaPresent;
		mp_tags[3].ti_Tag = TAG_DONE;
		mp_tags[3].ti_Data = 0;

		if (tmp != NULL) {
			for (i = 0; i < npix; i++) {
				tmp[i] = src[i] & 0x00ffffffUL;
			}
			mp_tags[2].ti_Data = FALSE;
			picture = MakePictureA(tmp, (UWORD)bitmap->width,
					(UWORD)bitmap->height, mp_tags);
			free(tmp);
			tmp = NULL;
			stripped = TRUE;
		} else {
			NSLOG(netsurf, WARNING,
			      "ami_bitmap_get_guigfx: no tmp for %ldx%ld",
			      (long)bitmap->width, (long)bitmap->height);
			mp_tags[2].ti_Data = !bitmap->opaque;
			picture = MakePictureA(amiga_bitmap_get_buffer(bitmap),
					(UWORD)bitmap->width,
					(UWORD)bitmap->height, mp_tags);
		}

		if((!bitmap->opaque) && nsoption_bool(invert_alpha)) {
			bmbuffer = amiga_bitmap_get_buffer(bitmap);
			for(i = 0; i < (ULONG)(bitmap->width * bitmap->height * 4); i += 4) {
				bmbuffer[i] = 255 - bmbuffer[i];
			}
		}

		if(picture == NULL) {
			NSLOG(netsurf, WARNING,
			      "ami_bitmap_get_guigfx: MakePicture failed %ldx%ld",
			      (long)bitmap->width, (long)bitmap->height);
			amiga_warn_user("BMConvErr", NULL);
			ami_rtg_freebitmap(tbm);
			return NULL;
		}

		if((!bitmap->opaque) && (bg != NS_TRANSPARENT) && stripped == FALSE) {
			DoPictureMethod(picture, PICMTHD_TINTALPHA,
					colour_rb_swap(bg), TAG_DONE);
		}

		if (bitmap->drawhandle != NULL) {
			ReleaseDrawHandle(bitmap->drawhandle);
			bitmap->drawhandle = NULL;
		}
		if (bitmap->pensharemap != NULL) {
			DeletePenShareMap(bitmap->pensharemap);
			bitmap->pensharemap = NULL;
		}

		/*
		 * NULL pensharemap so guigfx builds an internal MapEngine.
		 * A caller-owned PenShareMap skips MapEngine and DrawPicture
		 * crashes inside render.library Render().
		 */
		odh_tags[0].ti_Tag = OBP_Precision;
		odh_tags[0].ti_Data = ami_dt_precision();
		odh_tags[1].ti_Tag = GGFX_DitherMode;
		odh_tags[1].ti_Data = dithermode;
		odh_tags[2].ti_Tag = GGFX_AutoDither;
		odh_tags[2].ti_Data = FALSE;
		odh_tags[3].ti_Tag = TAG_DONE;
		odh_tags[3].ti_Data = 0;

		NSLOG(netsurf, INFO,
		      "ami_bitmap_get_guigfx: ObtainDrawHandle %ldx%ld (NULL psm)",
		      (long)width, (long)height);
		drawhandle = ObtainDrawHandleA(
			NULL,
			&rp,
			scrn->ViewPort.ColorMap,
			odh_tags);
		NSLOG(netsurf, INFO,
		      "ami_bitmap_get_guigfx: ObtainDrawHandle -> %p",
		      (void *)drawhandle);

		if (drawhandle != NULL) {
			draw_tags[0].ti_Tag = GGFX_DestWidth;
			draw_tags[0].ti_Data = (ULONG)width;
			draw_tags[1].ti_Tag = GGFX_DestHeight;
			draw_tags[1].ti_Data = (ULONG)height;
			draw_tags[2].ti_Tag = TAG_DONE;
			draw_tags[2].ti_Data = 0;

			NSLOG(netsurf, INFO,
			      "ami_bitmap_get_guigfx: DrawPicture %ldx%ld -> %dx%d",
			      (long)bitmap->width, (long)bitmap->height,
			      width, height);
			DrawPictureA(drawhandle, picture, 0, 0, draw_tags);
			NSLOG(netsurf, INFO, "ami_bitmap_get_guigfx: DrawPicture done");
			bitmap->drawhandle = drawhandle;
			drawhandle = NULL;
		} else {
			NSLOG(netsurf, WARNING,
			      "ami_bitmap_get_guigfx: ObtainDrawHandle failed");
			DeletePicture(picture);
			ami_rtg_freebitmap(tbm);
			return NULL;
		}

		DeletePicture(picture);
	} else {
		if(guigfx_warned == false) {
			amiga_warn_user("BMConvErr", NULL);
			guigfx_warned = true;
		}
		ami_rtg_freebitmap(tbm);
		return NULL;
	}

	if(((type == AMI_NSBM_TRUECOLOUR) && (nsoption_int(cache_bitmaps) == 2)) ||
			((type == AMI_NSBM_PALETTEMAPPED) && (((bitmap->width == width) &&
			(bitmap->height == height) && (nsoption_int(cache_bitmaps) == 2)) ||
			(nsoption_int(cache_bitmaps) >= 1)))) {
		bitmap->nativebm = tbm;
		bitmap->nativebmwidth = width;
		bitmap->nativebmheight = height;
		bitmap->native = type;
		bitmap->bg = bg;
	}

	return tbm;
}

#endif /* __amigaos4__ */

#ifndef __amigaos4__
/**
 * BitMapScale from a cached Remap BitMap. Result is ephemeral (caller frees
 * unless ami_bitmap_is_nativebm says it is the pinned nativebm).
 */
static struct BitMap *ami_bitmap_scale_native(struct BitMap *src,
		int srcw, int srch, int dstw, int dsth,
		struct BitMap *restrict friendbm)
{
	struct BitMap *dst;
	struct BitScaleArgs bsa;
	ULONG depth;

	if(src == NULL || srcw < 1 || srch < 1 || dstw < 1 || dsth < 1)
		return NULL;

	depth = GetBitMapAttr(src, BMA_DEPTH);
	dst = AllocBitMap((ULONG)dstw, (ULONG)dsth, depth,
			BMF_CLEAR | BMF_STANDARD,
			friendbm != NULL ? friendbm : src);
	if(dst == NULL) {
		dst = AllocBitMap((ULONG)dstw, (ULONG)dsth, depth,
				BMF_CLEAR, src);
	}
	if(dst == NULL)
		return NULL;

	memset(&bsa, 0, sizeof(bsa));
	bsa.bsa_SrcX = 0;
	bsa.bsa_SrcY = 0;
	bsa.bsa_SrcWidth = (UWORD)srcw;
	bsa.bsa_SrcHeight = (UWORD)srch;
	bsa.bsa_DestX = 0;
	bsa.bsa_DestY = 0;
	bsa.bsa_XSrcFactor = (UWORD)srcw;
	bsa.bsa_XDestFactor = (UWORD)dstw;
	bsa.bsa_YSrcFactor = (UWORD)srch;
	bsa.bsa_YDestFactor = (UWORD)dsth;
	bsa.bsa_SrcBitMap = src;
	bsa.bsa_DestBitMap = dst;
	bsa.bsa_Flags = 0;
	BitMapScale(&bsa);

	NSLOG(netsurf, DEBUG,
	      "ami_bitmap_scale_native: %dx%d -> %dx%d",
	      srcw, srch, dstw, dsth);
	return dst;
}
#endif

static inline struct BitMap *ami_bitmap_get_generic(struct bitmap *bitmap,
			int width, int height, struct BitMap *restrict friendbm, int type, colour bg)
{
	struct BitMap *restrict tbm = NULL;
	struct Screen *scrn = ami_gui_get_screen();

#ifndef __amigaos4__
	(void)scrn;
#endif

	if(bitmap->nativebm)
	{
#ifndef __amigaos4__
		BOOL nativebmalphablend = ((bitmap->bg == bg) || bitmap->opaque);
#else
		/* No pre-blend alpha on OS4 */
		BOOL nativebmalphablend = TRUE;
#endif
		if((bitmap->nativebmwidth == width) && (bitmap->nativebmheight == height) && nativebmalphablend)
		{
			tbm = bitmap->nativebm;
			return tbm;
		} else if((bitmap->nativebmwidth == bitmap->width) &&
				(bitmap->nativebmheight == bitmap->height) && nativebmalphablend) {
			tbm = bitmap->nativebm;
		} else {
			if(bitmap->nativebm) amiga_bitmap_modified(bitmap);
		}
	}

	if(tbm == NULL) {
#ifndef __amigaos4__
		/*
		 * Remap once at soft intrinsic size; BitScale for other plot
		 * sizes. Remapping at every blit size was a second full
		 * DTM_PROCLAYOUT per size (on top of decode layout).
		 */
		tbm = ami_bitmap_get_picturedt(bitmap, bitmap->width,
				bitmap->height, friendbm, type, bg);
		if (tbm == NULL) {
			return NULL;
		}
#else
		if(type == AMI_NSBM_PALETTEMAPPED)
			return ami_bitmap_get_guigfx(bitmap, width, height, friendbm, type, bg);

		if(type == AMI_NSBM_TRUECOLOUR) {
			tbm = ami_rtg_allocbitmap(bitmap->width, bitmap->height, 32, 0,
										friendbm, AMI_BITMAP_FORMAT);
			if(tbm == NULL) return NULL;

			ami_rtg_writepixelarray(amiga_bitmap_get_buffer(bitmap),
										tbm, bitmap->width, bitmap->height,
										bitmap->width * 4, AMI_BITMAP_RECTFMT);
		}

		if(((type == AMI_NSBM_TRUECOLOUR) && (nsoption_int(cache_bitmaps) == 2)) ||
				((type == AMI_NSBM_PALETTEMAPPED) && (((bitmap->width == width) &&
				(bitmap->height == height) && (nsoption_int(cache_bitmaps) == 2)) ||
				(nsoption_int(cache_bitmaps) >= 1)))) {
			bitmap->nativebm = tbm;
			bitmap->nativebmwidth = bitmap->width;
			bitmap->nativebmheight = bitmap->height;
			bitmap->native = type;
		}
#endif
	}

#ifndef __amigaos4__
	/* Cached intrinsic Remap — BitScale for other plot sizes */
	if ((bitmap->nativebmwidth == width) &&
	    (bitmap->nativebmheight == height)) {
		return tbm;
	}
	return ami_bitmap_scale_native(tbm, bitmap->nativebmwidth,
			bitmap->nativebmheight, width, height, friendbm);
#else
	if((bitmap->width != width) || (bitmap->height != height)) {
		if((bitmap->nativebmwidth == width) && (bitmap->nativebmheight == height))
			return bitmap->nativebm;

		struct BitMap *restrict scaledbm;
		struct BitScaleArgs bsa;
		int depth = 32;
		if(type == AMI_NSBM_PALETTEMAPPED) depth = 8;

		scaledbm = ami_rtg_allocbitmap(width, height, depth, 0,
									friendbm, AMI_BITMAP_FORMAT);
		if(__builtin_expect(((GfxBase->LibNode.lib_Version >= 53) &&
			(type == AMI_NSBM_TRUECOLOUR)), 1)) {
			/* AutoDoc says v52, but this function isn't in OS4.0, so checking for v53 (OS4.1)
			 * Additionally, when we use friend BitMaps in non 32-bit modes it freezes the OS */

			uint32 flags = 0;
			uint32 err = COMPERR_Success;
#ifdef AMI_NS_TRIANGLE_SCALING
			struct vertex vtx[6];
			VTX_RECT(0, 0, bitmap->width, bitmap->height, 0, 0, width, height);

			flags = COMPFLAG_HardwareOnly;
			if(nsoption_bool(scale_quality) == true) flags |= COMPFLAG_SrcFilter;
			
			err = CompositeTags(COMPOSITE_Src, tbm, scaledbm,
						COMPTAG_VertexArray, vtx,
						COMPTAG_VertexFormat, COMPVF_STW0_Present,
						COMPTAG_NumTriangles, 2,
						COMPTAG_Flags, flags,
						COMPTAG_FriendBitMap, scrn->RastPort.BitMap,
						TAG_DONE);

			if (err != COMPERR_Success) {
				NSLOG(netsurf, INFO,
				      "Composite error %ld - falling back",
				      err);
#else
			{
#endif
				flags = 0;
				if(nsoption_bool(scale_quality) == true) flags |= COMPFLAG_SrcFilter;

				err = CompositeTags(COMPOSITE_Src, tbm, scaledbm,
						COMPTAG_ScaleX, COMP_FLOAT_TO_FIX((float)width/bitmap->width),
						COMPTAG_ScaleY, COMP_FLOAT_TO_FIX((float)height/bitmap->height),
						COMPTAG_Flags, flags,
						COMPTAG_FriendBitMap, scrn->RastPort.BitMap,
						TAG_DONE);
				NSLOG(netsurf, INFO,
				      "Fallback returned error %ld", err);
			}
		} else /* Do it the old-fashioned way.  This is pretty slow, even on OS4.1 */
		{
			bsa.bsa_SrcX = 0;
			bsa.bsa_SrcY = 0;
			bsa.bsa_SrcWidth = bitmap->width;
			bsa.bsa_SrcHeight = bitmap->height;
			bsa.bsa_DestX = 0;
			bsa.bsa_DestY = 0;
			bsa.bsa_XSrcFactor = bitmap->width;
			bsa.bsa_XDestFactor = width;
			bsa.bsa_YSrcFactor = bitmap->height;
			bsa.bsa_YDestFactor = height;
			bsa.bsa_SrcBitMap = tbm;
			bsa.bsa_DestBitMap = scaledbm;
			bsa.bsa_Flags = 0;

			BitMapScale(&bsa);
		}

		if(bitmap->nativebm != tbm) ami_rtg_freebitmap(bitmap->nativebm);
		ami_rtg_freebitmap(tbm);
		tbm = scaledbm;
		bitmap->nativebm = NULL;
		bitmap->native = AMI_NSBM_NONE;

		if(nsoption_int(cache_bitmaps) >= 1)
		{
			bitmap->nativebm = tbm;
			bitmap->nativebmwidth = width;
			bitmap->nativebmheight = height;
			bitmap->native = type;
			bitmap->bg = bg;
		}
	}

	return tbm;
#endif /* __amigaos4__ */
}


static inline struct BitMap *ami_bitmap_get_truecolour(struct bitmap *bitmap,
			int width, int height, struct BitMap *friendbm, colour bg)
{
	if((bitmap->native != AMI_NSBM_NONE) && (bitmap->native != AMI_NSBM_TRUECOLOUR)) {
		amiga_bitmap_modified(bitmap);
	}

	return ami_bitmap_get_generic(bitmap, width, height, friendbm, AMI_NSBM_TRUECOLOUR, bg);
}

PLANEPTR ami_bitmap_get_mask(struct bitmap *bitmap, int width,
			int height, struct BitMap *n_bm)
{
	uint32 *bmi = (uint32 *) amiga_bitmap_get_buffer(bitmap);
	UBYTE maskbit = 0;
	ULONG bm_width;
	int y, x, bpr;

	if((height != bitmap->height) || (width != bitmap->width)) return NULL;
	if(amiga_bitmap_get_opaque(bitmap) == true) return NULL;
	if(bitmap->native_mask) return bitmap->native_mask;

	bm_width = GetBitMapAttr(n_bm, BMA_WIDTH);
	bpr = RASSIZE(bm_width, 1);
	/* Blitter mask plane — AllocRaster is always Chip */
	bitmap->native_mask = AllocRaster(bm_width, height);
	if(bitmap->native_mask == NULL)
		return NULL;
	bitmap->native_mask_width = bm_width;
	SetMem(bitmap->native_mask, 0, bpr * height);

	for(y=0; y<height; y++) {
		for(x=0; x<width; x++) {
			if ((*bmi & 0xff000000U) <= (ULONG)nsoption_int(mask_alpha)) maskbit = 0;
				else maskbit = 1;
			bmi++;
			bitmap->native_mask[(y*bpr) + (x/8)] |=
				maskbit << (7 - (x % 8));
		}
	}

	return bitmap->native_mask;
}

static inline struct BitMap *ami_bitmap_get_palettemapped(struct bitmap *bitmap,
					int width, int height, struct BitMap *friendbm, colour bg)
{
	if((bitmap->native != AMI_NSBM_NONE) && (bitmap->native != AMI_NSBM_PALETTEMAPPED)) {
		amiga_bitmap_modified(bitmap);
	}

	return ami_bitmap_get_generic(bitmap, width, height, friendbm, AMI_NSBM_PALETTEMAPPED, bg);
}

struct BitMap *ami_bitmap_get_native(struct bitmap *bitmap, int width, int height,
					bool palette_mapped, struct BitMap *friendbm, colour bg)
{
	if(bitmap == NULL) return NULL;

	if(__builtin_expect(palette_mapped == true, 0)) {
		return ami_bitmap_get_palettemapped(bitmap, width, height, friendbm, bg);
	} else {
		return ami_bitmap_get_truecolour(bitmap, width, height, friendbm, bg);
	}
}

void ami_bitmap_fini(void)
{
	if(pool_bitmap) ami_memory_itempool_delete(pool_bitmap);
	pool_bitmap = NULL;
}

static nserror bitmap_render(struct bitmap *bitmap, struct hlcache_handle *content)
{
	NSLOG(netsurf, INFO, "Entering bitmap_render");

#ifndef __amigaos4__
	/*
	 * History thumbnails call this during page load and force a full
	 * off-screen replot (every image Remap/BitScale, all text). On OS3
	 * that raced the on-screen paint and redrew the same hero/band
	 * several times. Leave a blank opaque thumb; local history still
	 * works without the snapshot.
	 */
	(void)content;
	amiga_bitmap_set_opaque(bitmap, true);
	NSLOG(netsurf, INFO,
	      "bitmap_render: skipped history thumb on OS3");
	return NSERROR_OK;
#else
	int plot_width;
	int plot_height;
	struct gui_globals *bm_globals;

	plot_width = MIN(content_get_width(content), bitmap->width);
	plot_height = ((plot_width * bitmap->height) + (bitmap->width / 2)) /
			bitmap->width;

	bm_globals = ami_plot_ra_alloc(bitmap->width, bitmap->height, true, false);
	if (bm_globals == NULL) {
		NSLOG(netsurf, WARNING, "bitmap_render: plot alloc failed");
		return NSERROR_NOMEM;
	}
	ami_clearclipreg(bm_globals);

	struct redraw_context ctx = {
		.interactive = false,
		.background_images = true,
		.plot = &amiplot,
		.priv = bm_globals
	};

	content_scaled_redraw(content, plot_width, plot_height, &ctx);

	ami_rtg_readpixelarray(ami_plot_ra_get_bitmap(bm_globals), &bitmap->pixdata,
							bitmap->width, bitmap->height, 4 * bitmap->width, AMI_BITMAP_RECTFMT);

	/**\todo In theory we should be able to move the bitmap to our native area
		to try to avoid re-conversion (at the expense of memory) */

	ami_plot_ra_free(bm_globals);
	amiga_bitmap_set_opaque(bitmap, true);

	return NSERROR_OK;
#endif
}

void ami_bitmap_set_url(struct bitmap *bm, struct nsurl *url)
{
	if(bm->url != NULL) return;
	bm->url = nsurl_ref(url);
}

void ami_bitmap_set_title(struct bitmap *bm, const char *title)
{
	if(bm->title != NULL) return;
	bm->title = strdup(title);
}

void ami_bitmap_set_icondata(struct bitmap *bm, ULONG *icondata)
{
	bm->icondata = icondata;
}

void ami_bitmap_free_icondata(struct bitmap *bm)
{
	if(bm->icondata) free(bm->icondata);
	bm->icondata = NULL;
}

bool ami_bitmap_is_nativebm(struct bitmap *bm, struct BitMap *nbm)
{
	if(bm->nativebm == nbm) return true;
		else return false;
}


static struct gui_bitmap_table bitmap_table = {
	.create = amiga_bitmap_create,
	.destroy = amiga_bitmap_destroy,
	.set_opaque = amiga_bitmap_set_opaque,
	.get_opaque = amiga_bitmap_get_opaque,
	.get_buffer = amiga_bitmap_get_buffer,
	.get_rowstride = amiga_bitmap_get_rowstride,
	.get_width = bitmap_get_width,
	.get_height = bitmap_get_height,
	.modified = amiga_bitmap_modified,
	.render = bitmap_render,
};

struct gui_bitmap_table *amiga_bitmap_table = &bitmap_table;
