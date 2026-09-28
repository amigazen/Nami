/*
 * Copyright 2026 amigazen project
 *
 * This file is part of NetSurf / Tsunami (MUI frontend).
 *
 * Plot into an Intuition RastPort (offscreen friend BitMap during MUIM_Draw).
 * NetSurf colours are 0xAABBGGRR (see red_from_colour) — not 0xRRGGBB.
 * Text uses the shared Amiga font engine; bitmaps use cybergraphics WPA.
 */

#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/cybergraphics.h>
#include <cybergraphx/cybergraphics.h>

#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

#include "utils/utils.h"
#include "utils/errors.h"
#include "utils/log.h"
#include "utils/nsoption.h"
#include "netsurf/plotters.h"
#include "netsurf/plot_style.h"
#include "netsurf/bitmap.h"

#include "amiga/font.h"
#include "amiga/bitmap.h"
#include "amiga/rtg.h"
#include "mui/plotters.h"
#include "mui/libs.h"

static struct RastPort *plot_rp;
static struct ColorMap *plot_cm;
static int plot_ox;
static int plot_oy;

/**
 * Convert NetSurf 0xAABBGGRR colour to an Amiga pen.
 */
static ULONG colour_to_pen(colour c)
{
	ULONG r, g, b;
	LONG pen;

	r = (ULONG)red_from_colour(c);
	g = (ULONG)green_from_colour(c);
	b = (ULONG)blue_from_colour(c);

	if (plot_cm != NULL) {
		pen = ObtainBestPen(plot_cm,
				    r << 24, g << 24, b << 24,
				    OBP_Precision, PRECISION_GUI,
				    OBP_FailIfBad, FALSE,
				    TAG_DONE);
		if (pen != -1) {
			return (ULONG)pen;
		}
	}

	if ((r + g + b) > (3 * 128)) {
		return 2;
	}
	return 1;
}

static void release_pen(ULONG pen)
{
	if (plot_cm != NULL && pen != 1 && pen != 2) {
		ReleasePen(plot_cm, pen);
	}
}

void tsunami_plot_set_target(struct RastPort *rp, int origin_x, int origin_y)
{
	plot_rp = rp;
	plot_ox = origin_x;
	plot_oy = origin_y;
}

void tsunami_plot_set_colormap(struct ColorMap *cm)
{
	plot_cm = cm;
}

static nserror tsunami_plot_clip(const struct redraw_context *ctx,
				 const struct rect *clip)
{
	(void)ctx;
	(void)clip;
	return NSERROR_OK;
}

static nserror tsunami_plot_rectangle(const struct redraw_context *ctx,
				      const plot_style_t *style,
				      const struct rect *rect)
{
	ULONG pen;
	int x0, y0, x1, y1;

	(void)ctx;
	if (plot_rp == NULL) {
		return NSERROR_OK;
	}

	x0 = plot_ox + rect->x0;
	y0 = plot_oy + rect->y0;
	x1 = plot_ox + rect->x1 - 1;
	y1 = plot_oy + rect->y1 - 1;

	if (x1 < x0 || y1 < y0) {
		return NSERROR_OK;
	}

	if (style->fill_type != PLOT_OP_TYPE_NONE) {
		/* Prefer FillPixelArray on RTG — avoids pen thrash / wrong pens. */
		if (CyberGfxBase != NULL &&
		    GetBitMapAttr(plot_rp->BitMap, BMA_DEPTH) > 8) {
			FillPixelArray(plot_rp, x0, y0,
				       (ULONG)(x1 - x0 + 1),
				       (ULONG)(y1 - y0 + 1),
				       colour_rb_swap(style->fill_colour) &
				       0x00ffffffUL);
		} else {
			pen = colour_to_pen(style->fill_colour);
			SetAPen(plot_rp, pen);
			RectFill(plot_rp, x0, y0, x1, y1);
			release_pen(pen);
		}
	}

	if (style->stroke_type != PLOT_OP_TYPE_NONE) {
		pen = colour_to_pen(style->stroke_colour);
		SetAPen(plot_rp, pen);
		Move(plot_rp, x0, y0);
		Draw(plot_rp, x1, y0);
		Draw(plot_rp, x1, y1);
		Draw(plot_rp, x0, y1);
		Draw(plot_rp, x0, y0);
		release_pen(pen);
	}

	return NSERROR_OK;
}

static nserror tsunami_plot_line(const struct redraw_context *ctx,
				 const plot_style_t *style,
				 const struct rect *line)
{
	ULONG pen;

	(void)ctx;
	if (plot_rp == NULL || style->stroke_type == PLOT_OP_TYPE_NONE) {
		return NSERROR_OK;
	}

	pen = colour_to_pen(style->stroke_colour);
	SetAPen(plot_rp, pen);
	Move(plot_rp, plot_ox + line->x0, plot_oy + line->y0);
	Draw(plot_rp, plot_ox + line->x1, plot_oy + line->y1);
	release_pen(pen);
	return NSERROR_OK;
}

