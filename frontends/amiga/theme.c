/*
 * Copyright 2010 Chris Young <chris@unsatisfactorysoftware.co.uk>
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

#include "amiga/os3support.h"

#include <stdlib.h>
#include <string.h>

#include <proto/clicktab.h>
#include <proto/datatypes.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/intuition.h>

#include <gadgets/clicktab.h>
#include <gadgets/space.h>
#ifdef __amigaos4__
#include <graphics/blitattr.h>
#endif
#include <images/led.h>
#include <intuition/imageclass.h>
#include <intuition/pointerclass.h>

#include "utils/messages.h"
#include "utils/nsoption.h"
#include "utils/utils.h"
#include "desktop/searchweb.h"
#include "netsurf/mouse.h"
#include "netsurf/window.h"

#include "amiga/gui.h"
#include "amiga/drag.h"
#include "amiga/bitmap.h"
#include "amiga/plotters.h"
#include "amiga/schedule.h"
#include "amiga/theme.h"
#include "amiga/misc.h"

static struct BitMap *throbber = NULL;
static struct bitmap *throbber_nsbm = NULL;
static int throbber_frames = 1;
static int throbber_update_interval;

/*
 * NetSurf shape → Intuition WA_PointerType (V47.34 / V53.37+).
 * Theme custom pointer images are not used; prefs supply the imagery.
 */
static const ULONG ami_os_pointer_type[AMI_LASTPOINTER + 1] = {
	POINTERTYPE_NORMAL,
	POINTERTYPE_LINK,
	POINTERTYPE_TEXT,
	POINTERTYPE_CONTEXTMENU,
	POINTERTYPE_NORTHRESIZE,
	POINTERTYPE_SOUTHRESIZE,
	POINTERTYPE_WESTRESIZE,
	POINTERTYPE_EASTRESIZE,
	POINTERTYPE_NORTHEASTRESIZE,
	POINTERTYPE_SOUTHWESTRESIZE,
	POINTERTYPE_NORTHWESTRESIZE,
	POINTERTYPE_SOUTHEASTRESIZE,
	POINTERTYPE_CROSS,
	POINTERTYPE_HAND,
	POINTERTYPE_BUSY,
	POINTERTYPE_HELP,
	POINTERTYPE_NODROP,
	POINTERTYPE_NOTALLOWED,
	POINTERTYPE_PROGRESS,
	POINTERTYPE_NONE,
	POINTERTYPE_DRAGANDDROP
};

/** True when SetWindowPointer accepts WA_PointerType. */
static BOOL
ami_pointer_type_supported(void)
{
	/* OS3.2: intuition 47.34+; OS4: 53.37+ (covered by version > 47). */
	if (LIB_IS_AT_LEAST((struct Library *)IntuitionBase, 47, 34)) {
		return TRUE;
	}
	return FALSE;
}

void ami_theme_init(void)
{
	char themefile[1024];
	BPTR lock = 0;

	strcpy(themefile,nsoption_charp(theme));
	AddPart(themefile,"Theme",100);

	lock = Lock(themefile,ACCESS_READ);

	if(!lock)
	{
		amiga_warn_user("ThemeApplyErr",nsoption_charp(theme));
		strcpy(themefile,"PROGDIR:Resources/Themes/Default/Theme");
		nsoption_set_charp(theme, (char *)strdup("PROGDIR:Resources/Themes/Default"));
	}
	else
	{
		UnLock(lock);
	}

	lock = Lock(themefile,ACCESS_READ);
	if(lock)
	{
		UnLock(lock);
		messages_add_from_file(themefile);
	}
}

int ami_theme_throbber_get_width(void)
{
	if(throbber_nsbm != NULL)
		return bitmap_get_width(throbber_nsbm) / throbber_frames;
	return 14 + 4 + 24;
}

int ami_theme_throbber_get_height(void)
{
	if(throbber_nsbm != NULL)
		return bitmap_get_height(throbber_nsbm);
	return 24;
}

void ami_theme_throbber_setup(void)
{
	char throbberfile[1024];
	struct bitmap *bm;

	ami_get_theme_filename(throbberfile,"theme_throbber",false);
	throbber_frames=atoi(messages_get("theme_throbber_frames"));
	if(throbber_frames == 0) throbber_frames = 1;
	throbber_update_interval = atoi(messages_get("theme_throbber_delay"));
	if(throbber_update_interval == 0) throbber_update_interval = 100;

	bm = ami_bitmap_from_datatype(throbberfile);
	if(bm == NULL) {
		throbber = NULL;
		throbber_nsbm = NULL;
		return;
	}
	throbber = ami_bitmap_get_native(bm, bitmap_get_width(bm), bitmap_get_height(bm),
		ami_plot_screen_is_palettemapped(), NULL, nsoption_colour(sys_colour_ButtonFace));

	throbber_nsbm = bm;
}

void ami_theme_throbber_free(void)
{
	amiga_bitmap_destroy(throbber_nsbm);
	throbber_nsbm = NULL;
	throbber = NULL;
}

