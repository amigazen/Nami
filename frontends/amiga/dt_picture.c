/*
 * Copyright 2011 - 2012 Chris Young <chris@unsatisfactorysoftware.co.uk>
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

/** \file
 * DataTypes picture handler (implementation)
*/

#ifdef WITH_AMIGA_DATATYPES
#include "amiga/os3support.h"

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <proto/datatypes.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <datatypes/datatypesclass.h>
#include <datatypes/pictureclass.h>
#include <intuition/classusr.h>
#include <intuition/gadgetclass.h>
#include <graphics/gfx.h>
#include <graphics/view.h>

#ifndef PDTA_ScaleQuality
#define PDTA_ScaleQuality	TAG_IGNORE
#endif
#ifndef PDTA_DitherQuality
#define PDTA_DitherQuality	(DTA_Dummy + 222)
#endif
#ifndef PDTA_MaxDitherPens
#define PDTA_MaxDitherPens	(DTA_Dummy + 221)
#endif
/* Real V47 tags — never TAG_IGNORE (that silently disables transparency). */
#ifndef PDTA_AlphaChannel
#define PDTA_AlphaChannel	(DTA_Dummy + 256)
#endif
#ifndef PDTA_MaskPlane
#define PDTA_MaskPlane		(DTA_Dummy + 258)
#endif
#ifndef PDTM_SCALE
#define PDTM_SCALE		(PDTM_Dummy + 2)
#endif
#ifndef mskNone
#define mskNone			0
#define mskHasMask		1
#define mskHasTransparentColor	2
#define mskLasso		3
#define mskHasAlpha		4
#endif

#include "utils/log.h"
#include "utils/messages.h"
#include "utils/file.h"
#include "netsurf/plotters.h"
#include "netsurf/bitmap.h"
#include "content/llcache.h"
#include "content/content.h"
#include "content/content_protected.h"
#include "content/content_factory.h"
#include "content/handlers/image/image_cache.h"

#include "utils/nsoption.h"

#include "amiga/bitmap.h"
#include "amiga/filetype.h"
#include "amiga/datatypes.h"
#include "amiga/memory.h"
#include "amiga/rtg.h"

/* Optional; pixel path must work when this stays NULL (classic OS3). */
extern struct Library *CyberGfxBase;


static nserror amiga_dt_picture_create(const content_handler *handler,
		lwc_string *imime_type, const struct http_parameter *params,
		llcache_handle *llcache, const char *fallback_charset,
		bool quirks, struct content **c);
static bool amiga_dt_picture_convert(struct content *c);
static nserror amiga_dt_picture_clone(const struct content *old, struct content **newc);
static void amiga_dt_picture_destroy(struct content *c);

static const content_handler amiga_dt_picture_content_handler = {
	.create = amiga_dt_picture_create,
	.data_complete = amiga_dt_picture_convert,
	.destroy = amiga_dt_picture_destroy,
	.redraw = image_cache_redraw,
	.clone = amiga_dt_picture_clone,
	.get_internal = image_cache_get_internal,
	.type = image_cache_content_type,
	.no_share = false,
};

struct amiga_dt_picture_content {
	struct content c;
	Object *dto;
	char *spill; /* T: file we created; NULL if using original file:// path */
};

/**
 * Register one MIME type for the picture.datatype content handler.
 */
static nserror amiga_dt_picture_register_mime(const char *mime)
{
	nserror error;

	error = content_factory_register_handler(mime,
			&amiga_dt_picture_content_handler);
	if (error == NSERROR_OK) {
		NSLOG(netsurf, INFO,
		      "amiga_dt_picture: registered %s", mime);
	} else {
		NSLOG(netsurf, WARNING,
		      "amiga_dt_picture: failed to register %s (%d)",
		      mime, (int)error);
	}
	return error;
}

nserror amiga_dt_picture_init(void)
{
	struct DataType *dt, *prevdt = NULL;
	lwc_string *type;
	nserror error;
	struct Node *node = NULL;
	unsigned int n_from_dt = 0;
	const char *const *mime;
	/* Common web image types - NewDTObject sniffs the payload, so these
	 * need not match an enumerated datatype name exactly. Without at
	 * least these registrations, hlcache treats images as UnacceptableType
	 * even when the body fetched successfully.
	 */
	static const char *const web_image_mimes[] = {
		"image/png",
		"image/gif",
		"image/jpeg",
		"image/jpg",
		"image/pjpeg",
		"image/bmp",
		"image/x-ms-bmp",
		/* Windows .ico is handled by frontends/amiga/ico.c (baked-in). */
		"image/webp",
		/* SVG via svg.datatype when installed; NewDTObject fails softly. */
		"image/svg+xml",
		"image/svg",
		NULL
	};

	mime = web_image_mimes;
	while (*mime != NULL) {
		error = amiga_dt_picture_register_mime(*mime);
		if (error != NSERROR_OK)
			return error;
		mime++;
	}

	while ((dt = ObtainDataType(DTST_RAM, NULL,
			DTA_DataType, prevdt,
			DTA_GroupID, GID_PICTURE,
			TAG_DONE)) != NULL) {
		if (prevdt)
			ReleaseDataType(prevdt);
		prevdt = dt;
		node = NULL;

		do {
			node = ami_mime_from_datatype(dt, &type, node);

			if (node) {
				error = amiga_dt_picture_register_mime(
						lwc_string_data(type));
				if (error != NSERROR_OK) {
					ReleaseDataType(prevdt);
					return error;
				}
				n_from_dt++;
			}
		} while (node != NULL);
	}

	if (prevdt)
		ReleaseDataType(prevdt);

	NSLOG(netsurf, INFO,
	      "amiga_dt_picture: %u extra MIME type(s) from DataTypes",
	      n_from_dt);

	return NSERROR_OK;
}

