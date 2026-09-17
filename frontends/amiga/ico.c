/*
 * Copyright 2026 AmigaZen / Kitsune
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

/**
 * \file
 * Built-in Windows .ico content handler for favicons.
 *
 * OS3 builds do not link libnsbmp, and picture.datatype rejects .ico
 * spill files (IoErr 212). Favicons are almost the only ICO use on the
 * web, so a small BMP-in-ICO decoder is enough. PNG-compressed ICO
 * entries are spilled as .png for picture.datatype when available.
 */

#include "amiga/os3support.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <proto/dos.h>
#include <proto/datatypes.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <datatypes/datatypesclass.h>
#include <datatypes/pictureclass.h>
#include <intuition/classusr.h>
#include <intuition/gadgetclass.h>

#include "utils/log.h"
#include "utils/messages.h"
#include "utils/utils.h"
#include "netsurf/bitmap.h"
#include "netsurf/content.h"
#include "netsurf/plotters.h"
#include "content/llcache.h"
#include "content/content.h"
#include "content/content_protected.h"
#include "content/content_factory.h"
#include "desktop/gui_internal.h"

#include "amiga/bitmap.h"
#include "amiga/gui.h"
#include "amiga/ico.h"

#ifndef PDTM_READPIXELARRAY
#define PDTM_READPIXELARRAY	(PDTM_Dummy + 1)
#endif
#ifndef PBPAFMT_ARGB
#define PBPAFMT_ARGB		0
#endif
#ifndef PBPAFMT_RGBA
#define PBPAFMT_RGBA		1
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

typedef struct amiga_ico_content {
	struct content base;
	struct bitmap *bitmap;
} amiga_ico_content;

struct ico_dirent {
	unsigned int width;
	unsigned int height;
	unsigned int bpp;
	unsigned int bytes;
	unsigned int offset;
	int is_png;
};

static nserror amiga_ico_create(const content_handler *handler,
		lwc_string *imime_type, const struct http_parameter *params,
		llcache_handle *llcache, const char *fallback_charset,
		bool quirks, struct content **c);
static bool amiga_ico_convert(struct content *c);
static void amiga_ico_destroy(struct content *c);
static bool amiga_ico_redraw(struct content *c,
		struct content_redraw_data *data, const struct rect *clip,
		const struct redraw_context *ctx);
static nserror amiga_ico_clone(const struct content *old, struct content **newc);
static content_type amiga_ico_content_type(void);
static void *amiga_ico_get_internal(const struct content *c, void *context);
static bool amiga_ico_is_opaque(struct content *c);

static const content_handler amiga_ico_content_handler = {
	.create = amiga_ico_create,
	.data_complete = amiga_ico_convert,
	.destroy = amiga_ico_destroy,
	.redraw = amiga_ico_redraw,
	.clone = amiga_ico_clone,
	.get_internal = amiga_ico_get_internal,
	.type = amiga_ico_content_type,
	.is_opaque = amiga_ico_is_opaque,
	.no_share = false,
};

static const char *amiga_ico_types[] = {
	"application/ico",
	"application/x-ico",
	"image/ico",
	"image/vnd.microsoft.icon",
	"image/x-icon"
};

CONTENT_FACTORY_REGISTER_TYPES(amiga_ico, amiga_ico_types,
		amiga_ico_content_handler)

static unsigned int
amiga_ico_rd16(const uint8_t *p)
{
	return (unsigned int)p[0] | ((unsigned int)p[1] << 8);
}

static unsigned int
amiga_ico_rd32(const uint8_t *p)
{
	return (unsigned int)p[0] |
		((unsigned int)p[1] << 8) |
		((unsigned int)p[2] << 16) |
		((unsigned int)p[3] << 24);
}

/**
 * Score an icon candidate: prefer ~16px favicon size, then nearby sizes.
 */
static int
amiga_ico_score(unsigned int w, unsigned int h, int want)
{
	int s;
	int d;

	s = (int)((w > h) ? w : h);
	d = s - want;
	if (d < 0) {
		d = -d;
	}
	/* Prefer at least want pixels; penalise huge icons harder. */
	if (s < want) {
		return 1000 + d;
	}
	return d;
}

