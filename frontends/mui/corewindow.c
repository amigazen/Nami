/*
 * Copyright 2017 Chris Young <chris@unsatisfactorysoftware.co.uk>
 * Copyright 2026 amigazen project
 *
 * Forked from frontends/amiga/corewindow.c for Tsunami (MUI).
 * Reaction Window/Space/Scroller replaced with MUI Window + Area + Prop.
 */

#include "mui/os3support.h"

#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/utility.h>
#include <proto/muimaster.h>
#include <libraries/mui.h>
#include <utility/hooks.h>

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "utils/log.h"
#include "utils/errors.h"
#include "netsurf/mouse.h"
#include "netsurf/plotters.h"
#include "netsurf/keypress.h"

#include "amiga/plotters.h"
#include "amiga/object.h"
#include "amiga/utf8.h"
#include "mui/gui.h"
#include "mui/misc.h"
#include "mui/schedule.h"
#include "mui/corewindow.h"

#ifndef MUI_EHF_GUIMODE
#define MUI_EHF_GUIMODE (1 << 1)
#endif

#define MUIA_Tsunami_CW_Core (TAG_USER | 0x541001)
#define MUIM_Tsunami_CW_SyncScroll (TAG_USER | 0x541010)

struct MUI_CustomClass *TsunamiCoreAreaClass;

struct CoreAreaData {
	struct MUI_EventHandlerNode ehnode;
	struct ami_corewindow *cw;
	int shown;
};

/* ---------- paint ---------- */

static void cw_paint(Object *obj, struct ami_corewindow *cw)
{
	struct RastPort *winrp;
	struct BitMap *srcbm;
	struct rect clip;
	struct redraw_context ctx;
	int w, h, sx, sy, x, y, tile_x, tile_y;
	int left, top, width, height;

	if (cw == NULL || cw->draw == NULL || cw->gg == NULL) {
		return;
	}

	w = _mwidth(obj);
	h = _mheight(obj);
	if (w < 1 || h < 1) {
		return;
	}

	cw->view_w = w;
	cw->view_h = h;
	sx = cw->scrollx;
	sy = cw->scrolly;

	winrp = _rp(obj);
	if (winrp == NULL) {
		return;
	}

	ami_plot_ra_get_size(cw->gg, &tile_x, &tile_y);
	if (tile_x < 1) {
		tile_x = 64;
	}
	if (tile_y < 1) {
		tile_y = 64;
	}

	srcbm = ami_plot_ra_get_bitmap(cw->gg);
	if (srcbm == NULL) {
		return;
	}

	SetAPen(winrp, 2);
	RectFill(winrp, _mleft(obj), _mtop(obj),
		 _mleft(obj) + w - 1, _mtop(obj) + h - 1);

	ctx.interactive = true;
	ctx.background_images = true;
	ctx.plot = &amiplot;
	ctx.priv = cw->gg;

	if (cw->shared_pens != NULL) {
		ami_plot_ra_set_pen_list(cw->gg, cw->shared_pens);
	}

	/*
	 * Match Amiga ami_cw_redraw_rect: clip is in document coordinates,
	 * origin is (-tile_x, -tile_y). Passing (0,0)-(tw,th) for every tile
	 * made treeviews (hotlist/history/cookies) redraw the same top-left
	 * content into each tile.
	 */
	left = sx;
	top = sy;
	width = w;
	height = h;

	for (y = top; y < (top + height); y += tile_y) {
		int tile_h;

		tile_h = tile_y;
		if (((top + height) - y) < tile_y) {
			tile_h = (top + height) - y;
		}
		if (tile_h <= 0) {
			break;
		}
		for (x = left; x < (left + width); x += tile_x) {
			int tile_w;

			tile_w = tile_x;
			if (((left + width) - x) < tile_x) {
				tile_w = (left + width) - x;
			}
			if (tile_w <= 0) {
				break;
			}
			clip.x0 = x;
			clip.y0 = y;
			clip.x1 = x + tile_w;
			clip.y1 = y + tile_h;
			if (cw->draw(cw, -x, -y, &clip, &ctx) == NSERROR_OK) {
				ami_clearclipreg(cw->gg);
				BltBitMapRastPort(srcbm, 0, 0, winrp,
						  _mleft(obj) + (x - sx),
						  _mtop(obj) + (y - sy),
						  tile_w, tile_h, 0xC0);
			}
		}
	}
}