static nserror amiga_dt_picture_create(const content_handler *handler,
		lwc_string *imime_type, const struct http_parameter *params,
		llcache_handle *llcache, const char *fallback_charset,
		bool quirks, struct content **c)
{
	struct amiga_dt_picture_content *adt;
	nserror error;

	adt = calloc(1, sizeof(struct amiga_dt_picture_content));
	if (adt == NULL)
		return NSERROR_NOMEM;

	error = content__init((struct content *)adt, handler, imime_type, params,
			llcache, fallback_charset, quirks);
	if (error != NSERROR_OK) {
		free(adt);
		return error;
	}

	*c = (struct content *)adt;

	return NSERROR_OK;
}

/**
 * Guess a file extension from image magic bytes so datatype selection
 * uses extension + DTST_FILE, not drawing.datatype via DTST_MEMORY.
 */
static const char *amiga_dt_picture_ext(const uint8_t *data, size_t size)
{
	if (size >= 8 && data[0] == 0x89 && data[1] == 'P' &&
	    data[2] == 'N' && data[3] == 'G') {
		return "png";
	}
	if (size >= 6 && data[0] == 'G' && data[1] == 'I' && data[2] == 'F') {
		return "gif";
	}
	if (size >= 3 && data[0] == 0xff && data[1] == 0xd8 && data[2] == 0xff) {
		return "jpg";
	}
	if (size >= 2 && data[0] == 'B' && data[1] == 'M') {
		return "bmp";
	}
	/* .ico spills fail with IoErr 212 — baked-in amiga_ico handles ICO. */
	if (size >= 5 && data[0] == '<' &&
	    (data[1] == 's' || data[1] == 'S') &&
	    (data[2] == 'v' || data[2] == 'V') &&
	    (data[3] == 'g' || data[3] == 'G')) {
		return "svg";
	}
	if (size >= 5 && data[0] == '<' && data[1] == '?' &&
	    (data[2] == 'x' || data[2] == 'X')) {
		/* <?xml ...> — likely SVG; svg.datatype sniffs content. */
		return "svg";
	}
	return "pic";
}

/**
 * True when PNG IHDR is RGB/grey with no alpha and no tRNS chunk.
 * picture.datatype READPIXELARRAY often returns black as ARGB 0x00000000 on
 * these (amiga.com interlaced RGB PNGs) — that must not become transparency.
 */
static BOOL amiga_dt_picture_png_is_opaque_rgb(const uint8_t *data, size_t size)
{
	size_t pos;
	ULONG len;
	ULONG ctype;
	UBYTE colour;
	BOOL seen_ihdr;
	BOOL has_trns;

	if (size < 24 || data[0] != 0x89 || data[1] != 'P' ||
	    data[2] != 'N' || data[3] != 'G') {
		return FALSE;
	}

	pos = 8;
	colour = 0xff;
	seen_ihdr = FALSE;
	has_trns = FALSE;

	while (pos + 12 <= size) {
		len = ((ULONG)data[pos] << 24) | ((ULONG)data[pos + 1] << 16) |
			((ULONG)data[pos + 2] << 8) | (ULONG)data[pos + 3];
		pos += 4;
		if (pos + 4 > size) {
			break;
		}
		ctype = ((ULONG)data[pos] << 24) | ((ULONG)data[pos + 1] << 16) |
			((ULONG)data[pos + 2] << 8) | (ULONG)data[pos + 3];
		pos += 4;
		if (pos + len + 4 > size) {
			break;
		}

		if (ctype == 0x49484452UL) { /* IHDR */
			if (len >= 13) {
				colour = data[pos + 9];
				seen_ihdr = TRUE;
			}
		} else if (ctype == 0x74524e53UL) { /* tRNS */
			has_trns = TRUE;
		} else if (ctype == 0x49454e44UL) { /* IEND */
			break;
		}

		pos += len + 4; /* data + CRC */
	}

	if (seen_ihdr == FALSE || has_trns != FALSE) {
		return FALSE;
	}
	/* 0 grey, 2 RGB — no alpha. 3 indexed may use tRNS (already excluded). */
	if (colour == 0 || colour == 2) {
		return TRUE;
	}
	return FALSE;
}