static int
amiga_ico_parse_dir(const uint8_t *data, size_t size,
		struct ico_dirent *ents, int max_ents)
{
	unsigned int type;
	unsigned int count;
	unsigned int i;
	unsigned int off;
	unsigned int w;
	unsigned int h;
	unsigned int bytes;
	unsigned int ioff;
	int n;

	if (size < 6) {
		return -1;
	}
	if (amiga_ico_rd16(data) != 0) {
		return -1;
	}
	type = amiga_ico_rd16(data + 2);
	if (type != 1) {
		return -1;
	}
	count = amiga_ico_rd16(data + 4);
	if (count == 0 || count > 64) {
		return -1;
	}
	if (size < 6 + (size_t)count * 16) {
		return -1;
	}

	n = 0;
	for (i = 0; i < count && n < max_ents; i++) {
		off = 6 + i * 16;
		w = data[off];
		h = data[off + 1];
		if (w == 0) {
			w = 256;
		}
		if (h == 0) {
			h = 256;
		}
		bytes = amiga_ico_rd32(data + off + 8);
		ioff = amiga_ico_rd32(data + off + 12);
		if (bytes < 8 || ioff >= size ||
		    bytes > size - ioff) {
			continue;
		}
		ents[n].width = w;
		ents[n].height = h;
		ents[n].bpp = amiga_ico_rd16(data + off + 6);
		ents[n].bytes = bytes;
		ents[n].offset = ioff;
		ents[n].is_png = 0;
		if (bytes >= 8 &&
		    data[ioff] == 0x89 && data[ioff + 1] == 'P' &&
		    data[ioff + 2] == 'N' && data[ioff + 3] == 'G') {
			ents[n].is_png = 1;
		}
		n++;
	}
	return n;
}

/**
 * Decode XOR bitmap + AND mask into NetSurf ARGB (A in high byte).
 */
static bool
amiga_ico_decode_bmp(const uint8_t *img, unsigned int bytes,
		unsigned int *out_w, unsigned int *out_h,
		uint8_t **out_pix)
{
	unsigned int hdr;
	unsigned int w;
	unsigned int h_full;
	unsigned int h;
	unsigned int bpp;
	unsigned int colors;
	unsigned int xor_stride;
	unsigned int and_stride;
	unsigned int pal_bytes;
	unsigned int xor_off;
	unsigned int and_off;
	unsigned int need;
	unsigned int x;
	unsigned int y;
	unsigned int idx;
	unsigned int bit;
	const uint8_t *pal;
	const uint8_t *xorp;
	const uint8_t *andp;
	uint8_t *pix;
	uint8_t *row;
	uint8_t a;
	uint8_t r;
	uint8_t g;
	uint8_t b;

	if (bytes < 40) {
		return false;
	}
	hdr = amiga_ico_rd32(img);
	if (hdr < 40) {
		return false;
	}
	w = amiga_ico_rd32(img + 4);
	h_full = amiga_ico_rd32(img + 8);
	/* ICO stores height as image*2 (XOR + AND). */
	if (h_full == 0) {
		return false;
	}
	h = h_full / 2;
	if (h == 0) {
		h = h_full;
	}
	if (w == 0 || w > 256 || h > 256) {
		return false;
	}
	bpp = amiga_ico_rd16(img + 14);
	if (bpp != 1 && bpp != 4 && bpp != 8 && bpp != 24 && bpp != 32) {
		return false;
	}
	colors = amiga_ico_rd32(img + 32);
	if (colors == 0 && bpp <= 8) {
		colors = 1U << bpp;
	}
	if (bpp > 8) {
		colors = 0;
	}

	xor_stride = ((w * bpp + 31) / 32) * 4;
	and_stride = ((w + 31) / 32) * 4;
	pal_bytes = colors * 4;
	xor_off = hdr + pal_bytes;
	and_off = xor_off + xor_stride * h;
	need = and_off + and_stride * h;
	if (need > bytes) {
		/* Some writers omit AND for 32bpp; still require XOR. */
		if (bpp == 32 && xor_off + xor_stride * h <= bytes) {
			and_off = 0;
		} else {
			return false;
		}
	}

	pal = img + hdr;
	xorp = img + xor_off;
	andp = (and_off != 0) ? (img + and_off) : NULL;

	pix = malloc((size_t)w * (size_t)h * 4);
	if (pix == NULL) {
		return false;
	}

	for (y = 0; y < h; y++) {
		/* BMP rows are bottom-up. */
		row = pix + (size_t)(h - 1 - y) * (size_t)w * 4;
		for (x = 0; x < w; x++) {
			r = 0;
			g = 0;
			b = 0;
			a = 0xff;
			if (bpp == 32) {
				b = xorp[y * xor_stride + x * 4 + 0];
				g = xorp[y * xor_stride + x * 4 + 1];
				r = xorp[y * xor_stride + x * 4 + 2];
				a = xorp[y * xor_stride + x * 4 + 3];
			} else if (bpp == 24) {
				b = xorp[y * xor_stride + x * 3 + 0];
				g = xorp[y * xor_stride + x * 3 + 1];
				r = xorp[y * xor_stride + x * 3 + 2];
			} else if (bpp == 8) {
				idx = xorp[y * xor_stride + x];
				if (idx < colors) {
					b = pal[idx * 4 + 0];
					g = pal[idx * 4 + 1];
					r = pal[idx * 4 + 2];
				}
			} else if (bpp == 4) {
				idx = xorp[y * xor_stride + x / 2];
				if ((x & 1) == 0) {
					idx = (idx >> 4) & 0x0f;
				} else {
					idx = idx & 0x0f;
				}
				if (idx < colors) {
					b = pal[idx * 4 + 0];
					g = pal[idx * 4 + 1];
					r = pal[idx * 4 + 2];
				}
			} else { /* 1bpp */
				bit = 7 - (x & 7);
				idx = (xorp[y * xor_stride + x / 8] >> bit) & 1;
				if (idx < colors) {
					b = pal[idx * 4 + 0];
					g = pal[idx * 4 + 1];
					r = pal[idx * 4 + 2];
				}
			}

			if (andp != NULL && bpp < 32) {
				bit = 7 - (x & 7);
				if ((andp[y * and_stride + x / 8] >> bit) & 1) {
					/* Clear RGB too — leftover XOR under AND
					 * becomes hot pink once remapped opaque. */
					a = 0;
					r = 0;
					g = 0;
					b = 0;
				}
			} else if (a == 0) {
				r = 0;
				g = 0;
				b = 0;
			}
			/* NetSurf Amiga soft buffer: AARRGGBB as bytes A,R,G,B */
			row[x * 4 + 0] = a;
			row[x * 4 + 1] = r;
			row[x * 4 + 2] = g;
			row[x * 4 + 3] = b;
		}
	}

	*out_w = w;
	*out_h = h;
	*out_pix = pix;
	return true;
}