/*
 * Amiga FE queues redraw via ami_schedule(1, …) and never paints from
 * inside IDCMP/gadget notify. Same rule for MUI: never MUI_Redraw from
 * Prop notify or core invalidate while Application_NewInput is nested.
 */
static void cw_redraw_cb(void *p)
{
	struct ami_corewindow *cw;
	ULONG open;

	cw = (struct ami_corewindow *)p;
	if (cw == NULL) {
		return;
	}
	cw->redraw_scheduled = false;
	if (cw->close_window) {
		return;
	}
	if (cw->objects[GID_CW_WIN] == NULL ||
	    cw->objects[GID_CW_DRAW] == NULL) {
		return;
	}
	open = FALSE;
	get(cw->objects[GID_CW_WIN], MUIA_Window_Open, &open);
	if (!open) {
		return;
	}
	MUI_Redraw(cw->objects[GID_CW_DRAW], MADF_DRAWOBJECT);
}

static void cw_invalidate_redraw(struct ami_corewindow *cw)
{
	if (cw == NULL || cw->objects[GID_CW_DRAW] == NULL) {
		return;
	}
	if (cw->close_window) {
		return;
	}
	if (cw->redraw_scheduled) {
		return;
	}
	cw->redraw_scheduled = true;
	tsunami_schedule(0, cw_redraw_cb, cw);
}

static void cw_close_deferred(void *p)
{
	struct ami_corewindow *cw;

	cw = (struct ami_corewindow *)p;
	if (cw == NULL || cw->close == NULL) {
		return;
	}
	/* close() owns the outer window struct and calls ami_corewindow_fini */
	cw->close(cw);
}

void ami_corewindow_request_close(struct ami_corewindow *ami_cw)
{
	if (ami_cw == NULL || ami_cw->close_window) {
		return;
	}
	ami_cw->close_window = true;
	/* Drop pending paints before dispose (Amiga: ami_schedule(-1, …)). */
	if (ami_cw->redraw_scheduled) {
		tsunami_schedule(-1, cw_redraw_cb, ami_cw);
		ami_cw->redraw_scheduled = false;
	}
	tsunami_schedule(0, cw_close_deferred, ami_cw);
}

/* ---------- core_window_table ---------- */

static nserror
ami_cw_invalidate_area(struct core_window *cw, const struct rect *r)
{
	struct ami_corewindow *ami_cw = (struct ami_corewindow *)cw;

	(void)r;
	cw_invalidate_redraw(ami_cw);
	return NSERROR_OK;
}

static nserror
ami_cw_get_window_dimensions(const struct core_window *cw,
			     int *width, int *height)
{
	struct ami_corewindow *ami_cw = (struct ami_corewindow *)cw;

	if (ami_cw->view_w < 1) {
		ami_cw->view_w = 200;
	}
	if (ami_cw->view_h < 1) {
		ami_cw->view_h = 150;
	}
	*width = ami_cw->view_w;
	*height = ami_cw->view_h;
	return NSERROR_OK;
}

static void cw_sync_scrollbars(struct ami_corewindow *cw)
{
	Object *hbar;
	Object *vbar;

	hbar = cw->objects[GID_CW_HSCROLL];
	vbar = cw->objects[GID_CW_VSCROLL];

	if (hbar != NULL) {
		set(hbar, MUIA_Prop_Entries,
		    cw->doc_w > 0 ? (ULONG)cw->doc_w : 1);
		set(hbar, MUIA_Prop_Visible,
		    cw->view_w > 0 ? (ULONG)cw->view_w : 1);
		set(hbar, MUIA_Prop_First, (ULONG)cw->scrollx);
	}
	if (vbar != NULL) {
		set(vbar, MUIA_Prop_Entries,
		    cw->doc_h > 0 ? (ULONG)cw->doc_h : 1);
		set(vbar, MUIA_Prop_Visible,
		    cw->view_h > 0 ? (ULONG)cw->view_h : 1);
		set(vbar, MUIA_Prop_First, (ULONG)cw->scrolly);
	}
}

