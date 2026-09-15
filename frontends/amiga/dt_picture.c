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
#ifndef PDTA_AlphaChannel
#define PDTA_AlphaChannel	TAG_IGNORE
#endif
#ifndef PDTA_MaskPlane
#define PDTA_MaskPlane		TAG_IGNORE
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
#include "amiga/gui.h"
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
	char *spill; /* RAM: file we created; NULL if using original file:// path */
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
		"image/x-icon",
		"image/vnd.microsoft.icon",
		"image/webp",
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
 * uses extension + DTST_FILE.
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
	if (size >= 4 && data[0] == 0x00 && data[1] == 0x00 &&
	    data[2] == 0x01 && data[3] == 0x00) {
		return "ico";
	}
	return "pic";
}

/* Keyed spill directory under T: for this NetSurf process (not RAM:). */
static char amiga_dt_spill_dir[40];
static BOOL amiga_dt_spill_ready;
static ULONG amiga_dt_spill_seq;

/**
 * Ensure T:nsXXXXXXXX/ exists (key = FindTask address). Files land inside.
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
	return TRUE;
}

/**
 * Soft-ARGB path needs the unremapped source bitmap + PDTA_CRegs.
 * Remapping TRUE for direct screen blit would bake screen pens into
 * the buffer and double-remap through guigfx.
 */
static Object *amiga_dt_picture_open_file(const char *filename,
		struct Screen *screen)
{
	Object *dto;
	struct gpLayout gpl;
	ULONG ok;

	(void)screen;

	dto = NewDTObject((APTR)filename,
			DTA_SourceType, DTST_FILE,
			DTA_GroupID, GID_PICTURE,
			PDTA_Remap, FALSE,
			PDTA_FreeSourceBitMap, FALSE,
			PDTA_DestMode, PMODE_V43,
			PDTA_UseFriendBitMap, FALSE,
			OBP_Precision, PRECISION_IMAGE,
			TAG_DONE);

	if (dto == NULL) {
		dto = NewDTObject((APTR)filename,
				DTA_SourceType, DTST_FILE,
				DTA_GroupID, GID_PICTURE,
				PDTA_Remap, FALSE,
				TAG_DONE);
	}

	if (dto == NULL) {
		NSLOG(netsurf, WARNING,
		      "amiga_dt_picture: NewDTObject(%s) IoErr=%ld",
		      filename, (long)IoErr());
		return NULL;
	}

	memset(&gpl, 0, sizeof(gpl));
	gpl.MethodID = DTM_PROCLAYOUT;
	gpl.gpl_GInfo = NULL;
	gpl.gpl_Initial = TRUE;
	ok = DoMethodA(dto, (Msg)&gpl);
	if (ok == 0) {
		NSLOG(netsurf, WARNING,
		      "amiga_dt_picture: DTM_PROCLAYOUT(%s) IoErr=%ld",
		      filename, (long)IoErr());
		DisposeDTObject(dto);
		return NULL;
	}

	return dto;
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

	cregs = NULL;
	numcols = 0;
	mask = NULL;
	mask_bpr = 0;

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
			TAG_DONE);
	if (cregs == NULL) {
		GetDTAttrs(dto, PDTA_GRegs, &cregs, TAG_DONE);
	}
	if (cregs == NULL || numcols == 0) {
		NSLOG(netsurf, WARNING,
		      "amiga_dt_picture: planar BitMap but no CRegs");
		return FALSE;
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

			if ((flags & BMF_INTERLEAVED) != 0) {
				/* Interleaved: plane d at offset d * (height*mod) — use
				 * BytesPerRow as total; fall back to ReadPixel-style via
				 * per-plane pointers when Planes[d] are distinct.
				 */
				for (d = 0; d < (int)depth; d++) {
					plane = (UBYTE *)planes[d];
					if (plane != NULL &&
					    (plane[byte_idx] & bit) != 0) {
						pen |= (1UL << d);
					}
				}
			} else {
				for (d = 0; d < (int)depth; d++) {
					plane = (UBYTE *)planes[d];
					if (plane != NULL &&
					    (plane[byte_idx] & bit) != 0) {
						pen |= (1UL << d);
					}
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
			}

			row[x] = (a << 24) | (r << 16) | (g << 8) | b;
		}
	}

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
 * - file://  -> open the Amiga path directly (no copy)
 * - http(s)  -> spill under keyed T:nsXXXXXXXX/ then DTST_FILE
 */