/**
 * Force every soft pixel fully opaque (fixes DT a=0 on black RGB).
 */
static void amiga_dt_picture_force_opaque_buffer(struct bitmap *bitmap)
{
	ULONG *px;
	ULONG npix;
	ULONG i;

	px = (ULONG *)amiga_bitmap_get_buffer(bitmap);
	if (px == NULL) {
		return;
	}
	npix = (ULONG)bitmap_get_width(bitmap) * (ULONG)bitmap_get_height(bitmap);
	for (i = 0; i < npix; i++) {
		px[i] |= 0xff000000UL;
	}
	amiga_bitmap_set_opaque(bitmap, true);
}

/* Keyed spill directory under T: for this NetSurf process. */
static char amiga_dt_spill_dir[40];
static BOOL amiga_dt_spill_ready;
static ULONG amiga_dt_spill_seq;

/**
 * Ensure T:nsXXXXXXXX/ exists (key = FindTask address). Files land inside.
 * Never spill to RAM: — only a keyed subfolder of T:.
 */
static BOOL amiga_dt_picture_ensure_spill_dir(void)
{
	BPTR lock;

	if (amiga_dt_spill_ready != FALSE) {
		return TRUE;
	}

	sprintf(amiga_dt_spill_dir, "T:ns%08lx",
		(unsigned long)FindTask(NULL));

	lock = CreateDir(amiga_dt_spill_dir);
	if (lock != 0) {
		UnLock(lock);
	}

	lock = Lock(amiga_dt_spill_dir, ACCESS_READ);
	if (lock == 0) {
		NSLOG(netsurf, WARNING,
		      "amiga_dt_picture: CreateDir(%s) IoErr=%ld",
		      amiga_dt_spill_dir, (long)IoErr());
		return FALSE;
	}
	UnLock(lock);
	amiga_dt_spill_ready = TRUE;
	NSLOG(netsurf, INFO,
	      "amiga_dt_picture: spill dir %s",
	      amiga_dt_spill_dir);
	return TRUE;
}

/**
 * Soft-ARGB decode: do NOT attach PDTA_Screen here.
 * With a Screen, picture.datatype builds a threshold MaskPlane / alpha even
 * for opaque PNGs; READPIXELARRAY then looks like real transparency and we
 * punch holes where none exist. Remap against the screen happens later in
 * ami_bitmap_get_picturedt.
 *
 * NewDTObject only — caller must PDTM_SCALE (if needed) then DTM_PROCLAYOUT.
 * Autodoc: PDTM_SCALE is only valid before the first layout.
 */
static Object *amiga_dt_picture_new_file(const char *filename)
{
	Object *dto;

	dto = NewDTObject((APTR)filename,
			DTA_SourceType, DTST_FILE,
			DTA_GroupID, GID_PICTURE,
			PDTA_Remap, FALSE,
			/* Keep source until READPIXELARRAY; we DisposeDTObject
			 * immediately after, so FreeSourceBitMap saves little. */
			PDTA_FreeSourceBitMap, FALSE,
			PDTA_DestMode, PMODE_V43,
			PDTA_UseFriendBitMap, FALSE,
			OBP_Precision, ami_dt_precision(),
			PDTA_ScaleQuality, ami_dt_scale_quality(),
			PDTA_DitherQuality, ami_dt_dither_quality(),
			TAG_DONE);

	if (dto == NULL) {
		dto = NewDTObject((APTR)filename,
				DTA_SourceType, DTST_FILE,
				DTA_GroupID, GID_PICTURE,
				PDTA_Remap, FALSE,
				PDTA_FreeSourceBitMap, FALSE,
				PDTA_DestMode, PMODE_V43,
				OBP_Precision, ami_dt_precision(),
				TAG_DONE);
	}

	if (dto == NULL) {
		NSLOG(netsurf, WARNING,
		      "amiga_dt_picture: NewDTObject(%s) IoErr=%ld",
		      filename, (long)IoErr());
	}

	return dto;
}

/**
 * Read nominal / BitMapHeader size from a freshly created (unlaid-out) dto.
 */