static nserror
ami_cw_update_size(struct core_window *cw, int width, int height)
{
	struct ami_corewindow *ami_cw = (struct ami_corewindow *)cw;

	if (width >= 0) {
		ami_cw->doc_w = width;
	}
	if (height >= 0) {
		ami_cw->doc_h = height;
	}
	cw_sync_scrollbars(ami_cw);
	return NSERROR_OK;
}

static nserror
ami_cw_get_scroll(const struct core_window *cw, int *x, int *y)
{
	struct ami_corewindow *ami_cw = (struct ami_corewindow *)cw;

	*x = ami_cw->scrollx;
	*y = ami_cw->scrolly;
	return NSERROR_OK;
}

static nserror
ami_cw_set_scroll(struct core_window *cw, int x, int y)
{
	struct ami_corewindow *ami_cw = (struct ami_corewindow *)cw;

	ami_cw->scrollx = x;
	ami_cw->scrolly = y;
	cw_sync_scrollbars(ami_cw);
	cw_invalidate_redraw(ami_cw);
	return NSERROR_OK;
}

static nserror
ami_cw_drag_status(struct core_window *cw, core_window_drag_status ds)
{
	struct ami_corewindow *ami_cw = (struct ami_corewindow *)cw;

	ami_cw->drag_status = ds;
	return NSERROR_OK;
}

static struct core_window_table ami_cw_cb_table = {
	ami_cw_invalidate_area,
	ami_cw_update_size,
	ami_cw_set_scroll,
	ami_cw_get_scroll,
	ami_cw_get_window_dimensions,
	ami_cw_drag_status
};

struct core_window_table *amiga_core_window_table = &ami_cw_cb_table;

/* ---------- Area class ---------- */

static ULONG caAskMinMax(struct IClass *cl, Object *obj,
			 struct MUIP_AskMinMax *msg)
{
	DoSuperMethodA(cl, obj, (Msg)msg);
	msg->MinMaxInfo->MinWidth += 80;
	msg->MinMaxInfo->DefWidth += 200;
	msg->MinMaxInfo->MaxWidth += MUI_MAXMAX;
	msg->MinMaxInfo->MinHeight += 60;
	msg->MinMaxInfo->DefHeight += 150;
	msg->MinMaxInfo->MaxHeight += MUI_MAXMAX;
	return 0;
}

static ULONG caDraw(struct IClass *cl, Object *obj, struct MUIP_Draw *msg)
{
	struct CoreAreaData *data;

	data = INST_DATA(cl, obj);
	DoSuperMethodA(cl, obj, (Msg)msg);
	if (!(msg->flags & (MADF_DRAWOBJECT | MADF_DRAWUPDATE))) {
		return 0;
	}
	if (data->cw != NULL) {
		data->cw->view_w = _mwidth(obj);
		data->cw->view_h = _mheight(obj);
		cw_paint(obj, data->cw);
	}
	return 0;
}

static ULONG caSetup(struct IClass *cl, Object *obj, Msg msg)
{
	struct CoreAreaData *data;

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
	return TRUE;
}

static ULONG caCleanup(struct IClass *cl, Object *obj, Msg msg)
{
	struct CoreAreaData *data;

	data = INST_DATA(cl, obj);
	DoMethod(_win(obj), MUIM_Window_RemEventHandler, &data->ehnode);
	return DoSuperMethodA(cl, obj, msg);
}

static ULONG caShow(struct IClass *cl, Object *obj, Msg msg)
{
	struct CoreAreaData *data;
	ULONG ok;

	ok = DoSuperMethodA(cl, obj, msg);
	if (!ok) {
		return FALSE;
	}
	data = INST_DATA(cl, obj);
	data->shown = 1;
	tsunami_set_screen(_screen(obj));
	if (data->cw != NULL) {
		data->cw->view_w = _mwidth(obj);
		data->cw->view_h = _mheight(obj);
		cw_sync_scrollbars(data->cw);
	}
	return TRUE;
}

static ULONG caHide(struct IClass *cl, Object *obj, Msg msg)
{
	struct CoreAreaData *data;

	data = INST_DATA(cl, obj);
	data->shown = 0;
	return DoSuperMethodA(cl, obj, msg);
}

static void ca_mouse_xy(Object *obj, struct ami_corewindow *cw,
			struct IntuiMessage *imsg, int *ox, int *oy)
{
	*ox = (int)imsg->MouseX - _mleft(obj) + cw->scrollx;
	*oy = (int)imsg->MouseY - _mtop(obj) + cw->scrolly;
}

