/*
 * Copyright 2026 amigazen project
 *
 * This file is part of NetSurf / Tsunami (MUI frontend).
 *
 * Open and close the Amiga libraries Tsunami needs. MUI chrome uses
 * muimaster; graphics/intuition/diskfont/ttengine drive the browser Area;
 * cybergraphics is used for bitmap blit; locale supplies LocaleBase for iconv;
 * iconv.library is optional (Tsunami shim falls back to the built-in converter).
 */

#include "mui/os3support.h"

#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/utility.h>
#include <proto/locale.h>
#include <proto/muimaster.h>
#include <proto/bitmap.h>
#include <devices/timer.h>

#include "utils/log.h"
#include "mui/libs.h"
#include "mui/iconv_lib.h"

struct Library *MUIMasterBase;
struct IntuitionBase *IntuitionBase;
struct GfxBase *GfxBase;
struct Library *UtilityBase;
struct Library *LocaleBase;
struct Library *DiskfontBase;
struct Library *TTEngineBase;
struct Library *CyberGfxBase;
struct Library *DataTypesBase;
struct Library *CodesetsBase;
struct Library *IconBase;
struct Library *LayersBase;
struct Library *BitMapBase;
struct Library *AslBase;
struct Library *IFFParseBase;
Class *BitMapClass;

/* bullet.library — Amiga FE Compugraphic / OutlineFont path (optional). */
struct Library *BulletBase;

/*
 * timer.device — required by proto/timer.h GetSysTime() in font_cache
 * (same TimerBase the Amiga FE schedule opens).
 */
struct Device *TimerBase;
static struct MsgPort *tsunami_timer_port;
static struct timerequest *tsunami_timer_req;

/* DOSBase is provided by the C runtime on Amiga; declare for clarity. */
extern struct DosLibrary *DOSBase;

static BOOL tsunami_timer_open(void)
{
	tsunami_timer_port = CreateMsgPort();
	if (tsunami_timer_port == NULL) {
		return FALSE;
	}
	tsunami_timer_req = (struct timerequest *)CreateIORequest(
		tsunami_timer_port, sizeof(struct timerequest));
	if (tsunami_timer_req == NULL) {
		DeleteMsgPort(tsunami_timer_port);
		tsunami_timer_port = NULL;
		return FALSE;
	}
	if (OpenDevice("timer.device", UNIT_VBLANK,
		       (struct IORequest *)tsunami_timer_req, 0) != 0) {
		DeleteIORequest((struct IORequest *)tsunami_timer_req);
		DeleteMsgPort(tsunami_timer_port);
		tsunami_timer_req = NULL;
		tsunami_timer_port = NULL;
		return FALSE;
	}
	TimerBase = tsunami_timer_req->tr_node.io_Device;
	return TRUE;
}

static void tsunami_timer_close(void)
{
	if (tsunami_timer_req != NULL) {
		CloseDevice((struct IORequest *)tsunami_timer_req);
		DeleteIORequest((struct IORequest *)tsunami_timer_req);
		tsunami_timer_req = NULL;
	}
	if (tsunami_timer_port != NULL) {
		DeleteMsgPort(tsunami_timer_port);
		tsunami_timer_port = NULL;
	}
	TimerBase = NULL;
}

/**
 * Open libraries required by Tsunami.
 */