static BOOL
amiga_dt_picture_query_size(Object *dto, int *width, int *height)
{
	struct BitMapHeader *bmh;
	ULONG nom_w;
	ULONG nom_h;
	ULONG got;

	if (dto == NULL || width == NULL || height == NULL) {
		return FALSE;
	}

	*width = 0;
	*height = 0;
	nom_w = 0;
	nom_h = 0;
	got = GetDTAttrs(dto,
			DTA_NominalHoriz, &nom_w,
			DTA_NominalVert, &nom_h,
			TAG_DONE);
	if (got >= 2 && nom_w > 0 && nom_h > 0) {
		*width = (int)nom_w;
		*height = (int)nom_h;
		return TRUE;
	}

	bmh = NULL;
	got = GetDTAttrs(dto, PDTA_BitMapHeader, &bmh, TAG_DONE);
	if (got != 0 && bmh != NULL &&
	    bmh->bmh_Width > 0 && bmh->bmh_Height > 0) {
		*width = (int)bmh->bmh_Width;
		*height = (int)bmh->bmh_Height;
		return TRUE;
	}

	return FALSE;
}

/**
 * Cap size, optional PDTM_SCALE, then DTM_PROCLAYOUT (once).
 * *width / *height are in/out: desired size in, final size out.
 * Pass 0,0 to use natural size (still capped on OS3).
 */
static BOOL
amiga_dt_picture_scale_and_layout(Object *dto, int *width, int *height)
{
	struct gpLayout gpl;
	struct BitMapHeader *bmh;
	ULONG ok;
	ULONG got;
	int nat_w;
	int nat_h;
	int want_w;
	int want_h;

	if (dto == NULL || width == NULL || height == NULL) {
		return FALSE;
	}

	nat_w = 0;
	nat_h = 0;
	if (amiga_dt_picture_query_size(dto, &nat_w, &nat_h) == FALSE) {
		NSLOG(netsurf, WARNING,
		      "amiga_dt_picture: no size before layout");
		return FALSE;
	}

	want_w = *width;
	want_h = *height;
	if (want_w <= 0 || want_h <= 0) {
		want_w = nat_w;
		want_h = nat_h;
	}

#ifndef __amigaos4__
	ami_memory_cap_image_dims(&want_w, &want_h);
#endif

	/* Autodoc: PDTM_SCALE only before first GM_LAYOUT / DTM_PROCLAYOUT. */
	if (want_w != nat_w || want_h != nat_h) {
		NSLOG(netsurf, INFO,
		      "amiga_dt_picture: PDTM_SCALE before layout %dx%d -> %dx%d",
		      nat_w, nat_h, want_w, want_h);
		ok = DoMethod(dto, PDTM_SCALE,
				(ULONG)want_w, (ULONG)want_h, 0);
		if (ok == 0) {
			NSLOG(netsurf, WARNING,
			      "amiga_dt_picture: PDTM_SCALE failed; "
			      "layout at native %dx%d",
			      nat_w, nat_h);
			want_w = nat_w;
			want_h = nat_h;
		}
	}

	memset(&gpl, 0, sizeof(gpl));
	gpl.MethodID = DTM_PROCLAYOUT;
	gpl.gpl_GInfo = NULL;
	gpl.gpl_Initial = TRUE;
	ok = DoMethodA(dto, (Msg)&gpl);
	if (ok == 0) {
		NSLOG(netsurf, WARNING,
		      "amiga_dt_picture: DTM_PROCLAYOUT IoErr=%ld",
		      (long)IoErr());
		return FALSE;
	}

	/* Confirm post-layout size. */
	bmh = NULL;
	got = GetDTAttrs(dto, PDTA_BitMapHeader, &bmh, TAG_DONE);
	if (got != 0 && bmh != NULL &&
	    bmh->bmh_Width > 0 && bmh->bmh_Height > 0) {
		want_w = (int)bmh->bmh_Width;
		want_h = (int)bmh->bmh_Height;
	}

	*width = want_w;
	*height = want_h;

#ifndef __amigaos4__
	if (ami_memory_under_pressure()) {
		ami_memory_try_purge();
	}
#endif

	return TRUE;
}

/**
 * True if BitMap is safe for ReadPixel / plane walk without CyberGFX.
 * Chunky/hi-colour RTG BitMaps must not be sampled with ReadPixel.
 */
static BOOL amiga_dt_picture_bitmap_is_planar(struct BitMap *bm)
{
	ULONG flags;
	ULONG depth;

	if (bm == NULL) {
		return FALSE;
	}
	flags = GetBitMapAttr(bm, BMA_FLAGS);
	depth = GetBitMapAttr(bm, BMA_DEPTH);
	if ((flags & BMF_STANDARD) != 0 && depth <= 8UL) {
		return TRUE;
	}
	return FALSE;
}

/**
 * Convert planar BitMap + PDTA_CRegs to NetSurf ARGB (no CyberGFX).
 * Walks planes directly — safer than ReadPixel on odd BitMap layouts.
 */