static ULONG caHandleEvent(struct IClass *cl, Object *obj,
			   struct MUIP_HandleEvent *msg)
{
	struct CoreAreaData *data;
	struct ami_corewindow *cw;
	struct IntuiMessage *imsg;
	int x, y;
	browser_mouse_state state;

	data = INST_DATA(cl, obj);
	cw = data->cw;
	(void)cl;
	if (cw == NULL || cw->mouse == NULL || msg->imsg == NULL) {
		return 0;
	}
	imsg = msg->imsg;

	switch (imsg->Class) {
	case IDCMP_MOUSEBUTTONS:
		ca_mouse_xy(obj, cw, imsg, &x, &y);
		state = BROWSER_MOUSE_HOVER;
		if (imsg->Code == SELECTDOWN) {
			state = BROWSER_MOUSE_PRESS_1;
			cw->mouse_state = BROWSER_MOUSE_PRESS_1;
			cw->mouse_x_click = x;
			cw->mouse_y_click = y;
			cw->drag_x_start = x;
			cw->drag_y_start = y;
		} else if (imsg->Code == SELECTUP) {
			state = BROWSER_MOUSE_CLICK_1;
			cw->mouse_state = BROWSER_MOUSE_HOVER;
		} else if (imsg->Code == MENUDOWN) {
			state = BROWSER_MOUSE_PRESS_2;
			cw->mouse_state = BROWSER_MOUSE_PRESS_2;
		} else if (imsg->Code == MENUUP) {
			state = BROWSER_MOUSE_CLICK_2;
			cw->mouse_state = BROWSER_MOUSE_HOVER;
		}
		cw->mouse(cw, state, x, y);
		if (cw->mouse_state == BROWSER_MOUSE_HOVER &&
		    cw->drag_end != NULL) {
			cw->drag_end(cw, x, y);
		}
		return MUI_EventHandlerRC_Eat;

	case IDCMP_MOUSEMOVE:
		ca_mouse_xy(obj, cw, imsg, &x, &y);
		state = BROWSER_MOUSE_HOVER;
		if (cw->mouse_state & BROWSER_MOUSE_PRESS_1) {
			state = BROWSER_MOUSE_DRAG_1;
		} else if (cw->mouse_state & BROWSER_MOUSE_PRESS_2) {
			state = BROWSER_MOUSE_DRAG_2;
		}
		cw->mouse(cw, state, x, y);
		return MUI_EventHandlerRC_Eat;

	case IDCMP_RAWKEY:
		if (cw->key != NULL) {
			/* Map a few common keys; full map can grow later. */
			if (imsg->Code == 0x45) { /* Esc */
				cw->key(cw, NS_KEY_ESCAPE);
			} else if (imsg->Code == 0x4C) {
				cw->key(cw, NS_KEY_UP);
			} else if (imsg->Code == 0x4D) {
				cw->key(cw, NS_KEY_DOWN);
			} else if (imsg->Code == 0x4F) {
				cw->key(cw, NS_KEY_LEFT);
			} else if (imsg->Code == 0x4E) {
				cw->key(cw, NS_KEY_RIGHT);
			} else if (imsg->Code == 0x46) {
				cw->key(cw, NS_KEY_DELETE_RIGHT);
			} else if (imsg->Code == 0x41) {
				cw->key(cw, NS_KEY_DELETE_LEFT);
			}
		}
		return MUI_EventHandlerRC_Eat;

	default:
		break;
	}
	return 0;
}

static ULONG caSet(struct IClass *cl, Object *obj, struct opSet *msg)
{
	struct CoreAreaData *data;
	struct TagItem *tags;
	struct TagItem *tag;

	data = INST_DATA(cl, obj);
	tags = msg->ops_AttrList;
	while ((tag = NextTagItem(&tags)) != NULL) {
		if (tag->ti_Tag == MUIA_Tsunami_CW_Core) {
			data->cw = (struct ami_corewindow *)tag->ti_Data;
		}
	}
	return DoSuperMethodA(cl, obj, (Msg)msg);
}