BOOL tsunami_libs_open(void)
{
	IntuitionBase = (struct IntuitionBase *)OpenLibrary("intuition.library", 39);
	if (IntuitionBase == NULL) {
		return FALSE;
	}

	GfxBase = (struct GfxBase *)OpenLibrary("graphics.library", 39);
	if (GfxBase == NULL) {
		CloseLibrary((struct Library *)IntuitionBase);
		IntuitionBase = NULL;
		return FALSE;
	}

	UtilityBase = OpenLibrary("utility.library", 39);
	if (UtilityBase == NULL) {
		CloseLibrary((struct Library *)GfxBase);
		CloseLibrary((struct Library *)IntuitionBase);
		GfxBase = NULL;
		IntuitionBase = NULL;
		return FALSE;
	}

	if (!tsunami_timer_open()) {
		NSLOG(netsurf, WARNING, "timer.device required for font cache");
		CloseLibrary(UtilityBase);
		CloseLibrary((struct Library *)GfxBase);
		CloseLibrary((struct Library *)IntuitionBase);
		UtilityBase = NULL;
		GfxBase = NULL;
		IntuitionBase = NULL;
		return FALSE;
	}

	DiskfontBase = OpenLibrary("diskfont.library", 40);
	if (DiskfontBase == NULL) {
		NSLOG(netsurf, WARNING, "diskfont.library v40 required");
		tsunami_timer_close();
		CloseLibrary(UtilityBase);
		CloseLibrary((struct Library *)GfxBase);
		CloseLibrary((struct Library *)IntuitionBase);
		UtilityBase = NULL;
		GfxBase = NULL;
		IntuitionBase = NULL;
		return FALSE;
	}

	/* icon.library v44 for GetIconTags (amiga filetype MIMETYPE tooltypes). */
	IconBase = OpenLibrary("icon.library", 44);
	if (IconBase == NULL) {
		NSLOG(netsurf, WARNING, "icon.library v44 required");
		CloseLibrary(DiskfontBase);
		tsunami_timer_close();
		CloseLibrary(UtilityBase);
		CloseLibrary((struct Library *)GfxBase);
		CloseLibrary((struct Library *)IntuitionBase);
		DiskfontBase = NULL;
		UtilityBase = NULL;
		GfxBase = NULL;
		IntuitionBase = NULL;
		return FALSE;
	}

	/* layers.library required by ami_plot_ra_alloc (CreateUpfrontLayer). */
	LayersBase = OpenLibrary("layers.library", 39);
	if (LayersBase == NULL) {
		NSLOG(netsurf, WARNING, "layers.library v39 required");
		CloseLibrary(IconBase);
		CloseLibrary(DiskfontBase);
		tsunami_timer_close();
		CloseLibrary(UtilityBase);
		CloseLibrary((struct Library *)GfxBase);
		CloseLibrary((struct Library *)IntuitionBase);
		IconBase = NULL;
		DiskfontBase = NULL;
		UtilityBase = NULL;
		GfxBase = NULL;
		IntuitionBase = NULL;
		return FALSE;
	}

	/* Optional — AmiSSL path uses amihttp; cybergraphics for RTG blit. */
	CyberGfxBase = OpenLibrary("cybergraphics.library", 41);
	if (CyberGfxBase != NULL) {
		NSLOG(netsurf, INFO, "cybergraphics.library v%d.%d",
		      CyberGfxBase->lib_Version, CyberGfxBase->lib_Revision);
	} else {
		NSLOG(netsurf, WARNING,
		      "cybergraphics.library missing — page images will not blit");
	}
	DataTypesBase = OpenLibrary("datatypes.library", 39);
	CodesetsBase = OpenLibrary("codesets.library", 6);
	TTEngineBase = OpenLibrary("ttengine.library", 6);
	if (TTEngineBase != NULL) {
		NSLOG(netsurf, INFO, "ttengine.library v%d.%d",
		      TTEngineBase->lib_Version, TTEngineBase->lib_Revision);
	}

	/* Same as Amiga FE libs.c — OutlineFont / GlyphEngine for bullet path. */
	BulletBase = OpenLibrary("bullet.library", 0);
	if (BulletBase != NULL) {
		NSLOG(netsurf, INFO, "bullet.library v%d.%d",
		      BulletBase->lib_Version, BulletBase->lib_Revision);
	} else {
		NSLOG(netsurf, INFO,
		      "bullet.library missing — diskfont/ttengine only");
	}

	LocaleBase = OpenLibrary("locale.library", 38);

	/* Optional full converter; mui/iconv.c falls back if this is NULL. */
	tsunami_iconv_lib_open();

	/* ASL requesters (Open/Save) and IFF clipboard. */
	AslBase = OpenLibrary("asl.library", 37);
	if (AslBase == NULL) {
		NSLOG(netsurf, WARNING, "asl.library missing — Open/Save disabled");
	}
	IFFParseBase = OpenLibrary("iffparse.library", 37);
	if (IFFParseBase == NULL) {
		NSLOG(netsurf, WARNING,
		      "iffparse.library missing — clipboard disabled");
	}

	/*
	 * images/bitmap.image — Amiga FE loads AISS toolbar icons via BitMapObj.
	 * Optional: text toolbar buttons are used if this class is missing.
	 */
	BitMapBase = OpenLibrary("images/bitmap.image", 41);
	BitMapClass = NULL;
	if (BitMapBase != NULL) {
		BitMapClass = BITMAP_GetClass();
		if (BitMapClass != NULL) {
			NSLOG(netsurf, INFO, "images/bitmap.image v%d.%d",
			      BitMapBase->lib_Version, BitMapBase->lib_Revision);
		} else {
			NSLOG(netsurf, WARNING,
			      "BITMAP_GetClass failed — text toolbar icons");
			CloseLibrary(BitMapBase);
			BitMapBase = NULL;
		}
	} else {
		NSLOG(netsurf, WARNING,
		      "images/bitmap.image missing — text toolbar icons");
	}

	/* MUI 3.8+ (muimaster V19). */
	MUIMasterBase = OpenLibrary("muimaster.library", 19);
	if (MUIMasterBase == NULL) {
		if (BitMapBase != NULL) {
			CloseLibrary(BitMapBase);
			BitMapBase = NULL;
			BitMapClass = NULL;
		}
		if (LocaleBase != NULL) {
			CloseLibrary(LocaleBase);
			LocaleBase = NULL;
		}
		tsunami_iconv_lib_close();
		if (TTEngineBase != NULL) {
			CloseLibrary(TTEngineBase);
			TTEngineBase = NULL;
		}
		if (BulletBase != NULL) {
			CloseLibrary(BulletBase);
			BulletBase = NULL;
		}
		if (CodesetsBase != NULL) {
			CloseLibrary(CodesetsBase);
			CodesetsBase = NULL;
		}
		if (DataTypesBase != NULL) {
			CloseLibrary(DataTypesBase);
			DataTypesBase = NULL;
		}
		if (CyberGfxBase != NULL) {
			CloseLibrary(CyberGfxBase);
			CyberGfxBase = NULL;
		}
		CloseLibrary(LayersBase);
		CloseLibrary(IconBase);
		CloseLibrary(DiskfontBase);
		tsunami_timer_close();
		CloseLibrary(UtilityBase);
		CloseLibrary((struct Library *)GfxBase);
		CloseLibrary((struct Library *)IntuitionBase);
		LayersBase = NULL;
		IconBase = NULL;
		DiskfontBase = NULL;
		UtilityBase = NULL;
		GfxBase = NULL;
		IntuitionBase = NULL;
		return FALSE;
	}

	return TRUE;
}