static BOOL amiga_dt_picture_planar_to_argb(Object *dto, struct BitMap *bm,
		UBYTE *buf, int width, int height, ULONG stride)
{
	ULONG *cregs;
	UWORD numcols;
	PLANEPTR mask;
	ULONG mask_bpr;
	ULONG flags;
	ULONG depth;
	ULONG bm_w;
	ULONG bm_h;
	ULONG bpr;
	int x;
	int y;
	int d;
	ULONG *row;
	ULONG pen;
	ULONG a;
	ULONG r;
	ULONG g;
	ULONG b;
	ULONG byte_idx;
	UBYTE bit;
	UBYTE *plane;
	PLANEPTR *planes;
	struct BitMapHeader *bmh;
	UBYTE masking;
	UWORD transp_pen;
	BOOL use_transp_pen;

	cregs = NULL;
	numcols = 0;
	mask = NULL;
	mask_bpr = 0;
	bmh = NULL;
	masking = mskNone;
	transp_pen = 0;
	use_transp_pen = FALSE;

	bm_w = GetBitMapAttr(bm, BMA_WIDTH);
	bm_h = GetBitMapAttr(bm, BMA_HEIGHT);
	depth = GetBitMapAttr(bm, BMA_DEPTH);
	flags = GetBitMapAttr(bm, BMA_FLAGS);
	bpr = (ULONG)bm->BytesPerRow;
	planes = bm->Planes;

	if ((ULONG)width > bm_w) {
		width = (int)bm_w;
	}
	if ((ULONG)height > bm_h) {
		height = (int)bm_h;
	}
	if (depth > 8UL) {
		depth = 8UL;
	}
	if (bpr == 0 || planes == NULL) {
		return FALSE;
	}

	GetDTAttrs(dto,
			PDTA_CRegs, &cregs,
			PDTA_NumColors, &numcols,
			PDTA_MaskPlane, &mask,
			PDTA_BitMapHeader, &bmh,
			TAG_DONE);
	if (cregs == NULL) {
		GetDTAttrs(dto, PDTA_GRegs, &cregs, TAG_DONE);
	}
	if (cregs == NULL || numcols == 0) {
		NSLOG(netsurf, WARNING,
		      "amiga_dt_picture: planar BitMap but no CRegs");
		return FALSE;
	}

	if (bmh != NULL) {
		masking = bmh->bmh_Masking;
		transp_pen = bmh->bmh_Transparent;
		if (masking == mskHasTransparentColor ||
		    masking == mskLasso) {
			use_transp_pen = TRUE;
		}
	}

	/*
	 * Only honour MaskPlane when the header says the file has a mask.
	 * A non-NULL PDTA_MaskPlane alone is often a screen-threshold artefact.
	 */
	if (mask != NULL &&
	    masking != mskHasMask &&
	    masking != mskHasTransparentColor &&
	    masking != mskHasAlpha &&
	    masking != mskLasso) {
		mask = NULL;
	}

	if (mask != NULL) {
		mask_bpr = bm_w / 8UL;
		if ((flags & BMF_INTERLEAVED) != 0 && depth > 0) {
			mask_bpr /= depth;
		}
		if (mask_bpr == 0) {
			mask_bpr = (bm_w + 7UL) / 8UL;
		}
	}

	for (y = 0; y < height; y++) {
		row = (ULONG *)(buf + (ULONG)y * stride);
		for (x = 0; x < width; x++) {
			pen = 0;
			byte_idx = (ULONG)y * bpr + ((ULONG)x / 8UL);
			bit = (UBYTE)(0x80U >> (x & 7));

			for (d = 0; d < (int)depth; d++) {
				plane = (UBYTE *)planes[d];
				if (plane != NULL &&
				    (plane[byte_idx] & bit) != 0) {
					pen |= (1UL << d);
				}
			}

			r = 0;
			g = 0;
			b = 0;
			a = 0xffUL;
			if (pen < (ULONG)numcols) {
				r = cregs[pen * 3UL] >> 24;
				g = cregs[pen * 3UL + 1UL] >> 24;
				b = cregs[pen * 3UL + 2UL] >> 24;
			}

			if (mask != NULL && mask_bpr != 0) {
				byte_idx = (ULONG)y * mask_bpr +
						((ULONG)x / 8UL);
				bit = (UBYTE)(0x80U >> (x & 7));
				if ((mask[byte_idx] & bit) == 0) {
					a = 0;
				}
			} else if (use_transp_pen != FALSE &&
				   pen == (ULONG)transp_pen) {
				a = 0;
			}

			row[x] = (a << 24) | (r << 16) | (g << 8) | b;
		}
	}

	NSLOG(netsurf, INFO,
	      "amiga_dt_picture: planar transparency mask=%p masking=%u transp=%u",
	      (void *)mask, (unsigned)masking, (unsigned)transp_pen);

	return TRUE;
}

/**
 * Pull ARGB into NetSurf soft bitmap without requiring CyberGFX.
 * Never ReadPixel a chunky/hi-colour RTG BitMap (crashes on OS3).
 */