static ULONG caNew(struct IClass *cl, Object *obj, struct opSet *msg)
{
	struct CoreAreaData *data;
	struct TagItem *tags;
	struct TagItem *tag;

	/*
	 * Do not call caSet from OM_NEW — caSet's DoSuperMethodA would
	 * re-dispatch this same OM_NEW message and hang classic MUI.
	 */
	obj = (Object *)DoSuperMethodA(cl, obj, (Msg)msg);
	if (obj == NULL) {
		return 0;
	}
	data = INST_DATA(cl, obj);
	memset(data, 0, sizeof(*data));

	tags = msg->ops_AttrList;
	while ((tag = NextTagItem(&tags)) != NULL) {
		if (tag->ti_Tag == MUIA_Tsunami_CW_Core) {
			data->cw = (struct ami_corewindow *)tag->ti_Data;
		}
	}
	return (ULONG)obj;
}

static ULONG ASM SAVEDS CoreAreaDispatcher(REG(a0, struct IClass *cl),
					   REG(a2, Object *obj),
					   REG(a1, Msg msg))
{
	switch (msg->MethodID) {
	case OM_NEW:
		return caNew(cl, obj, (struct opSet *)msg);
	case OM_SET:
		return caSet(cl, obj, (struct opSet *)msg);
	case MUIM_AskMinMax:
		return caAskMinMax(cl, obj, (struct MUIP_AskMinMax *)msg);
	case MUIM_Draw:
		return caDraw(cl, obj, (struct MUIP_Draw *)msg);
	case MUIM_Setup:
		return caSetup(cl, obj, msg);
	case MUIM_Cleanup:
		return caCleanup(cl, obj, msg);
	case MUIM_Show:
		return caShow(cl, obj, msg);
	case MUIM_Hide:
		return caHide(cl, obj, msg);
	case MUIM_HandleEvent:
		return caHandleEvent(cl, obj, (struct MUIP_HandleEvent *)msg);
	default:
		return DoSuperMethodA(cl, obj, msg);
	}
}

BOOL tsunami_corewindow_class_init(void)
{
	if (TsunamiCoreAreaClass != NULL) {
		return TRUE;
	}
	TsunamiCoreAreaClass = MUI_CreateCustomClass(NULL, MUIC_Area, NULL,
						    sizeof(struct CoreAreaData),
						    (APTR)CoreAreaDispatcher);
	return TsunamiCoreAreaClass != NULL;
}

void tsunami_corewindow_class_fini(void)
{
	if (TsunamiCoreAreaClass != NULL) {
		MUI_DeleteCustomClass(TsunamiCoreAreaClass);
		TsunamiCoreAreaClass = NULL;
	}
}

/* ---------- scroll notify ---------- */

static void cw_scroll_from_prop(struct ami_corewindow *cw)
{
	ULONG hx, vy;

	hx = 0;
	vy = 0;
	if (cw->objects[GID_CW_HSCROLL] != NULL) {
		get(cw->objects[GID_CW_HSCROLL], MUIA_Prop_First, &hx);
	}
	if (cw->objects[GID_CW_VSCROLL] != NULL) {
		get(cw->objects[GID_CW_VSCROLL], MUIA_Prop_First, &vy);
	}
	cw->scrollx = (int)hx;
	cw->scrolly = (int)vy;
	cw_invalidate_redraw(cw);
}

static void SAVEDS ASM
cw_scroll_func(REG(a0, struct Hook *hook),
	       REG(a2, Object *obj),
	       REG(a1, APTR arg))
{
	struct ami_corewindow *cw;

	(void)obj;
	(void)arg;
	cw = (struct ami_corewindow *)hook->h_Data;
	if (cw != NULL) {
		cw_scroll_from_prop(cw);
	}
}

static void SAVEDS ASM
cw_close_func(REG(a0, struct Hook *hook),
	      REG(a2, Object *obj),
	      REG(a1, APTR arg))
{
	struct ami_corewindow *cw;

	(void)obj;
	(void)arg;
	cw = (struct ami_corewindow *)hook->h_Data;
	/* Amiga: menu Close sets close_window; WMHI_CLOSEWINDOW destroys
	 * from the event loop. Never DisposeObject inside this notify. */
	ami_corewindow_request_close(cw);
}

/* ---------- init / fini ---------- */