/**
 * Try picture.datatype on an embedded PNG blob (modern favicons).
 */
static bool
amiga_ico_decode_png_via_dt(const uint8_t *png, unsigned int bytes,
		unsigned int *out_w, unsigned int *out_h,
		uint8_t **out_pix)
{
	char path[48];
	BPTR fh;
	Object *dto;
	struct BitMapHeader *bmh;
	struct pdtBlitPixelArray pbpa;
	struct gpLayout gpl;
	ULONG ok;
	ULONG w;
	ULONG h;
	uint8_t *pix;
	struct Screen *screen;

	sprintf(path, "T:nsico%08lx.png", (unsigned long)FindTask(NULL));
	fh = Open(path, MODE_NEWFILE);
	if (fh == 0) {
		return false;
	}
	if ((ULONG)Write(fh, (APTR)png, bytes) != bytes) {
		Close(fh);
		DeleteFile(path);
		return false;
	}
	Close(fh);

	screen = ami_gui_get_screen();
	dto = NewDTObject((APTR)path,
			DTA_SourceType, DTST_FILE,
			DTA_GroupID, GID_PICTURE,
			PDTA_Screen, screen,
			PDTA_Remap, FALSE,
			PDTA_DestMode, PMODE_V43,
			TAG_DONE);
	DeleteFile(path);
	if (dto == NULL) {
		return false;
	}

	memset(&gpl, 0, sizeof(gpl));
	gpl.MethodID = DTM_PROCLAYOUT;
	gpl.gpl_GInfo = NULL;
	gpl.gpl_Initial = TRUE;
	ok = DoMethodA(dto, (Msg)&gpl);
	if (ok == 0) {
		DisposeDTObject(dto);
		return false;
	}

	bmh = NULL;
	GetDTAttrs(dto, PDTA_BitMapHeader, &bmh, TAG_DONE);
	if (bmh == NULL || bmh->bmh_Width == 0 || bmh->bmh_Height == 0) {
		DisposeDTObject(dto);
		return false;
	}
	w = bmh->bmh_Width;
	h = bmh->bmh_Height;
	if (w > 256 || h > 256) {
		DisposeDTObject(dto);
		return false;
	}

	pix = malloc((size_t)w * (size_t)h * 4);
	if (pix == NULL) {
		DisposeDTObject(dto);
		return false;
	}
	memset(pix, 0, (size_t)w * (size_t)h * 4);

	memset(&pbpa, 0, sizeof(pbpa));
	pbpa.MethodID = PDTM_READPIXELARRAY;
	pbpa.pbpa_PixelData = pix;
	pbpa.pbpa_PixelFormat = PBPAFMT_ARGB;
	pbpa.pbpa_PixelArrayMod = w * 4;
	pbpa.pbpa_Left = 0;
	pbpa.pbpa_Top = 0;
	pbpa.pbpa_Width = w;
	pbpa.pbpa_Height = h;
	ok = DoMethodA(dto, (Msg)&pbpa);
	if (ok == 0) {
		pbpa.pbpa_PixelFormat = PBPAFMT_RGBA;
		ok = DoMethodA(dto, (Msg)&pbpa);
	}
	DisposeDTObject(dto);
	if (ok == 0) {
		free(pix);
		return false;
	}

	*out_w = (unsigned int)w;
	*out_h = (unsigned int)h;
	*out_pix = pix;
	return true;
}

