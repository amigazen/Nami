/*
 * Copyright 2026 amigazen project
 *
 * This file is part of NetSurf / Tsunami (MUI frontend).
 *
 * Custom MUI Area hosting NetSurf content. Page paint uses the Amiga FE
 * plotters (amiplot + ami_plot_ra_*) and the same tiled BltBitMapRastPort
 * path as frontends/amiga/gui.c — not a hand-rolled plotter.
 */

#include "mui/os3support.h"

#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/utility.h>
#include <proto/muimaster.h>
#include <libraries/mui.h>

#include <stdlib.h>
#include <string.h>

#include "utils/log.h"
#include "netsurf/browser_window.h"
#include "netsurf/mouse.h"
#include "netsurf/window.h"
#include "netsurf/plotters.h"

#include "amiga/font.h"
#include "amiga/plotters.h"
#include "amiga/object.h"
#include "mui/gui.h"
#include "mui/browser.h"
#include "mui/misc.h"
#include "mui/schedule.h"

#ifndef MUI_EHF_GUIMODE
#define MUI_EHF_GUIMODE (1 << 1)
#endif

/* Match Amiga FE: yield between horizontal paint strips. */
#define TSUNAMI_PAINT_YIELD_STRIP 64

struct MUI_CustomClass *TsunamiBrowserClass;

/* Shared Amiga plot render area (same role as gui.c browserglob). */
static struct gui_globals *browserglob;

/*
 * Palette-screen pens for amiplot. Without this list, ami_plot_obtain_pen()
 * immediately ReleasePen()s and forces pen 1 (black on WB) — blank black page.
 * Same contract as Amiga FE gwin->shared_pens + ami_plot_ra_set_pen_list().
 */
static struct MinList *browser_pens;

struct BrowserData {
	struct MUI_EventHandlerNode ehnode;
	struct gui_window *gw;
	int mouse_x;
	int mouse_y;
	int shown;
	int redraw_scheduled;
};

/**
 * Ensure ami_plot_ra + shared pen list exist now that a Screen is known.
 */
static void ensure_browserglob(void)
{
	if (browserglob != NULL) {
		return;
	}
	if (ami_gui_get_screen() == NULL) {
		NSLOG(netsurf, WARNING,
		      "TsunamiBrowser: no screen for ami_plot_ra_alloc");
		return;
	}

	/*
	 * alloc_pen_list=false — we attach our own list (Amiga FE does the
	 * same via ami_plot_ra_set_pen_list on the window's shared_pens).
	 */
	browserglob = ami_plot_ra_alloc(0, 0, false, false);
	if (browserglob == NULL) {
		NSLOG(netsurf, WARNING, "TsunamiBrowser: ami_plot_ra_alloc failed");
		return;
	}

	browser_pens = ami_AllocMinList();
	if (browser_pens != NULL) {
		ami_plot_ra_set_pen_list(browserglob, browser_pens);
		NSLOG(netsurf, INFO,
		      "TsunamiBrowser: ami_plot_ra_alloc -> %p pens=%p",
		      (void *)browserglob, (void *)browser_pens);
	} else {
		NSLOG(netsurf, WARNING,
		      "TsunamiBrowser: no pen list — palette plot will be black");
	}
}

/**
 * Free shared plot RA (call from gui fini).
 */
void tsunami_browser_plot_fini(void)
{
	if (browser_pens != NULL) {
		ami_plot_release_pens(browser_pens);
		free(browser_pens);
		browser_pens = NULL;
	}
	if (browserglob != NULL) {
		ami_plot_ra_free(browserglob);
		browserglob = NULL;
	}
}

/**
 * Push current Area size into gui_window and reformat when it changes.
 */
static void sync_size_reformat(Object *obj, struct BrowserData *data)
{
	int w, h;
	int old_w, old_h;

	if (data->gw == NULL) {
		return;
	}

	w = _mwidth(obj);
	h = _mheight(obj);
	if (w < 1 || h < 1) {
		return;
	}

	old_w = data->gw->width;
	old_h = data->gw->height;
	data->gw->width = w;
	data->gw->height = h;

	if (data->gw->bw != NULL && (w != old_w || h != old_h)) {
		browser_window_schedule_reformat(data->gw->bw);
		tsunami_gui_sync_scrollbars(data->gw);
	}
}