/**
 * Close libraries opened by tsunami_libs_open.
 */
void tsunami_libs_close(void)
{
	if (MUIMasterBase != NULL) {
		CloseLibrary(MUIMasterBase);
		MUIMasterBase = NULL;
	}
	if (BitMapBase != NULL) {
		CloseLibrary(BitMapBase);
		BitMapBase = NULL;
		BitMapClass = NULL;
	}
	if (IFFParseBase != NULL) {
		CloseLibrary(IFFParseBase);
		IFFParseBase = NULL;
	}
	if (AslBase != NULL) {
		CloseLibrary(AslBase);
		AslBase = NULL;
	}
	if (LocaleBase != NULL) {
		CloseLibrary(LocaleBase);
		LocaleBase = NULL;
	}
	tsunami_iconv_lib_close();
	if (TTEngineBase != NULL) {
		CloseLibrary(TTEngineBase);
		TTEngineBase = NULL;
	}
	if (BulletBase != NULL) {
		CloseLibrary(BulletBase);
		BulletBase = NULL;
	}
	if (CodesetsBase != NULL) {
		CloseLibrary(CodesetsBase);
		CodesetsBase = NULL;
	}
	if (DataTypesBase != NULL) {
		CloseLibrary(DataTypesBase);
		DataTypesBase = NULL;
	}
	if (CyberGfxBase != NULL) {
		CloseLibrary(CyberGfxBase);
		CyberGfxBase = NULL;
	}
	if (LayersBase != NULL) {
		CloseLibrary(LayersBase);
		LayersBase = NULL;
	}
	if (IconBase != NULL) {
		CloseLibrary(IconBase);
		IconBase = NULL;
	}
	if (DiskfontBase != NULL) {
		CloseLibrary(DiskfontBase);
		DiskfontBase = NULL;
	}
	tsunami_timer_close();
	if (UtilityBase != NULL) {
		CloseLibrary(UtilityBase);
		UtilityBase = NULL;
	}
	if (GfxBase != NULL) {
		CloseLibrary((struct Library *)GfxBase);
		GfxBase = NULL;
	}
	if (IntuitionBase != NULL) {
		CloseLibrary((struct Library *)IntuitionBase);
		IntuitionBase = NULL;
	}
}