static BOOL amiga_dt_picture_read_pixels(Object *dto, UBYTE *buf,
		int width, int height, ULONG stride)
{
	struct BitMap *bm;
	struct BitMap *srcbm;
	ULONG got;
	ULONG ok;
	UBYTE *p;
	struct pdtBlitPixelArray pbpa;
	ULONG depth;

	memset(&pbpa, 0, sizeof(pbpa));
	pbpa.MethodID = PDTM_READPIXELARRAY;
	pbpa.pbpa_PixelData = buf;
	pbpa.pbpa_PixelFormat = PBPAFMT_ARGB;
	pbpa.pbpa_PixelArrayMod = stride;
	pbpa.pbpa_Left = 0;
	pbpa.pbpa_Top = 0;
	pbpa.pbpa_Width = (ULONG)width;
	pbpa.pbpa_Height = (ULONG)height;
	ok = DoMethodA(dto, (Msg)&pbpa);
	if (ok != 0) {
		NSLOG(netsurf, INFO,
		      "amiga_dt_picture: pixels via READPIXELARRAY ARGB");
		return TRUE;
	}

	pbpa.pbpa_PixelFormat = PBPAFMT_RGBA;
	ok = DoMethodA(dto, (Msg)&pbpa);
	if (ok != 0) {
		NSLOG(netsurf, INFO,
		      "amiga_dt_picture: pixels via READPIXELARRAY RGBA");
		return TRUE;
	}

	/* Prefer unremapped source BitMap (planar + CRegs). */
	srcbm = NULL;
	bm = NULL;
	got = GetDTAttrs(dto, PDTA_BitMap, &srcbm, TAG_DONE);
	if (got != 0 && amiga_dt_picture_bitmap_is_planar(srcbm)) {
		NSLOG(netsurf, INFO,
		      "amiga_dt_picture: pixels via planar BitMap+CRegs %dx%d",
		      width, height);
		return amiga_dt_picture_planar_to_argb(dto, srcbm, buf,
				width, height, stride);
	}

	got = GetDTAttrs(dto, PDTA_DestBitMap, &bm, TAG_DONE);
	if (got == 0 || bm == NULL) {
		bm = srcbm;
	}
	if (bm == NULL) {
		NSLOG(netsurf, WARNING,
		      "amiga_dt_picture: no BitMap after layout");
		return FALSE;
	}

	depth = GetBitMapAttr(bm, BMA_DEPTH);

	if (CyberGfxBase != NULL) {
		p = buf;
		ami_rtg_readpixelarray(bm, &p, (ULONG)width, (ULONG)height,
				stride, AMI_RTG_RECTFMT_ARGB);
		NSLOG(netsurf, INFO,
		      "amiga_dt_picture: pixels via CGX BitMap %dx%d depth=%lu",
		      width, height, (unsigned long)depth);
		return TRUE;
	}

	if (amiga_dt_picture_bitmap_is_planar(bm)) {
		NSLOG(netsurf, INFO,
		      "amiga_dt_picture: pixels via planar DestBitMap %dx%d",
		      width, height);
		return amiga_dt_picture_planar_to_argb(dto, bm, buf,
				width, height, stride);
	}

	/* Chunky/hi-colour without CyberGFX: do not call ReadPixel. */
	NSLOG(netsurf, WARNING,
	      "amiga_dt_picture: chunky BitMap depth=%lu without CyberGFX "
	      "(skip ReadPixel)",
	      (unsigned long)depth);
	return FALSE;
}

/**
 * Create picture.datatype object via DTST_FILE with a path that has a
 * real image extension. DTST_MEMORY loses to drawing.datatype
 * (IoErr ERROR_OBJECT_WRONG_TYPE / 212) on this OS3 setup.
 *
 * Does NOT layout — caller must amiga_dt_picture_scale_and_layout() so
 * PDTM_SCALE can run before the first DTM_PROCLAYOUT.
 *
 * - file://  -> open the Amiga path directly (no copy)
 * - http(s)  -> spill under keyed T:nsXXXXXXXX/ then DTST_FILE
 */