/**
 * Paint browser content with Amiga amiplot into the Area (tiled blit).
 *
 * Copied from ami_do_redraw_tiled() in frontends/amiga/gui.c — same
 * browser_window_redraw / BltBitMapRastPort contract.
 */
static void paint_amiplot(Object *obj, struct BrowserData *data)
{
	struct gui_window *gw;
	struct redraw_context ctx;
	struct rect clip;
	struct RastPort *winrp;
	struct BitMap *srcbm;
	int w, h;
	int sx, sy;
	int left, top, width, height;
	int tile_size_x, tile_size_y;
	int step_y;
	int x, y;
	int dest_x, dest_y;

	gw = data->gw;
	if (gw == NULL || gw->bw == NULL || browserglob == NULL) {
		return;
	}

	w = _mwidth(obj);
	h = _mheight(obj);
	winrp = _rp(obj);
	if (w < 1 || h < 1 || winrp == NULL) {
		return;
	}

	sx = gw->scrollx;
	sy = gw->scrolly;
	left = sx;
	top = sy;
	width = w;
	height = h;

	memset(&ctx, 0, sizeof(ctx));
	ctx.interactive = true;
	ctx.background_images = true;
	ctx.plot = &amiplot;
	ctx.priv = browserglob;

	ami_plot_ra_get_size(browserglob, &tile_size_x, &tile_size_y);
	if (tile_size_x < 1) {
		tile_size_x = w;
	}
	if (tile_size_y < 1) {
		tile_size_y = h;
	}

	step_y = tile_size_y;
	if (step_y <= 0 || step_y > TSUNAMI_PAINT_YIELD_STRIP) {
		step_y = TSUNAMI_PAINT_YIELD_STRIP;
	}

	srcbm = ami_plot_ra_get_bitmap(browserglob);
	if (srcbm == NULL) {
		NSLOG(netsurf, WARNING, "TsunamiBrowser: plot BitMap is NULL");
		return;
	}

	dest_x = _mleft(obj);
	dest_y = _mtop(obj);

	/*
	 * FillArea is FALSE — wipe with WB pen 2 (usually white) without
	 * ObtainBestPen so we do not burn ColourMap slots before amiplot.
	 */
	SetAPen(winrp, 2);
	RectFill(winrp, dest_x, dest_y, dest_x + w - 1, dest_y + h - 1);

	if (browser_window_redraw_ready(gw->bw) == false) {
		return;
	}

	/* Same as ami_do_redraw_tiled: bind pen list for this paint. */
	if (browser_pens != NULL) {
		ami_plot_ra_set_pen_list(browserglob, browser_pens);
	}

	NSLOG(netsurf, INFO,
	      "TsunamiBrowser paint %dx%d scroll=%d,%d tile=%dx%d",
	      w, h, sx, sy, tile_size_x, tile_size_y);

	tsunami_gui_throbber_heavy_begin();

	for (y = top; y < (top + height); y += step_y) {
		clip.y0 = 0;
		clip.y1 = step_y;
		if (clip.y1 > tile_size_y) {
			clip.y1 = tile_size_y;
		}
		if (clip.y1 > ((top + height) - y)) {
			clip.y1 = (top + height) - y;
		}
		if (((y - sy) + clip.y1) > h) {
			clip.y1 = h - (y - sy);
		}
		if (clip.y1 <= 0) {
			break;
		}

		for (x = left; x < (left + width); x += tile_size_x) {
			clip.x0 = 0;
			clip.x1 = tile_size_x;
			if (clip.x1 > ((left + width) - x)) {
				clip.x1 = (left + width) - x;
			}
			if (((x - sx) + clip.x1) > w) {
				clip.x1 = w - (x - sx);
			}
			if (clip.x1 <= 0) {
				break;
			}

			if (browser_window_redraw(gw->bw,
						  clip.x0 - x,
						  clip.y0 - y,
						  &clip, &ctx)) {
				ami_clearclipreg(browserglob);
				BltBitMapRastPort(srcbm, 0, 0, winrp,
						  dest_x + (x - sx),
						  dest_y + (y - sy),
						  clip.x1, clip.y1, 0xC0);
			}
		}
		/* Event loop is blocked in MUIM_Draw — keep KITT alive. */
		tsunami_gui_throbber_pulse();
	}

	tsunami_gui_throbber_heavy_end();

	/*
	 * Amiga FE releases pens on new content only. Releasing after every
	 * paint remaps ColourMap slots and made text/colours vanish on AGA.
	 */
}