nserror ami_corewindow_init(struct ami_corewindow *ami_cw)
{
	Object *area;
	Object *hbar;
	Object *vbar;
	Object *win;

	if (ami_cw == NULL || ami_cw->draw == NULL) {
		return NSERROR_BAD_PARAMETER;
	}
	if (tsunami_app == NULL) {
		return NSERROR_INIT_FAILED;
	}
	if (!tsunami_corewindow_class_init()) {
		return NSERROR_INIT_FAILED;
	}

	ami_cw->drag_status = CORE_WINDOW_DRAG_NONE;
	ami_cw->mouse_state = BROWSER_MOUSE_HOVER;
	ami_cw->scrollx = 0;
	ami_cw->scrolly = 0;
	ami_cw->doc_w = 1;
	ami_cw->doc_h = 1;
	ami_cw->view_w = 200;
	ami_cw->view_h = 150;
	ami_cw->close_window = false;
	ami_cw->redraw_scheduled = false;
	ami_cw->dragging = false;

	ami_cw->gg = ami_plot_ra_alloc(100, 100, false, true);
	if (ami_cw->gg == NULL) {
		return NSERROR_NOMEM;
	}
	ami_cw->shared_pens = ami_AllocMinList();
	if (ami_cw->shared_pens != NULL) {
		ami_plot_ra_set_pen_list(ami_cw->gg, ami_cw->shared_pens);
	}

	NSLOG(netsurf, INFO, "ami_corewindow_init: NewObject Area");
	area = NewObject(TsunamiCoreAreaClass->mcc_Class, NULL,
			 MUIA_Tsunami_CW_Core, (ULONG)ami_cw,
			 MUIA_FillArea, FALSE,
			 MUIA_InnerLeft, 0,
			 MUIA_InnerTop, 0,
			 MUIA_InnerRight, 0,
			 MUIA_InnerBottom, 0,
			 TAG_DONE);
	NSLOG(netsurf, INFO, "ami_corewindow_init: Area=%p Scrollbars",
	      (void *)area);
	hbar = ScrollbarObject,
		MUIA_Prop_Entries, 1,
		MUIA_Prop_Visible, 1,
		MUIA_Prop_First, 0,
		MUIA_Group_Horiz, TRUE,
	End;
	vbar = ScrollbarObject,
		MUIA_Prop_Entries, 1,
		MUIA_Prop_Visible, 1,
		MUIA_Prop_First, 0,
	End;
	NSLOG(netsurf, INFO, "ami_corewindow_init: hbar=%p vbar=%p Window",
	      (void *)hbar, (void *)vbar);

	if (area == NULL || hbar == NULL || vbar == NULL) {
		if (area != NULL) {
			MUI_DisposeObject(area);
		}
		if (hbar != NULL) {
			MUI_DisposeObject(hbar);
		}
		if (vbar != NULL) {
			MUI_DisposeObject(vbar);
		}
		ami_plot_ra_free(ami_cw->gg);
		ami_cw->gg = NULL;
		return NSERROR_NOMEM;
	}

	win = WindowObject,
		MUIA_Window_Title,
			ami_cw->wintitle != NULL ? ami_cw->wintitle : (STRPTR)"Tsunami",
		/* No shared Window_ID — each tree viewer is a distinct window. */
		MUIA_Window_Width, 320,
		MUIA_Window_Height, 240,
		MUIA_Window_CloseGadget, TRUE,
		MUIA_Window_DepthGadget, TRUE,
		MUIA_Window_SizeGadget, TRUE,
		MUIA_Window_DragBar, TRUE,
		WindowContents, VGroup,
			Child, HGroup,
				Child, area,
				Child, vbar,
			End,
			Child, hbar,
		End,
	End;

	if (win == NULL) {
		NSLOG(netsurf, WARNING, "ami_corewindow_init: WindowObject failed");
		MUI_DisposeObject(area);
		MUI_DisposeObject(hbar);
		MUI_DisposeObject(vbar);
		ami_plot_ra_free(ami_cw->gg);
		ami_cw->gg = NULL;
		return NSERROR_NOMEM;
	}
	NSLOG(netsurf, INFO, "ami_corewindow_init: Window=%p", (void *)win);

	ami_cw->objects[GID_CW_WIN] = win;
	ami_cw->objects[GID_CW_DRAW] = area;
	ami_cw->objects[GID_CW_HSCROLL] = hbar;
	ami_cw->objects[GID_CW_VSCROLL] = vbar;

	ami_cw->scroll_hook.h_Entry = (HOOKFUNC)cw_scroll_func;
	ami_cw->scroll_hook.h_Data = ami_cw;
	ami_cw->close_hook.h_Entry = (HOOKFUNC)cw_close_func;
	ami_cw->close_hook.h_Data = ami_cw;

	DoMethod(hbar, MUIM_Notify, MUIA_Prop_First, MUIV_EveryTime,
		 win, 3, MUIM_CallHook, &ami_cw->scroll_hook, 0);
	DoMethod(vbar, MUIM_Notify, MUIA_Prop_First, MUIV_EveryTime,
		 win, 3, MUIM_CallHook, &ami_cw->scroll_hook, 0);
	DoMethod(win, MUIM_Notify, MUIA_Window_CloseRequest, TRUE,
		 win, 3, MUIM_CallHook, &ami_cw->close_hook, 0);

	/*
	 * Add to the Application but leave closed. Callers open after their
	 * manager_init so the first MUIM_Draw does not paint an empty tree
	 * (and so Open is not nested inside Application_NewInput ReturnID).
	 */
	DoMethod(tsunami_app, OM_ADDMEMBER, win);
	NSLOG(netsurf, INFO, "ami_corewindow_init ADDMEMBER ok (closed)");
	ami_cw->win = NULL;
	return NSERROR_OK;
}