static bool
amiga_ico_pick_and_decode(const uint8_t *data, size_t size,
		unsigned int *out_w, unsigned int *out_h,
		uint8_t **out_pix)
{
	struct ico_dirent ents[64];
	int n;
	int i;
	int best;
	int best_score;
	int score;
	int pass;

	n = amiga_ico_parse_dir(data, size, ents, 64);
	if (n <= 0) {
		return false;
	}

	/* Pass 0: prefer BMP entries (always decodable). Pass 1: any. */
	for (pass = 0; pass < 2; pass++) {
		best = -1;
		best_score = 0x7fffffff;
		for (i = 0; i < n; i++) {
			if (pass == 0 && ents[i].is_png) {
				continue;
			}
			score = amiga_ico_score(ents[i].width,
					ents[i].height, 16);
			if (score < best_score) {
				best_score = score;
				best = i;
			}
		}
		if (best < 0) {
			continue;
		}

		if (ents[best].is_png) {
			if (amiga_ico_decode_png_via_dt(
					data + ents[best].offset,
					ents[best].bytes,
					out_w, out_h, out_pix)) {
				return true;
			}
		} else {
			if (amiga_ico_decode_bmp(
					data + ents[best].offset,
					ents[best].bytes,
					out_w, out_h, out_pix)) {
				return true;
			}
		}
	}

	/* Last resort: try every entry. */
	for (i = 0; i < n; i++) {
		if (ents[i].is_png) {
			if (amiga_ico_decode_png_via_dt(
					data + ents[i].offset,
					ents[i].bytes,
					out_w, out_h, out_pix)) {
				return true;
			}
		} else if (amiga_ico_decode_bmp(
				data + ents[i].offset,
				ents[i].bytes,
				out_w, out_h, out_pix)) {
			return true;
		}
	}
	return false;
}

static nserror
amiga_ico_create(const content_handler *handler,
		lwc_string *imime_type, const struct http_parameter *params,
		llcache_handle *llcache, const char *fallback_charset,
		bool quirks, struct content **c)
{
	amiga_ico_content *ico;
	nserror error;

	ico = calloc(1, sizeof(amiga_ico_content));
	if (ico == NULL) {
		return NSERROR_NOMEM;
	}

	error = content__init(&ico->base, handler, imime_type, params,
			llcache, fallback_charset, quirks);
	if (error != NSERROR_OK) {
		free(ico);
		return error;
	}

	*c = (struct content *)ico;
	return NSERROR_OK;
}