static Object *amiga_dt_picture_newdtobject(struct amiga_dt_picture_content *adt)
{
	const uint8_t *data;
	size_t size;
	Object *dto;
	char *path;
	nserror path_err;
	const char *ext;
	BPTR fh;
	LONG written;

	if (adt->dto != NULL) {
		return adt->dto;
	}

	data = content__get_source_data((struct content *)adt, &size);
	if (data == NULL || size == 0) {
		NSLOG(netsurf, WARNING,
		      "amiga_dt_picture: empty source (size=%lu)",
		      (unsigned long)size);
		return NULL;
	}

	path = NULL;
	dto = NULL;

	/* Local files: open the real Amiga path directly. */
	path_err = netsurf_nsurl_to_path(
			llcache_handle_get_url(adt->c.llcache), &path);
	if (path_err == NSERROR_OK && path != NULL) {
		dto = amiga_dt_picture_new_file(path);
		if (dto != NULL) {
			NSLOG(netsurf, INFO,
			      "amiga_dt_picture: NewDTObject file=%s size=%lu",
			      path, (unsigned long)size);
			free(path);
			adt->dto = dto;
			return adt->dto;
		}
		free(path);
		path = NULL;
	}

	/* Remote (or file open failed): spill under keyed T: folder. */
	if (amiga_dt_picture_ensure_spill_dir() == FALSE) {
		return NULL;
	}

	ext = amiga_dt_picture_ext(data, size);
	amiga_dt_spill_seq++;
	path = malloc(64);
	if (path == NULL) {
		return NULL;
	}
	sprintf(path, "%s/%lu.%s",
		amiga_dt_spill_dir,
		(unsigned long)amiga_dt_spill_seq, ext);

	fh = Open(path, MODE_NEWFILE);
	if (fh == 0) {
		NSLOG(netsurf, WARNING,
		      "amiga_dt_picture: Open(%s) IoErr=%ld",
		      path, (long)IoErr());
		free(path);
		return NULL;
	}
	written = Write(fh, (APTR)data, (LONG)size);
	Close(fh);
	if (written != (LONG)size) {
		NSLOG(netsurf, WARNING,
		      "amiga_dt_picture: Write(%s) %ld/%lu",
		      path, (long)written, (unsigned long)size);
		DeleteFile(path);
		free(path);
		return NULL;
	}

	dto = amiga_dt_picture_new_file(path);
	if (dto == NULL) {
		DeleteFile(path);
		free(path);
		return NULL;
	}

	adt->spill = path;
	adt->dto = dto;
	NSLOG(netsurf, DEBUG,
	      "amiga_dt_picture: NewDTObject spill=%s size=%lu",
	      path, (unsigned long)size);
	return adt->dto;
}

static char *amiga_dt_picture_datatype(struct content *c)
{
	lwc_string *mime;
	char *filetype;

	/* Avoid Lock + ObtainDataType on every image — that was a hot-path
	 * DOS/datatypes probe just for the status title string. */
	mime = content__get_mime_type(c);
	if (mime != NULL) {
		filetype = strdup(lwc_string_data(mime));
		if (filetype != NULL) {
			return filetype;
		}
	}

	filetype = strdup("DataTypes");
	return filetype;
}

/**
 * Pull ARGB soft buffer from an already-laid-out picture.datatype object.
 * Caller owns dto lifetime. On NOMEM broadcasts CONTENT_MSG_ERROR.
 */
static struct bitmap *amiga_dt_picture_bitmap_from_dto(struct content *c,
		Object *dto)
{
	union content_msg_data msg_data;
	UBYTE *bm_buffer;
	struct bitmap *bitmap;
	BOOL read_ok;
	const uint8_t *src;
	size_t src_size;

	bitmap = amiga_bitmap_create(c->width, c->height, BITMAP_NONE);
	if (bitmap == NULL) {
		msg_data.errordata.errorcode = NSERROR_NOMEM;
		msg_data.errordata.errormsg = messages_get("NoMemory");
		content_broadcast(c, CONTENT_MSG_ERROR, &msg_data);
		return NULL;
	}

	bm_buffer = amiga_bitmap_get_buffer(bitmap);
	read_ok = amiga_dt_picture_read_pixels(dto, bm_buffer,
			c->width, c->height,
			(ULONG)amiga_bitmap_get_rowstride(bitmap));
	if (read_ok == FALSE) {
		NSLOG(netsurf, WARNING,
		      "amiga_dt_picture: pixel read failed %dx%d",
		      c->width, c->height);
		amiga_bitmap_destroy(bitmap);
		return NULL;
	}

	src = content__get_source_data(c, &src_size);
	if (src != NULL &&
	    amiga_dt_picture_png_is_opaque_rgb(src, src_size) != FALSE) {
		/*
		 * amiga.com banner/button: 8-bit RGB interlaced, no tRNS.
		 * DT READPIXELARRAY yields black as 0x00000000; normalize
		 * would treat that as a hole and preblend to white.
		 */
		amiga_dt_picture_force_opaque_buffer(bitmap);
		NSLOG(netsurf, INFO,
		      "amiga_dt_picture: opaque RGB PNG %dx%d — forced a=0xff",
		      c->width, c->height);
	} else {
		amiga_bitmap_normalize_alpha(bitmap);
	}

	return bitmap;
}

/**
 * Scale dto to content intrinsic size when a spill reopen is full-res.
 * Must run before DTM_PROCLAYOUT (see amiga_dt_picture_scale_and_layout).
 */
