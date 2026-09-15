/*
 * Copyright 2015-2025 Chris Young <chris@unsatisfactorysoftware.co.uk>
 *
 * This file is part of NetSurf, http://www.netsurf-browser.org/
 *
 * Abstract RTG helpers using cybergraphics.library (chunky Write/ReadPixelArray)
 * and graphics.library AllocBitMap/FreeBitMap.
 */

#include "amiga/rtg.h"

#include <proto/cybergraphics.h>
#include <cybergraphx/cybergraphics.h>

extern struct Library *CyberGfxBase;

struct BitMap *ami_rtg_allocbitmap(ULONG width, ULONG height, ULONG depth,
	ULONG flags, struct BitMap *friendbm, ULONG format)
{
	ULONG bmflags;

	bmflags = flags | BMF_MINPLANES;
	/*
	 * BMF_SPECIALFMT + PIXFMT needs cybergraphics.library. Without it,
	 * AllocBitMap can hang or return unusable bitmaps on classic OS3.
	 */
	if (format != 0 && CyberGfxBase != NULL) {
		bmflags |= BMF_SPECIALFMT | SHIFT_PIXFMT(format);
	}

	/*
	 * Palette / AGA: guigfx DrawPicture needs a BMF_STANDARD planar
	 * BitMap. BMF_MINPLANES alone yields a non-standard map; guigfx then
	 * picks WritePixelArray and crashes when CyberGFX is absent.
	 */
	if (depth <= 8UL && CyberGfxBase == NULL) {
		bmflags = (flags | BMF_STANDARD | BMF_CLEAR) & ~BMF_MINPLANES;
		/* Friend of an RTG map can still force non-standard; prefer none. */
		if (friendbm != NULL &&
		    (GetBitMapAttr(friendbm, BMA_FLAGS) & BMF_STANDARD) == 0) {
			friendbm = NULL;
		}
	}

	return AllocBitMap(width, height, depth, bmflags, friendbm);
}

void ami_rtg_freebitmap(struct BitMap *bm)
{
	if (bm != NULL) {
		FreeBitMap(bm);
	}
}

void ami_rtg_writepixelarray(UBYTE *pixdata, struct BitMap *bm,
	ULONG width, ULONG height, ULONG bpr, ULONG format)
{
	struct RastPort trp;

	if (pixdata == NULL || bm == NULL || CyberGfxBase == NULL) {
		return;
	}

	InitRastPort(&trp);
	trp.BitMap = bm;

	/* format is RECTFMT_* for CyberGFX PixelArray calls */
	WritePixelArray(pixdata, 0, 0, bpr, &trp, 0, 0, width, height, format);
}

void ami_rtg_readpixelarray(struct BitMap *bm, UBYTE **pixdata,
	ULONG width, ULONG height, ULONG bpr, ULONG format)
{
	struct RastPort trp;

	if (pixdata == NULL || *pixdata == NULL || bm == NULL ||
	    CyberGfxBase == NULL) {
		return;
	}

	InitRastPort(&trp);
	trp.BitMap = bm;

	ReadPixelArray(*pixdata, 0, 0, bpr, &trp, 0, 0, width, height, format);
}