void ami_get_theme_filename(char *filename, const char *themestring, bool protocol)
{
	if(protocol)
		strcpy(filename,"file:///");
	else
		strcpy(filename,"");

	if(messages_get(themestring)[0] == '*')
	{
		strncat(filename,messages_get(themestring)+1,100);
	}
	else
	{
		strcat(filename, nsoption_charp(theme));
		AddPart(filename, messages_get(themestring), 100);
	}
}

void gui_window_set_pointer(struct gui_window *g, gui_pointer_shape shape)
{
	ami_set_pointer(ami_gui_get_gui_window_2(g), shape, true);
}

void ami_update_pointer(struct Window *win, gui_pointer_shape shape)
{
	ULONG ptype;
	BOOL ptr_delay;

	if (win == NULL) {
		return;
	}
	if (ami_drag_has_data()) {
		return; /**\todo check this shouldn't be drag_in_progress */
	}

	if ((int)shape < 0 || (int)shape > AMI_LASTPOINTER) {
		shape = GUI_POINTER_DEFAULT;
	}

	/*
	 * Prefer built-in pointer types (prefs imagery). On older Intuition,
	 * only busy vs default Preferences pointer are available.
	 */
	if (ami_pointer_type_supported()) {
		ptr_delay = FALSE;
		if (shape == GUI_POINTER_WAIT) {
			ptr_delay = TRUE;
		}
		ptype = ami_os_pointer_type[shape];
		SetWindowPointer(win,
				WA_PointerType, ptype,
				WA_PointerDelay, ptr_delay,
				TAG_DONE);
		return;
	}

	if (shape == GUI_POINTER_WAIT) {
		SetWindowPointer(win,
				WA_BusyPointer, TRUE,
				WA_PointerDelay, TRUE,
				TAG_DONE);
	} else {
		/* NULL WA_Pointer → Preferences default pointer */
		SetWindowPointer(win, TAG_DONE);
	}
}

void ami_init_mouse_pointers(void)
{
	/* System / Preferences pointers only — no theme pointerclass objects. */
}

void ami_mouse_pointers_free(void)
{
}

void gui_window_start_throbber(struct gui_window *g)
{
	if(!g) return;
	if(nsoption_bool(kiosk_mode)) return;

#ifdef __amigaos4__
	if(ami_gui_get_tab_node(g) && (ami_gui2_get_tabs(ami_gui_get_gui_window_2(g)) > 1))
	{
		SetClickTabNodeAttrs(ami_gui_get_tab_node(g), TNA_Flagged, TRUE, TAG_DONE);
		RefreshGadgets((APTR)ami_gui2_get_object(ami_gui_get_gui_window_2(g), AMI_GAD_TABS),
			ami_gui_get_window(g), NULL);
	}
#endif

	ami_gui_set_throbbing(g, true);
	if(ami_gui_get_throbber_frame(g) == 0) ami_gui_set_throbber_frame(g, 1);
	ami_throbber_redraw_schedule(throbber_update_interval, g);
}

/** Prefer system boingball (and LED when present). */
static bool ami_throbber_use_sys(struct gui_window *g)
{
	struct gui_window_2 *gwin;

	gwin = ami_gui_get_gui_window_2(g);
	if(gwin == NULL)
		return false;
	return (ami_gui2_get_throbber_boing(gwin) != NULL) ? true : false;
}

/**
 * Draw network LED blink lights and/or spinning boingball.
 * When active is TRUE, advances the boingball frame via IM_MOVE.
 */
static void ami_throbber_draw_sys(struct gui_window *g, struct IBox *bbox, BOOL active)
{
	struct gui_window_2 *gwin;
	Object *led;
	Object *boing;
	WORD *vals;
	struct Window *win;
	struct DrawInfo *dri;
	struct Image *lim;
	struct impDraw imsg;
	WORD led_w;
	WORD bx;

	gwin = ami_gui_get_gui_window_2(g);
	led = ami_gui2_get_throbber_led(gwin);
	boing = ami_gui2_get_throbber_boing(gwin);
	vals = ami_gui2_get_throbber_led_vals(gwin);
	win = ami_gui_get_window(g);
	if(boing == NULL || win == NULL)
		return;

	led_w = 0;
	bx = bbox->Left;
	if(led != NULL) {
		lim = (struct Image *)led;
		led_w = (lim->Width > 0) ? lim->Width : 14;
		bx = bbox->Left + led_w + 4;
	}

	dri = GetScreenDrawInfo(win->WScreen);
	if(dri != NULL) {
		SetAPen(win->RPort, dri->dri_Pens[BACKGROUNDPEN]);
		RectFill(win->RPort, bbox->Left, bbox->Top,
				(WORD)(bbox->Left + bbox->Width - 1),
				(WORD)(bbox->Top + bbox->Height - 1));
	}

	if(led != NULL && vals != NULL) {
		/* LED_Raw: all segments = solid activity lamps */
		vals[0] = active ? ((ami_gui_get_throbber_frame(g) & 1) ? 0x7F7F : 0) : 0;
		SetAttrs(led, LED_Values, vals, LED_Raw, TRUE, TAG_DONE);
		DrawImage(win->RPort, (struct Image *)led, bbox->Left, bbox->Top);
	}

	if(active) {
		imsg.MethodID = IM_MOVE;
		imsg.imp_RPort = win->RPort;
		imsg.imp_Offset.X = bx;
		imsg.imp_Offset.Y = bbox->Top;
		imsg.imp_State = IDS_NORMAL;
		imsg.imp_DrInfo = dri;
		imsg.imp_Dimensions.Width = 0;
		imsg.imp_Dimensions.Height = 0;
		DoMethodA(boing, (Msg)&imsg);
	} else {
		DrawImage(win->RPort, (struct Image *)boing, bx, bbox->Top);
	}
	if(dri != NULL)
		FreeScreenDrawInfo(win->WScreen, dri);
}

