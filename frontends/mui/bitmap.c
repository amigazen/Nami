/*
 * Copyright 2026 amigazen project
 *
 * This file is part of NetSurf / Tsunami (MUI frontend).
 *
 * In-memory ARGB bitmaps for the core. Blitting into the MUI Area is
 * handled by the plotter table during browser_window_redraw.
 */

#include <stdbool.h>
#include <stdlib.h>

#include "utils/errors.h"
#include "netsurf/bitmap.h"
#include "mui/bitmap.h"

struct bitmap {
	void *ptr;
	size_t rowstride;
	int width;
	int height;
	bool opaque;
};

static void *bitmap_create(int width, int height, enum gui_bitmap_flags flags)
{
	struct bitmap *ret;

	ret = calloc(1, sizeof(*ret));
	if (ret == NULL) {
		return NULL;
	}

	ret->width = width;
	ret->height = height;
	ret->opaque = (flags & BITMAP_OPAQUE) == BITMAP_OPAQUE;
	ret->rowstride = (size_t)width * 4;
	ret->ptr = calloc((size_t)width, (size_t)height * 4);
	if (ret->ptr == NULL) {
		free(ret);
		return NULL;
	}

	return ret;
}

static void bitmap_destroy(void *bitmap)
{
	struct bitmap *bmap;

	bmap = bitmap;
	free(bmap->ptr);
	free(bmap);
}

static void bitmap_set_opaque(void *bitmap, bool opaque)
{
	struct bitmap *bmap;

	bmap = bitmap;
	bmap->opaque = opaque;
}

static bool bitmap_get_opaque(void *bitmap)
{
	struct bitmap *bmap;

	bmap = bitmap;
	return bmap->opaque;
}

unsigned char *bitmap_get_buffer(void *bitmap)
{
	struct bitmap *bmap;

	bmap = bitmap;
	if (bmap == NULL) {
		return NULL;
	}
	return (unsigned char *)(bmap->ptr);
}

size_t bitmap_get_rowstride(void *bitmap)
{
	struct bitmap *bmap;

	bmap = bitmap;
	if (bmap == NULL) {
		return 0;
	}
	return bmap->rowstride;
}

static void bitmap_modified(void *bitmap)
{
	(void)bitmap;
}

int bitmap_get_width(void *bitmap)
{
	struct bitmap *bmap;

	bmap = bitmap;
	if (bmap == NULL) {
		return 0;
	}
	return bmap->width;
}

int bitmap_get_height(void *bitmap)
{
	struct bitmap *bmap;

	bmap = bitmap;
	if (bmap == NULL) {
		return 0;
	}
	return bmap->height;
}

static nserror bitmap_render(struct bitmap *bitmap,
			     struct hlcache_handle *content)
{
	(void)bitmap;
	(void)content;
	return NSERROR_OK;
}

static struct gui_bitmap_table bitmap_table = {
	.create = bitmap_create,
	.destroy = bitmap_destroy,
	.set_opaque = bitmap_set_opaque,
	.get_opaque = bitmap_get_opaque,
	.get_buffer = bitmap_get_buffer,
	.get_rowstride = bitmap_get_rowstride,
	.get_width = bitmap_get_width,
	.get_height = bitmap_get_height,
	.modified = bitmap_modified,
	.render = bitmap_render,
};

struct gui_bitmap_table *tsunami_bitmap_table = &bitmap_table;
