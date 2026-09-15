/*
 * Copyright 2015 Chris Young <chris@unsatisfactorysoftware.co.uk>
 *
 * This file is part of NetSurf, http://www.netsurf-browser.org/
 *
 * Abstract RTG helpers for cybergraphics.library / graphics.library.
 */

#ifndef AMIGA_RTG_H
#define AMIGA_RTG_H 1

#include <proto/graphics.h>
#include <cybergraphx/cybergraphics.h>

/* AllocBitMap PIXFMT for 32-bit ARGB friend/special bitmaps */
#define AMI_RTG_PIXFMT_ARGB32	PIXFMT_ARGB32
/* WritePixelArray / ReadPixelArray source/dest format */
#define AMI_RTG_RECTFMT_ARGB	RECTFMT_ARGB

struct BitMap *ami_rtg_allocbitmap(ULONG width, ULONG height, ULONG depth,
	ULONG flags, struct BitMap *friendbm, ULONG format);
void ami_rtg_freebitmap(struct BitMap *bm);

void ami_rtg_writepixelarray(UBYTE *pixdata, struct BitMap *bm,
	ULONG width, ULONG height, ULONG bpr, ULONG format);

void ami_rtg_readpixelarray(struct BitMap *bm, UBYTE **pixdata,
	ULONG width, ULONG height, ULONG bpr, ULONG format);

#endif