/**
 * Release palette pens (call on NEW_CONTENT / fini).
 */
void tsunami_browser_release_pens(void)
{
	if (browser_pens != NULL) {
		ami_plot_release_pens(browser_pens);
	}
}

/**
 * AskMinMax — browser content wants a large default viewport.
 */
static ULONG mAskMinMax(struct IClass *cl, Object *obj,
			struct MUIP_AskMinMax *msg)
{
	DoSuperMethodA(cl, obj, (Msg)msg);

	msg->MinMaxInfo->MinWidth += 100;
	msg->MinMaxInfo->DefWidth += 200;
	msg->MinMaxInfo->MaxWidth += MUI_MAXMAX;

	msg->MinMaxInfo->MinHeight += 80;
	msg->MinMaxInfo->DefHeight += 150;
	msg->MinMaxInfo->MaxHeight += MUI_MAXMAX;

	return 0;
}

static ULONG mDraw(struct IClass *cl, Object *obj, struct MUIP_Draw *msg)
{
	struct BrowserData *data;

	data = INST_DATA(cl, obj);
	DoSuperMethodA(cl, obj, (Msg)msg);

	if (!(msg->flags & (MADF_DRAWOBJECT | MADF_DRAWUPDATE))) {
		return 0;
	}

	sync_size_reformat(obj, data);
	ensure_browserglob();
	paint_amiplot(obj, data);
	return 0;
}

static ULONG mShow(struct IClass *cl, Object *obj, Msg msg)
{
	struct BrowserData *data;
	ULONG ok;

	ok = DoSuperMethodA(cl, obj, msg);
	if (!ok) {
		return FALSE;
	}

	data = INST_DATA(cl, obj);
	data->shown = 1;
	tsunami_set_screen(_screen(obj));
	ami_font_setdevicedpi(0);
	ensure_browserglob();
	sync_size_reformat(obj, data);
	NSLOG(netsurf, INFO, "TsunamiBrowser Show %dx%d glob=%p",
	      data->gw != NULL ? data->gw->width : 0,
	      data->gw != NULL ? data->gw->height : 0,
	      (void *)browserglob);
	NSLOG(netsurf, INFO, "TABTRACE: browser Show done");
	return TRUE;
}

static ULONG mHide(struct IClass *cl, Object *obj, Msg msg)
{
	struct BrowserData *data;

	data = INST_DATA(cl, obj);
	data->shown = 0;
	return DoSuperMethodA(cl, obj, msg);
}

static ULONG mSetup(struct IClass *cl, Object *obj, Msg msg)
{
	struct BrowserData *data;

	data = INST_DATA(cl, obj);
	if (!DoSuperMethodA(cl, obj, msg)) {
		return FALSE;
	}

	data->ehnode.ehn_Object = obj;
	data->ehnode.ehn_Class = cl;
	data->ehnode.ehn_Events = IDCMP_MOUSEBUTTONS | IDCMP_MOUSEMOVE |
		IDCMP_RAWKEY;
	data->ehnode.ehn_Flags = MUI_EHF_GUIMODE;
	data->ehnode.ehn_Priority = 0;

	DoMethod(_win(obj), MUIM_Window_AddEventHandler, &data->ehnode);
	tsunami_set_screen(_screen(obj));
	return TRUE;
}