void gui_window_stop_throbber(struct gui_window *g)
{
	struct IBox *bbox;

	if(!g) return;
	if(nsoption_bool(kiosk_mode)) return;

#ifdef __amigaos4__
	if(ami_gui_get_tab_node(g) && (ami_gui2_get_tabs(ami_gui_get_gui_window_2(g)) > 1))
	{
		SetClickTabNodeAttrs(ami_gui_get_tab_node(g), TNA_Flagged, FALSE, TAG_DONE);
		RefreshGadgets((APTR)ami_gui2_get_object(ami_gui_get_gui_window_2(g), AMI_GAD_TABS),
			ami_gui_get_window(g), NULL);
	}
#endif

	if(IS_CURRENT_GW(ami_gui_get_gui_window_2(g), g)) {
		if(ami_gui_get_space_box(ami_gui2_get_object(ami_gui_get_gui_window_2(g), AMI_GAD_THROBBER), &bbox) != NSERROR_OK) {
			amiga_warn_user("NoMemory", "");
			return;
		}

		if(ami_throbber_use_sys(g)) {
			ami_throbber_draw_sys(g, bbox, FALSE);
		} else if(throbber != NULL) {
			BltBitMapRastPort(throbber, 0, 0, ami_gui_get_window(g)->RPort,
				bbox->Left, bbox->Top, 
				ami_theme_throbber_get_width(), ami_theme_throbber_get_height(),
				0x0C0);
		}
		ami_gui_free_space_box(bbox);
	}

	ami_gui_set_throbbing(g, false);
	ami_throbber_redraw_schedule(-1, g);
}

static void ami_throbber_update(void *p)
{
	struct gui_window *g = (struct gui_window *)p;
	struct IBox *bbox;
	int frame = 0;

	if(!g) return;
	if(!ami_gui2_get_object(ami_gui_get_gui_window_2(g), AMI_GAD_THROBBER)) return;

	if(ami_gui_get_throbbing(g) == true) {
		frame = ami_gui_get_throbber_frame(g);
		ami_gui_set_throbber_frame(g, frame + 1);
		if(ami_throbber_use_sys(g)) {
			if(ami_gui_get_throbber_frame(g) > 6)
				ami_gui_set_throbber_frame(g, 1);
		} else if(ami_gui_get_throbber_frame(g) > (throbber_frames-1)) {
			ami_gui_set_throbber_frame(g, 1);
		}
	}

	if(IS_CURRENT_GW(ami_gui_get_gui_window_2(g),g)) {
		if(ami_gui_get_space_box(ami_gui2_get_object(ami_gui_get_gui_window_2(g), AMI_GAD_THROBBER), &bbox) != NSERROR_OK) {
			amiga_warn_user("NoMemory", "");
			return;
		}

		if(ami_throbber_use_sys(g)) {
			ami_throbber_draw_sys(g, bbox, TRUE);
		} else if(throbber != NULL) {
#ifdef __amigaos4__
			BltBitMapTags(BLITA_SrcX, ami_theme_throbber_get_width() * frame,
						BLITA_SrcY, 0,
						BLITA_DestX, bbox->Left,
						BLITA_DestY, bbox->Top,
						BLITA_Width, ami_theme_throbber_get_width(),
						BLITA_Height, ami_theme_throbber_get_height(),
						BLITA_Source, throbber,
						BLITA_Dest, ami_gui_get_window(g)->RPort,
						BLITA_SrcType, BLITT_BITMAP,
						BLITA_DestType, BLITT_RASTPORT,
					TAG_DONE);
#else
			BltBitMapRastPort(throbber, ami_theme_throbber_get_width() * frame,
				0, ami_gui_get_window(g)->RPort,
				bbox->Left, bbox->Top,
				ami_theme_throbber_get_width(), ami_theme_throbber_get_height(),
				0xC0);
#endif
		}
		ami_gui_free_space_box(bbox);
	}

	if(frame > 0) ami_throbber_redraw_schedule(throbber_update_interval, g);
}

void ami_throbber_redraw_schedule(int t, struct gui_window *g)
{
	ami_schedule(t, ami_throbber_update, g); 
}