static BOOL
amiga_dt_picture_layout_for_content(struct content *c, Object *dto)
{
	int width;
	int height;

	width = c->width;
	height = c->height;
	return amiga_dt_picture_scale_and_layout(dto, &width, &height);
}

static struct bitmap *amiga_dt_picture_cache_convert(struct content *c)
{
	Object *dto;
	struct bitmap *bitmap;
	struct amiga_dt_picture_content *adt =
			(struct amiga_dt_picture_content *)c;

	NSLOG(netsurf, INFO, "amiga_dt_picture_cache_convert");

	dto = amiga_dt_picture_newdtobject(adt);
	if (dto == NULL) {
		return NULL;
	}

	if (amiga_dt_picture_layout_for_content(c, dto) == FALSE) {
		DisposeDTObject(dto);
		adt->dto = NULL;
		return NULL;
	}

	bitmap = amiga_dt_picture_bitmap_from_dto(c, dto);

	/* Drop the DT object; keep spill so a later miss can reopen without
	 * depending on llcache still holding the source bytes. */
	DisposeDTObject(dto);
	adt->dto = NULL;

	return bitmap;
}

static bool amiga_dt_picture_convert(struct content *c)
{
	int width;
	int height;
	char *title;
	Object *dto;
	char *filetype;
	struct bitmap *bitmap;
	struct amiga_dt_picture_content *adt =
			(struct amiga_dt_picture_content *)c;

	NSLOG(netsurf, DEBUG, "amiga_dt_picture_convert");

	dto = amiga_dt_picture_newdtobject(adt);
	if (dto == NULL) {
		NSLOG(netsurf, WARNING,
		      "amiga_dt_picture_convert: NewDTObject failed for %s",
		      nsurl_access(llcache_handle_get_url(c->llcache)));
		return false;
	}

	/* 0,0 => natural size, capped, scaled before first PROCLAYOUT. */
	width = 0;
	height = 0;
	if (amiga_dt_picture_scale_and_layout(dto, &width, &height) == FALSE) {
		NSLOG(netsurf, WARNING,
		      "amiga_dt_picture_convert: scale/layout failed for %s",
		      nsurl_access(llcache_handle_get_url(c->llcache)));
		DisposeDTObject(dto);
		adt->dto = NULL;
		return false;
	}

	NSLOG(netsurf, DEBUG,
	      "amiga_dt_picture_convert: ok %dx%d", width, height);

	c->width = width;
	c->height = height;
	c->size = (size_t)width * (size_t)height * 4;

	/* Decode soft pixels now so READY/DONE means the image can paint.
	 * Leaving only dimensions forced first paint through cache_convert
	 * during box-build and left DT objects open for seconds on Aminet. */
	bitmap = amiga_dt_picture_bitmap_from_dto(c, dto);
	DisposeDTObject(dto);
	adt->dto = NULL;
	if (bitmap == NULL) {
		return false;
	}

	filetype = amiga_dt_picture_datatype(c);
	if (filetype != NULL) {
		title = messages_get_buff("DataTypesTitle",
			nsurl_access_leaf(llcache_handle_get_url(c->llcache)),
			filetype, c->width, c->height);
		if (title != NULL) {
			content__set_title(c, title);
			free(title);
		}
		free(filetype);
	}

	image_cache_add(c, bitmap, amiga_dt_picture_cache_convert);

	content_set_ready(c);
	content_set_done(c);
	content_set_status(c, "");
	return true;
}

static nserror amiga_dt_picture_clone(const struct content *old, struct content **newc)
{
	struct amiga_dt_picture_content *adt;
	nserror error;

	NSLOG(netsurf, INFO, "amiga_dt_picture_clone");

	adt = calloc(1, sizeof(struct amiga_dt_picture_content));
	if (adt == NULL)
		return NSERROR_NOMEM;

	error = content__clone(old, (struct content *)adt);
	if (error != NSERROR_OK) {
		content_destroy((struct content *)adt);
		return error;
	}

	if ((old->status == CONTENT_STATUS_READY) ||
	    (old->status == CONTENT_STATUS_DONE)) {
		if (amiga_dt_picture_convert((struct content *)adt) == false) {
			content_destroy((struct content *)adt);
			return NSERROR_CLONE_FAILED;
		}
	}

	*newc = (struct content *)adt;

	return NSERROR_OK;
}

static void amiga_dt_picture_destroy(struct content *c)
{
	struct amiga_dt_picture_content *adt = (struct amiga_dt_picture_content *)c;

	if (adt->dto != NULL) {
		DisposeDTObject(adt->dto);
		adt->dto = NULL;
	}
	if (adt->spill != NULL) {
		DeleteFile(adt->spill);
		free(adt->spill);
		adt->spill = NULL;
	}

	image_cache_destroy(c);
}

#endif