static ULONG mCleanup(struct IClass *cl, Object *obj, Msg msg)
{
	struct BrowserData *data;

	data = INST_DATA(cl, obj);
	DoMethod(_win(obj), MUIM_Window_RemEventHandler, &data->ehnode);
	return DoSuperMethodA(cl, obj, msg);
}

static ULONG mHandleEvent(struct IClass *cl, Object *obj,
			  struct MUIP_HandleEvent *msg)
{
	struct BrowserData *data;
	struct IntuiMessage *imsg;
	int x, y;
	int left, top, width, height;
	browser_mouse_state mouse;

	data = INST_DATA(cl, obj);
	imsg = msg->imsg;

	if (imsg == NULL || data->gw == NULL || data->gw->bw == NULL ||
	    !data->shown) {
		return 0;
	}

	/*
	 * Handler is on the Window — only claim events inside this Area or
	 * toolbar / URL gadgets never receive clicks. Return 0 (not
	 * DoSuperMethod) when outside so chrome keeps the event.
	 */
	left = _mleft(obj);
	top = _mtop(obj);
	width = _mwidth(obj);
	height = _mheight(obj);

	if (width < 1 || height < 1 ||
	    imsg->MouseX < left || imsg->MouseX >= left + width ||
	    imsg->MouseY < top || imsg->MouseY >= top + height) {
		return 0;
	}

	x = imsg->MouseX - left + data->gw->scrollx;
	y = imsg->MouseY - top + data->gw->scrolly;

	if (imsg->Class == IDCMP_MOUSEBUTTONS) {
		if (imsg->Code == SELECTDOWN) {
			mouse = BROWSER_MOUSE_PRESS_1;
			browser_window_mouse_click(data->gw->bw, mouse, x, y);
			return MUI_EventHandlerRC_Eat;
		}
		if (imsg->Code == SELECTUP) {
			mouse = BROWSER_MOUSE_CLICK_1;
			browser_window_mouse_click(data->gw->bw, mouse, x, y);
			return MUI_EventHandlerRC_Eat;
		}
	} else if (imsg->Class == IDCMP_MOUSEMOVE) {
		data->mouse_x = x;
		data->mouse_y = y;
		browser_window_mouse_track(data->gw->bw, 0, x, y);
		return 0;
	}

	return 0;
}

static ULONG mSet(struct IClass *cl, Object *obj, struct opSet *msg)
{
	struct BrowserData *data;
	struct TagItem *tags;
	struct TagItem *tag;

	data = INST_DATA(cl, obj);
	tags = msg->ops_AttrList;

	while ((tag = NextTagItem(&tags)) != NULL) {
		if (tag->ti_Tag == MUIA_Tsunami_Browser_GuiWindow) {
			data->gw = (struct gui_window *)tag->ti_Data;
		}
	}

	return DoSuperMethodA(cl, obj, (Msg)msg);
}

static ULONG mNew(struct IClass *cl, Object *obj, struct opSet *msg)
{
	struct BrowserData *data;
	struct TagItem *tags;
	struct TagItem *tag;

	obj = (Object *)DoSuperMethodA(cl, obj, (Msg)msg);
	if (obj == NULL) {
		return 0;
	}

	data = INST_DATA(cl, obj);
	memset(data, 0, sizeof(*data));

	tags = msg->ops_AttrList;
	while ((tag = NextTagItem(&tags)) != NULL) {
		if (tag->ti_Tag == MUIA_Tsunami_Browser_GuiWindow) {
			data->gw = (struct gui_window *)tag->ti_Data;
		}
	}

	NSLOG(netsurf, INFO, "TsunamiBrowser OM_NEW gw=%p tab=%d",
	      (void *)data->gw,
	      data->gw != NULL ? data->gw->tab_index : -1);
	NSLOG(netsurf, INFO, "TABTRACE: browser OM_NEW done");
	return (ULONG)obj;
}