static Object *amiga_dt_picture_newdtobject(struct amiga_dt_picture_content *adt)
{
	const uint8_t *data;
	size_t size;
	Object *dto;
	struct Screen *screen;
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

	screen = ami_gui_get_screen();
	path = NULL;
	dto = NULL;

	/* Local files: open the real Amiga path directly. */
	path_err = netsurf_nsurl_to_path(
			llcache_handle_get_url(adt->c.llcache), &path);
	if (path_err == NSERROR_OK && path != NULL) {
		dto = amiga_dt_picture_open_file(path, screen);
		if (dto != NULL) {
			NSLOG(netsurf, INFO,
			      "amiga_dt_picture: layout ok file=%s size=%lu",
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

	dto = amiga_dt_picture_open_file(path, screen);
	if (dto == NULL) {
		DeleteFile(path);
		free(path);
		return NULL;
	}

	adt->spill = path;
	adt->dto = dto;
	NSLOG(netsurf, INFO,
	      "amiga_dt_picture: layout ok spill=%s size=%lu",
	      path, (unsigned long)size);
	return adt->dto;
}

static char *amiga_dt_picture_datatype(struct content *c)
{
	struct amiga_dt_picture_content *adt = (struct amiga_dt_picture_content *)c;
	struct DataType *dt;
	char *filetype = NULL;
	const char *path;
	char *p;
	BPTR lock;

	path = adt->spill;
	p = NULL;
	if (path == NULL) {
		if (netsurf_nsurl_to_path(llcache_handle_get_url(c->llcache),
					 &p) == NSERROR_OK) {
			path = p;
			lock = Lock(path, ACCESS_READ);
			if (lock != 0) {
				dt = ObtainDataType(DTST_FILE, (APTR)lock,
						DTA_GroupID, GID_PICTURE,
						TAG_DONE);
				UnLock(lock);
				if (dt != NULL) {
					filetype = strdup(dt->dtn_Header->dth_Name);
					ReleaseDataType(dt);
				}
			}
			free(p);
			path = NULL;
		}
	} else {
		lock = Lock(path, ACCESS_READ);
		if (lock != 0) {
			dt = ObtainDataType(DTST_FILE, (APTR)lock,
					DTA_GroupID, GID_PICTURE,
					TAG_DONE);
			UnLock(lock);
			if (dt != NULL) {
				filetype = strdup(dt->dtn_Header->dth_Name);
				ReleaseDataType(dt);
			}
		}
	}

	if (filetype == NULL) {
		filetype = strdup("DataTypes");
	}
	return filetype;
}

static struct bitmap *amiga_dt_picture_cache_convert(struct content *c)
{
	union content_msg_data msg_data;
	UBYTE *bm_buffer;
	Object *dto;
	struct bitmap *bitmap;
	struct amiga_dt_picture_content *adt = (struct amiga_dt_picture_content *)c;
	BOOL has_alpha = FALSE;
	ULONG got;
	BOOL read_ok;

	NSLOG(netsurf, INFO, "amiga_dt_picture_cache_convert");

	dto = amiga_dt_picture_newdtobject(adt);
	if (dto == NULL) {
		return NULL;
	}

	bitmap = amiga_bitmap_create(c->width, c->height, BITMAP_NONE);
	if (!bitmap) {
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

	got = GetDTAttrs(dto, PDTA_AlphaChannel, &has_alpha, TAG_DONE);
	if (got == 1) {
		amiga_bitmap_set_opaque(bitmap, has_alpha == FALSE);
	} else {
		amiga_bitmap_set_opaque(bitmap, bitmap_test_opaque(bitmap));
	}

	DisposeDTObject(dto);
	adt->dto = NULL;
	if (adt->spill != NULL) {
		DeleteFile(adt->spill);
		free(adt->spill);
		adt->spill = NULL;
	}

	return bitmap;
}

static bool amiga_dt_picture_convert(struct content *c)
{
	int width, height;
	char *title;
	Object *dto;
	struct BitMapHeader *bmh;
	char *filetype;
	ULONG got;
	ULONG nom_w, nom_h;
	struct amiga_dt_picture_content *adt = (struct amiga_dt_picture_content *)c;

	NSLOG(netsurf, INFO, "amiga_dt_picture_convert");

	dto = amiga_dt_picture_newdtobject(adt);
	if (dto == NULL) {
		NSLOG(netsurf, WARNING,
		      "amiga_dt_picture_convert: open/layout failed for %s",
		      nsurl_access(llcache_handle_get_url(c->llcache)));
		return false;
	}

	width = 0;
	height = 0;
	nom_w = 0;
	nom_h = 0;
	got = GetDTAttrs(dto,
			DTA_NominalHoriz, &nom_w,
			DTA_NominalVert, &nom_h,
			TAG_DONE);
	if (got >= 2 && nom_w > 0 && nom_h > 0) {
		width = (int)nom_w;
		height = (int)nom_h;
	}

	if (width <= 0 || height <= 0) {
		bmh = NULL;
		got = GetDTAttrs(dto, PDTA_BitMapHeader, &bmh, TAG_DONE);
		if (got != 0 && bmh != NULL) {
			width = (int)bmh->bmh_Width;
			height = (int)bmh->bmh_Height;
		}
	}

	if (width <= 0 || height <= 0) {
		NSLOG(netsurf, WARNING,
		      "amiga_dt_picture_convert: no size after layout for %s",
		      nsurl_access(llcache_handle_get_url(c->llcache)));
		return false;
	}

	NSLOG(netsurf, INFO,
	      "amiga_dt_picture_convert: ok %dx%d", width, height);

	c->width = width;
	c->height = height;
	c->size = (size_t)width * (size_t)height * 4;

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

	image_cache_add(c, NULL, amiga_dt_picture_cache_convert);

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