static bool
amiga_ico_convert(struct content *c)
{
	amiga_ico_content *ico = (amiga_ico_content *)c;
	const uint8_t *data;
	size_t size;
	unsigned int w;
	unsigned int h;
	uint8_t *pix;
	uint8_t *dst;
	char *title;
	bool opaque;
	unsigned int i;
	unsigned int n;

	data = content__get_source_data(c, &size);
	if (data == NULL || size == 0) {
		content_broadcast_error(c, NSERROR_ICO_ERROR, NULL);
		return false;
	}

	pix = NULL;
	w = 0;
	h = 0;
	if (!amiga_ico_pick_and_decode(data, size, &w, &h, &pix)) {
		NSLOG(netsurf, WARNING, "amiga_ico: decode failed (%lu bytes)",
		      (unsigned long)size);
		content_broadcast_error(c, NSERROR_ICO_ERROR, NULL);
		return false;
	}

	ico->bitmap = amiga_bitmap_create((int)w, (int)h, BITMAP_NONE);
	if (ico->bitmap == NULL) {
		free(pix);
		content_broadcast_error(c, NSERROR_NOMEM, NULL);
		return false;
	}

	dst = amiga_bitmap_get_buffer(ico->bitmap);
	memcpy(dst, pix, (size_t)w * (size_t)h * 4);
	free(pix);

	opaque = true;
	n = w * h;
	for (i = 0; i < n; i++) {
		if (dst[i * 4] != 0xff) {
			opaque = false;
			break;
		}
	}
	amiga_bitmap_set_opaque(ico->bitmap, opaque);
	amiga_bitmap_modified(ico->bitmap);

	c->width = (int)w;
	c->height = (int)h;
	c->size += (size_t)w * (size_t)h * 4;

	title = messages_get_buff("ICOTitle",
			nsurl_access_leaf(llcache_handle_get_url(c->llcache)),
			c->width, c->height);
	if (title != NULL) {
		content__set_title(c, title);
		free(title);
	}

	content_set_ready(c);
	content_set_done(c);
	content_set_status(c, "");
	NSLOG(netsurf, INFO, "amiga_ico: decoded %ux%u favicon", w, h);
	return true;
}

static void
amiga_ico_destroy(struct content *c)
{
	amiga_ico_content *ico = (amiga_ico_content *)c;

	if (ico->bitmap != NULL) {
		amiga_bitmap_destroy(ico->bitmap);
		ico->bitmap = NULL;
	}
}

static bool
amiga_ico_redraw(struct content *c, struct content_redraw_data *data,
		const struct rect *clip, const struct redraw_context *ctx)
{
	amiga_ico_content *ico = (amiga_ico_content *)c;
	bitmap_flags_t flags = BITMAPF_NONE;

	if (ico->bitmap == NULL) {
		return false;
	}
	if (data->repeat_x) {
		flags |= BITMAPF_REPEAT_X;
	}
	if (data->repeat_y) {
		flags |= BITMAPF_REPEAT_Y;
	}
	return (ctx->plot->bitmap(ctx, ico->bitmap,
				  data->x, data->y,
				  data->width, data->height,
				  data->background_colour,
				  flags) == NSERROR_OK);
}

static nserror
amiga_ico_clone(const struct content *old, struct content **newc)
{
	amiga_ico_content *ico;
	nserror error;

	ico = calloc(1, sizeof(amiga_ico_content));
	if (ico == NULL) {
		return NSERROR_NOMEM;
	}

	error = content__clone(old, &ico->base);
	if (error != NSERROR_OK) {
		content_destroy(&ico->base);
		return error;
	}

	if (old->status == CONTENT_STATUS_READY ||
	    old->status == CONTENT_STATUS_DONE) {
		if (amiga_ico_convert(&ico->base) == false) {
			content_destroy(&ico->base);
			return NSERROR_CLONE_FAILED;
		}
	}

	*newc = (struct content *)ico;
	return NSERROR_OK;
}

static void *
amiga_ico_get_internal(const struct content *c, void *context)
{
	amiga_ico_content *ico = (amiga_ico_content *)c;

	return ico->bitmap;
}

static content_type
amiga_ico_content_type(void)
{
	return CONTENT_IMAGE;
}

static bool
amiga_ico_is_opaque(struct content *c)
{
	amiga_ico_content *ico = (amiga_ico_content *)c;

	if (ico->bitmap == NULL) {
		return false;
	}
	return amiga_bitmap_get_opaque(ico->bitmap);
}