static ULONG mSyncScroll(struct IClass *cl, Object *obj, Msg msg)
{
	struct BrowserData *data;
	struct gui_window *gw;
	ULONG first;

	(void)msg;
	data = INST_DATA(cl, obj);
	gw = data->gw;
	if (gw == NULL) {
		return 0;
	}

	if (gw->vbar != NULL) {
		first = 0;
		get(gw->vbar, MUIA_Prop_First, &first);
		gw->scrolly = (int)first;
	}
	if (gw->hbar != NULL) {
		first = 0;
		get(gw->hbar, MUIA_Prop_First, &first);
		gw->scrollx = (int)first;
	}

	/*
	 * Never MUI_Redraw from Prop notify — during Group ExitChange /
	 * ActivePage that re-enters layout and deadlocks classic MUI.
	 */
	tsunami_browser_redraw(obj);
	return 0;
}

static ULONG ASM SAVEDS BrowserDispatcher(REG(a0, struct IClass *cl),
					  REG(a2, Object *obj),
					  REG(a1, Msg msg))
{
	switch (msg->MethodID) {
	case OM_NEW:
		return mNew(cl, obj, (struct opSet *)msg);
	case OM_SET:
		return mSet(cl, obj, (struct opSet *)msg);
	case MUIM_AskMinMax:
		return mAskMinMax(cl, obj, (struct MUIP_AskMinMax *)msg);
	case MUIM_Draw:
		return mDraw(cl, obj, (struct MUIP_Draw *)msg);
	case MUIM_Show:
		return mShow(cl, obj, msg);
	case MUIM_Hide:
		return mHide(cl, obj, msg);
	case MUIM_Setup:
		return mSetup(cl, obj, msg);
	case MUIM_Cleanup:
		return mCleanup(cl, obj, msg);
	case MUIM_HandleEvent:
		return mHandleEvent(cl, obj, (struct MUIP_HandleEvent *)msg);
	case MUIM_Tsunami_Browser_SyncScroll:
		return mSyncScroll(cl, obj, msg);
	}

	return DoSuperMethodA(cl, obj, msg);
}

BOOL tsunami_browser_class_init(void)
{
	TsunamiBrowserClass = MUI_CreateCustomClass(NULL, MUIC_Area, NULL,
						    sizeof(struct BrowserData),
						    (APTR)BrowserDispatcher);
	return TsunamiBrowserClass != NULL;
}

void tsunami_browser_class_fini(void)
{
	tsunami_browser_plot_fini();
	if (TsunamiBrowserClass != NULL) {
		MUI_DeleteCustomClass(TsunamiBrowserClass);
		TsunamiBrowserClass = NULL;
	}
}

/**
 * Deferred MUIM_Draw — avoid re-entrant paint inside NewInput / notifies.
 */
static void tsunami_browser_redraw_cb(void *p)
{
	Object *browser;
	struct BrowserData *data;

	browser = (Object *)p;
	if (browser == NULL || TsunamiBrowserClass == NULL ||
	    TsunamiBrowserClass->mcc_Class == NULL) {
		return;
	}

	data = INST_DATA(TsunamiBrowserClass->mcc_Class, browser);
	data->redraw_scheduled = 0;
	MUI_Redraw(browser, MADF_DRAWOBJECT);
}

void tsunami_browser_redraw(Object *browser)
{
	struct BrowserData *data;

	if (browser == NULL || TsunamiBrowserClass == NULL ||
	    TsunamiBrowserClass->mcc_Class == NULL) {
		return;
	}

	data = INST_DATA(TsunamiBrowserClass->mcc_Class, browser);
	if (data->redraw_scheduled) {
		return;
	}
	data->redraw_scheduled = 1;
	tsunami_schedule(0, tsunami_browser_redraw_cb, browser);
}

void tsunami_browser_detach(Object *browser)
{
	struct BrowserData *data;

	if (browser == NULL || TsunamiBrowserClass == NULL ||
	    TsunamiBrowserClass->mcc_Class == NULL) {
		return;
	}

	data = INST_DATA(TsunamiBrowserClass->mcc_Class, browser);
	if (data->redraw_scheduled) {
		tsunami_schedule(-1, tsunami_browser_redraw_cb, browser);
		data->redraw_scheduled = 0;
	}
	data->gw = NULL;
}