/**
 * Open a core window that ami_corewindow_init left closed.
 */
void ami_corewindow_open(struct ami_corewindow *ami_cw)
{
	if (ami_cw == NULL || ami_cw->objects[GID_CW_WIN] == NULL) {
		return;
	}
	/* Re-present after CloseGadget: cancel pending deferred destroy. */
	if (ami_cw->close_window) {
		tsunami_schedule(-1, cw_close_deferred, ami_cw);
		ami_cw->close_window = false;
	}
	NSLOG(netsurf, INFO, "ami_corewindow_open title=%s",
	      ami_cw->wintitle != NULL ? ami_cw->wintitle : "(null)");
	set(ami_cw->objects[GID_CW_WIN], MUIA_Window_Open, TRUE);
	get(ami_cw->objects[GID_CW_WIN], MUIA_Window_Window,
	    (ULONG *)&ami_cw->win);
	NSLOG(netsurf, INFO, "ami_corewindow_open done win=%p",
	      (void *)ami_cw->win);
}

nserror ami_corewindow_fini(struct ami_corewindow *ami_cw)
{
	if (ami_cw == NULL) {
		return NSERROR_OK;
	}

	/* Match Amiga ami_corewindow_fini: cancel queued redraw before dispose. */
	ami_cw->close_window = true;
	if (ami_cw->redraw_scheduled) {
		tsunami_schedule(-1, cw_redraw_cb, ami_cw);
		ami_cw->redraw_scheduled = false;
	}
	tsunami_schedule(-1, cw_close_deferred, ami_cw);

	if (ami_cw->objects[GID_CW_WIN] != NULL) {
		set(ami_cw->objects[GID_CW_WIN], MUIA_Window_Open, FALSE);
		DoMethod(tsunami_app, OM_REMMEMBER, ami_cw->objects[GID_CW_WIN]);
		MUI_DisposeObject(ami_cw->objects[GID_CW_WIN]);
		ami_cw->objects[GID_CW_WIN] = NULL;
		ami_cw->objects[GID_CW_DRAW] = NULL;
		ami_cw->objects[GID_CW_HSCROLL] = NULL;
		ami_cw->objects[GID_CW_VSCROLL] = NULL;
	}
	ami_cw->win = NULL;

	if (ami_cw->shared_pens != NULL) {
		ami_plot_release_pens(ami_cw->shared_pens);
		/* list itself owned via ami_AllocMinList — free nodes via plot */
		ami_cw->shared_pens = NULL;
	}
	if (ami_cw->gg != NULL) {
		ami_plot_ra_free(ami_cw->gg);
		ami_cw->gg = NULL;
	}
	if (ami_cw->wintitle != NULL) {
		ami_utf8_free(ami_cw->wintitle);
		ami_cw->wintitle = NULL;
	}
	return NSERROR_OK;
}