static nserror tsunami_plot_disc(const struct redraw_context *ctx,
				 const plot_style_t *style,
				 int x, int y, int radius)
{
	ULONG pen;

	(void)ctx;
	if (plot_rp == NULL) {
		return NSERROR_OK;
	}

	if (style->fill_type != PLOT_OP_TYPE_NONE) {
		pen = colour_to_pen(style->fill_colour);
		SetAPen(plot_rp, pen);
		DrawEllipse(plot_rp, plot_ox + x, plot_oy + y,
			    radius, radius);
		release_pen(pen);
	}
	return NSERROR_OK;
}

static nserror tsunami_plot_arc(const struct redraw_context *ctx,
				const plot_style_t *style,
				int x, int y, int radius,
				int angle1, int angle2)
{
	(void)ctx;
	(void)style;
	(void)x;
	(void)y;
	(void)radius;
	(void)angle1;
	(void)angle2;
	return NSERROR_OK;
}

static nserror tsunami_plot_polygon(const struct redraw_context *ctx,
				    const plot_style_t *style,
				    const int *p, unsigned int n)
{
	(void)ctx;
	(void)style;
	(void)p;
	(void)n;
	return NSERROR_OK;
}

static nserror tsunami_plot_path(const struct redraw_context *ctx,
				 const plot_style_t *pstyle,
				 const float *p, unsigned int n,
				 const float transform[6])
{
	(void)ctx;
	(void)pstyle;
	(void)p;
	(void)n;
	(void)transform;
	return NSERROR_OK;
}

/**
 * Blit soft ARGB bitmap via cybergraphics when possible.
 */
static nserror tsunami_plot_bitmap(const struct redraw_context *ctx,
				   struct bitmap *bitmap,
				   int x, int y, int width, int height,
				   colour bg, bitmap_flags_t flags)
{
	unsigned char *buf;
	int bw, bh;
	size_t stride;

	(void)ctx;
	(void)bg;
	(void)flags;

	if (plot_rp == NULL || bitmap == NULL) {
		return NSERROR_OK;
	}

	buf = amiga_bitmap_get_buffer(bitmap);
	bw = bitmap_get_width(bitmap);
	bh = bitmap_get_height(bitmap);
	stride = amiga_bitmap_get_rowstride(bitmap);

	if (buf == NULL || bw <= 0 || bh <= 0) {
		return NSERROR_OK;
	}

	if (width <= 0) {
		width = bw;
	}
	if (height <= 0) {
		height = bh;
	}

	if (CyberGfxBase == NULL) {
		return NSERROR_OK;
	}

	/* Soft buffers are ARGB8888 (bitmap_set_format in main). */
	WritePixelArray(buf, 0, 0, (ULONG)stride,
			plot_rp, plot_ox + x, plot_oy + y,
			(width == bw && height == bh) ? width : bw,
			(width == bw && height == bh) ? height : bh,
			RECTFMT_ARGB);

	return NSERROR_OK;
}

/**
 * Text — delegate to Amiga font engine (diskfont / ttengine / bullet).
 */
static nserror tsunami_plot_text(const struct redraw_context *ctx,
				 const struct plot_font_style *fstyle,
				 int x, int y,
				 const char *text, size_t length)
{
	ULONG pen;
	bool aa;

	(void)ctx;
	if (plot_rp == NULL || text == NULL || length == 0) {
		return NSERROR_OK;
	}

	pen = colour_to_pen(fstyle->foreground);
	SetAPen(plot_rp, pen);
	SetDrMd(plot_rp, JAM1);

	if (ami_nsfont != NULL) {
		aa = nsoption_bool(font_antialiasing);
		ami_nsfont->text(plot_rp, text, (ULONG)length, fstyle,
				 (ULONG)(plot_ox + x), (ULONG)(plot_oy + y), aa);
	} else {
		Move(plot_rp, plot_ox + x, plot_oy + y);
		Text(plot_rp, (STRPTR)text, (ULONG)length);
	}

	release_pen(pen);
	return NSERROR_OK;
}

static const struct plotter_table plotters = {
	.clip = tsunami_plot_clip,
	.arc = tsunami_plot_arc,
	.disc = tsunami_plot_disc,
	.line = tsunami_plot_line,
	.rectangle = tsunami_plot_rectangle,
	.polygon = tsunami_plot_polygon,
	.path = tsunami_plot_path,
	.bitmap = tsunami_plot_bitmap,
	.text = tsunami_plot_text,
	.option_knockout = false,
};

const struct plotter_table *tsunami_plotters = &plotters;
