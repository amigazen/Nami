/*
 * Copyright 2008-2026 Chris Young <chris@unsatisfactorysoftware.co.uk>
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


#ifdef __amigaos4__
/* Custom StringView class */
#include "amiga/stringview/stringview.h"
#include "amiga/stringview/urlhistory.h"
#endif

/* AmigaOS libraries */
#ifdef __amigaos4__
#include <proto/application.h>
#endif
#include <proto/asl.h>
#include <proto/datatypes.h>
#include <proto/diskfont.h>
#include <proto/dos.h>
#include <dos/rdargs.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/icon.h>
#include <proto/intuition.h>
#include <proto/keymap.h>
#include <proto/layers.h>
#include <proto/locale.h>
#include <proto/utility.h>
#include <proto/wb.h>

#ifdef WITH_AMISSL
/* AmiSSL needs everything to use bsdsocket.library directly to avoid problems */
#include <proto/bsdsocket.h>
#define waitselect WaitSelect
#endif

/* Other OS includes */
#include <datatypes/textclass.h>
#include <devices/inputevent.h>
#include <graphics/gfxbase.h>
#include <graphics/gfxmacros.h>
#include <graphics/rpattr.h>
#ifdef __amigaos4__
#include <diskfont/diskfonttag.h>
#include <graphics/blitattr.h>
#include <intuition/gui.h>
#include <libraries/application.h>
#include <libraries/keymap.h>
#endif
#include <intuition/icclass.h>
#include <intuition/imageclass.h>
#include <intuition/gadgetclass.h>
#include <intuition/screens.h>
#include <libraries/gadtools.h>
#include <workbench/workbench.h>

/* ReAction libraries */
#include <proto/bevel.h>
#include <proto/bitmap.h>
#include <proto/button.h>
#include <proto/chooser.h>
#include <proto/clicktab.h>
#include <proto/label.h>
#include <proto/layout.h>
#include <proto/listbrowser.h>
#include <proto/scroller.h>
#include <proto/space.h>
#include <proto/speedbar.h>
#include <proto/string.h>
#include <proto/virtual.h>
#include <proto/window.h>

#include <classes/window.h>
#include <gadgets/button.h>
#include <gadgets/chooser.h>
#include <gadgets/clicktab.h>
#include <gadgets/layout.h>
#include <gadgets/listbrowser.h>
#include <gadgets/scroller.h>
#include <gadgets/space.h>
#include <gadgets/speedbar.h>
#include <gadgets/string.h>
#include <gadgets/virtual.h>
#include <images/bevel.h>
#include <images/bitmap.h>
#include <images/label.h>
#include <images/led.h>
#include <images/penmap.h>

#include <reaction/reaction_macros.h>

/* newlib includes */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>

/* NetSurf core includes */
#include "utils/log.h"
#include "utils/messages.h"
#include "utils/nsoption.h"
#include "utils/utf8.h"
#include "utils/utils.h"
#include "utils/nsurl.h"
#include "utils/file.h"
#include "netsurf/window.h"
#include "netsurf/fetch.h"
#include "netsurf/misc.h"
#include "netsurf/mouse.h"
#include "netsurf/netsurf.h"
#include "netsurf/content.h"
#include "netsurf/browser_window.h"
#include "netsurf/cookie_db.h"
#include "netsurf/url_db.h"
#include "netsurf/keypress.h"
#include "content/backing_store.h"
#include "content/fetch.h"
#ifdef WITH_AMIHTTP
#include "content/fetchers/amihttp.h"
#endif
#include "desktop/browser_history.h"
#include "desktop/hotlist.h"
#include "desktop/version.h"
#include "desktop/save_complete.h"
#include "desktop/searchweb.h"

/* NetSurf Amiga platform includes */
#include <libraries/gadtools.h>
#include <libraries/gtdrag.h>

#include "amiga/gui.h"
#include "amiga/arexx.h"
#include "amiga/bitmap.h"
#include "amiga/clipboard.h"
#include "amiga/cookies.h"
#include "amiga/ctxmenu.h"
#include "amiga/datatypes.h"
#include "amiga/download.h"
#include "amiga/drag.h"
#include "amiga/ami_gtdrag.h"
#include "amiga/file.h"
#include "amiga/filetype.h"
#include "amiga/font.h"
#include "amiga/gui_options.h"
#include "amiga/help.h"
#include "amiga/history.h"
#include "amiga/history_local.h"
#include "amiga/hotlist.h"
#include "amiga/icon.h"
#include "amiga/ico.h"
#include "amiga/launch.h"
#include "amiga/libs.h"
#include "amiga/memory.h"
#include "amiga/menu.h"
#include "amiga/misc.h"
#include "amiga/nsoption.h"
#include "amiga/pageinfo.h"
#include "amiga/plotters.h"
#include "amiga/plugin_hack.h"
#include "amiga/print.h"
#include "amiga/schedule.h"
#include "amiga/search.h"
#include "amiga/selectmenu.h"
#include "amiga/theme.h"
#include "amiga/utf8.h"
#include "amiga/corewindow.h"

#define AMINS_SCROLLERPEN NUMDRIPENS
#define NSA_KBD_SCROLL_PX 10
/* Mouse wheel step — was 50px/notch and felt jumpy */
#define NSA_WHEEL_SCROLL_PX 20
#define NSA_MAX_HOTLIST_BUTTON_LEN 20

#define SCROLL_TOP INT_MIN
#define SCROLL_PAGE_UP (INT_MIN + 1)
#define SCROLL_PAGE_DOWN (INT_MAX - 1)
#define SCROLL_BOTTOM (INT_MAX)

/* Horizontal band height for yield-during-paint (IDCMP between strips) */
#define AMI_PAINT_YIELD_STRIP 64

/* Content settle coalesce vs interactive scroll/resize (ms) */
#ifndef __amigaos4__
#define AMI_REDRAW_CONTENT_MS 100
#else
#define AMI_REDRAW_CONTENT_MS 20
#endif
#define AMI_REDRAW_INTERACTIVE_MS 20

/* Fixed gap between toolbar groups (nav | URL+info | throbber) */
#define AMI_TB_GROUP_GAP 8

/* Extra mouse button defines to match those in intuition/intuition.h */
#define SIDEDOWN  (IECODE_4TH_BUTTON)
#define SIDEUP    (IECODE_4TH_BUTTON | IECODE_UP_PREFIX)
#define EXTRADOWN (IECODE_5TH_BUTTON)
#define EXTRAUP   (IECODE_5TH_BUTTON | IECODE_UP_PREFIX)

/* Left OR Right Shift/Alt keys */
#define NSA_QUAL_SHIFT (IEQUALIFIER_RSHIFT | IEQUALIFIER_LSHIFT)
#define NSA_QUAL_ALT (IEQUALIFIER_RALT | IEQUALIFIER_LALT)

#ifdef __amigaos4__
#define NSA_STATUS_TEXT GA_Text
#else
#define NSA_STATUS_TEXT STRINGA_TextVal
#endif

#ifdef __amigaos4__
#define BOOL_MISMATCH(a,b) ((a == FALSE) && (b != FALSE)) || ((a != FALSE) && (b == FALSE))
#else
#define BOOL_MISMATCH(a,b) (1)
#endif

extern struct gui_utf8_table *amiga_utf8_table;

enum
{
    OID_MAIN = 0,
	OID_VSCROLL,
	OID_HSCROLL,
	GID_MAIN,
	GID_TABLAYOUT,
	GID_BROWSER,
	GID_STATUS,
	GID_URL,
	GID_ICON,
	GID_STOP,
	GID_STOP_BM,
	GID_STOP_BM_G, /* AISS _g ghosted */
	GID_STOP_BM_S, /* AISS _s selected/pressed */
	GID_STOP_BM_H, /* AISS _h hovered */
	GID_RELOAD,
	GID_RELOAD_BM,
	GID_RELOAD_BM_G,
	GID_RELOAD_BM_S,
	GID_RELOAD_BM_H,
	GID_HOME,
	GID_HOME_BM,
	GID_HOME_BM_G,
	GID_HOME_BM_S,
	GID_HOME_BM_H,
	GID_BACK,
	GID_BACK_BM,
	GID_BACK_BM_G,
	GID_BACK_BM_S,
	GID_BACK_BM_H,
	GID_FORWARD,
	GID_FORWARD_BM,
	GID_FORWARD_BM_G,
	GID_FORWARD_BM_S,
	GID_FORWARD_BM_H,
	GID_THROBBER,
	GID_SEARCH_ICON,
	GID_PAGEINFO,
	GID_PAGEINFO_INSECURE_BM,
	GID_PAGEINFO_INTERNAL_BM,
	GID_PAGEINFO_LOCAL_BM,
	GID_PAGEINFO_SECURE_BM,
	GID_PAGEINFO_WARNING_BM,
	GID_FAVE,
	GID_FAVE_ADD,
	GID_FAVE_RMV,
	GID_CLOSETAB,
	GID_CLOSETAB_BM,
	GID_CLOSETAB_BM_G,
	GID_ADDTAB_HINT,
	GID_TABS,
	GID_TABS_FLAG,
	GID_CHROMELAYOUT,
	GID_WIN_CLOSE,
	GID_WIN_CLOSE_BM,
	GID_WIN_ZOOM,
	GID_WIN_ZOOM_BM,
	GID_WIN_DEPTH,
	GID_WIN_DEPTH_BM,
	GID_WIN_DRAG,
	GID_CHROME_RPAD, /* Nami: room for SizeBRight scroller strip */
	/* Nami: left sidebar (hotlist favicons + tab buttons) */
	GID_SIDELAYOUT, /* LayoutV panel — no virtual.gadget */
	GID_SIDE_VIRTUAL, /* unused (was Virtual wrapper); keep objects[] index */
	GID_SIDE_NEWTAB,
	GID_SIDE_NEWTAB_BM,
	GID_SIDE_CLOSETAB, /* close current tab */
	GID_SIDE_CLOSETAB_BM,
	GID_SIDE_HOTLAYOUT, /* V of H rows: HotlistToolbar favicons (wraps) */
	GID_SIDE_TABLIST, /* sidebar LayoutV: pool of tab ButtonObjs */
	GID_SIDE_TOGGLE,
	GID_SIDE_TOGGLE_BM,
	GID_SIDE_STRIP, /* legacy zero-size placeholder */
	GID_BODYLAYOUT, /* H: sidebar | weight | browser column */
	GID_BROWSERCOL, /* browser column inside BODYLAYOUT */
	GID_SEARCHSTRING,
	GID_TOOLBARLAYOUT,
	GID_HOTLIST,
	GID_HOTLISTLAYOUT,
	GID_HOTLISTSEPBAR,
	GID_HSCROLL,
	GID_HSCROLLLAYOUT,
	GID_VSCROLL,
	GID_VSCROLLLAYOUT,
	/* Nami border scroll chrome (sysiclass arrows + images) */
	GID_SCROLL_UP,
	GID_SCROLL_DOWN,
	GID_SCROLL_LEFT,
	GID_SCROLL_RIGHT,
	GID_SCROLL_UP_BM,
	GID_SCROLL_DOWN_BM,
	GID_SCROLL_LEFT_BM,
	GID_SCROLL_RIGHT_BM,
	GID_LOGLAYOUT,
	GID_LOG,
	GID_LAST
};

/* Nami sidebar: GA_IDs outside objects[] for hotlist + tab buttons */
#define AMI_SIDE_HOTLIST_MAX	12
#define AMI_SIDE_HOT_COLS	2
#define AMI_SIDE_HOT_ROWS \
	((AMI_SIDE_HOTLIST_MAX + AMI_SIDE_HOT_COLS - 1) / AMI_SIDE_HOT_COLS)
#define GID_SIDE_HOT_BASE	900
#define AMI_SIDE_TAB_MAX	8
#define GID_SIDE_TAB_BASE	920
/* Favicon cell to the left of each tab title button (same slot index). */
#define GID_SIDE_TAB_ICON_BASE	928

/* Classic Amiga prop-gadget empty-area checker (BACKGROUNDPEN × SHADOWPEN). */
static UWORD ami_nami_sidebar_checker[2] = { 0x5555, 0xAAAA };
/* LAYOUT_BackFill hook; h_Data = SIDELAYOUT Object* for domain clipping. */
static struct Hook ami_nami_sidebar_bf_hook;

struct gui_window_2 {
	struct ami_generic_window w;
	struct Window *win;
	Object *restrict objects[GID_LAST];
	struct gui_window *gw; /* currently-displayed gui_window */
	bool redraw_required;
	int throbber_frame;
	struct List tab_list;
	ULONG tabs;
	ULONG next_tab;
	struct Node *last_new_tab;
	struct Node *new_tab_tab;
	struct Hook scrollerhook;
	browser_mouse_state mouse_state;
	browser_mouse_state key_state;
	ULONG throbber_update_count;
	struct find_window *searchwin;
	ULONG oldh;
	ULONG oldv;
	int temp;
	bool redraw_scroll;
	bool redraw_interactive; /* pending paint uses short coalesce */
	bool new_content;
	struct ami_menu_data *menu_data[AMI_MENU_AREXX_MAX + 1]; /* only for GadTools menus */
	ULONG hotlist_items;
	Object *restrict hotlist_toolbar_lab[AMI_GUI_TOOLBAR_MAX];
	struct List hotlist_toolbar_list;
	struct List *web_search_list;
	Object *search_bm;
	char *restrict svbuffer;
	char *restrict status;
	char *restrict wintitle;
	char icontitle[24];
	char *restrict helphints[GID_LAST];
	browser_mouse_state prev_mouse_state;
	struct timeval lastclick;
	struct AppIcon *appicon; /* iconify appicon */
	struct DiskObject *dobj; /* iconify appicon */
	struct Hook favicon_hook;
	struct Hook throbber_hook;
	struct Hook browser_hook;
	struct Hook chrome_close_hook;
	struct Hook chrome_zoom_hook;
	struct Hook chrome_depth_hook;
	struct Hook chrome_drag_hook;
	struct Hook *ctxmenu_hook;
	Object *restrict history_ctxmenu[2];
	Object *clicktab_ctxmenu;
	gui_drag_type drag_op;
	struct IBox *ptr_lock;
	struct AppWindow *appwin;
	struct MinList *shared_pens;
	gui_pointer_shape mouse_pointer;
	struct Menu *imenu; /* Intuition menu */
	bool closed; /* Window has been closed (via menu) */
	bool ui_nami; /* Nami chrome style frozen at create */
	bool chrome_dragging;
	bool chrome_depth_back; /* last depth action sent window back */
	WORD chrome_armed; /* GID_WIN_* while LMB down, else 0 */
	WORD chrome_drag_offx;
	WORD chrome_drag_offy;
	WORD chrome_drag_orig_left; /* window LeftEdge at drag start */
	WORD chrome_drag_orig_top; /* window TopEdge at drag start */
	struct timeval chrome_drag_click; /* DoubleClick timing on drag strip */
	struct IBox chrome_zoom_rest; /* size before drag-strip zoom */
	bool chrome_zoom_have_rest; /* TRUE if chrome_zoom_rest is valid */
	WORD tb_hover_gid; /* Nami toolbar hover (0 = none) */
	WORD tb_armed_gid; /* Nami toolbar pressed (0 = none) */
	Object *throbber_led; /* led.image network blink */
	Object *throbber_boing; /* boingball.image spinner */
	WORD throbber_led_vals[1]; /* LED_Values array (1 pair) */
	/* Nami: Intuition screen-title overlay (not the window drag bar) */
	char *screentitle; /* last string passed to SetWindowTitles screen slot */
	ULONG screentitletime; /* wall-clock second of last telemetry refresh */
	ULONG status_screentime; /* when status was promoted into the bar; 0 = off */
	UBYTE nami_vprop_shift; /* PGA_ scale shift for >64k extents */
	UBYTE nami_hprop_shift;
	bool nami_border_depth; /* TRUE if GID_WIN_DEPTH is AddGList border gadget */
	/* Nami sidebar: hotlist favicon cluster + vertical tab button pool */
	bool sidebar_expanded;
	WORD sidebar_weight; /* CHILD_WeightedWidth when expanded */
	Object *sidebar_def_icon; /* default 16×16 favicon */
	Object *side_hot_btn[AMI_SIDE_HOTLIST_MAX];
	Object *side_hot_bm[AMI_SIDE_HOTLIST_MAX];
	nsurl *side_hot_url[AMI_SIDE_HOTLIST_MAX];
	Object *side_hot_row[AMI_SIDE_HOT_ROWS]; /* LayoutH rows inside HOTLAYOUT */
	int side_hot_count;
	Object *side_tab_row[AMI_SIDE_TAB_MAX]; /* LayoutH: icon | title */
	Object *side_tab_icon_btn[AMI_SIDE_TAB_MAX]; /* favicon cell (fixed width) */
	Object *side_tab_btn[AMI_SIDE_TAB_MAX]; /* title: GA_Text + BCJ_LEFT */
	struct gui_window *side_tab_gw[AMI_SIDE_TAB_MAX]; /* dense; NULL after last */
	/* gtdrag ImageNode faces for tab buttons (optional library) */
	struct ImageNode side_tab_inode[AMI_SIDE_TAB_MAX];
	bool gtdrag_registered;
};

struct gui_window
{
	struct gui_window_2 *shared;
	int tab;
	struct Node *tab_node;
	int c_x; /* Caret X posn */
	int c_y; /* Caret Y posn */
	int c_w; /* Caret width */
	int c_h; /* Caret height */
	int c_h_temp;
	int scrollx;
	int scrolly;
	struct ami_history_local_window *hw;
	struct List dllist;
	struct hlcache_handle *favicon;
	bool throbbing;
	char *tabtitle; /* full title (local charset) for hints / ARexx */
	char *tab_label; /* truncated label shown on the clicktab / sidebar */
	Object *sidebar_icon; /* Nami tab button favicon BitMapObj */
	int sidebar_tab_slot; /* pool index, or -1 */
	char *sidebar_help; /* URL string for GA_GadgetHelpText */
	APTR deferred_rects_pool;
	struct MinList *deferred_rects;
	struct browser_window *bw;
	struct ColumnInfo *logcolumns;
	struct List loglist;
};

struct ami_gui_tb_userdata {
	struct List *sblist;
	struct gui_window_2 *gw;
	int items;
};

static struct MinList *window_list = NULL;
static struct Screen *scrn = NULL;
static struct MsgPort *sport = NULL;
static struct gui_window *cur_gw = NULL;

static bool ami_quit = false;

static struct MsgPort *schedulermsgport = NULL;
static struct MsgPort *appport;
#ifdef __amigaos4__
static Class *urlStringClass;
#endif

static BOOL locked_screen = FALSE;
static int screen_signal = -1;
static bool win_destroyed;
static STRPTR nsscreentitle;
static char ami_screentitle_buf[256];
static struct gui_window_2 *ami_last_screentitle_gwin = NULL;
static struct gui_globals *browserglob = NULL;

/* Seconds status text may occupy the screen title before reverting to telemetry */
#define AMI_SCREENTITLE_STATUS_SECS 3

static struct MsgPort *applibport = NULL;
static uint32 ami_appid = 0;
static ULONG applibsig = 0;
static ULONG rxsig = 0;
static struct Hook newprefs_hook;

static STRPTR temp_homepage_url = NULL;
static char **cli_urls = NULL;
static int cli_url_count = 0;
/* -1 = use prefs ui_style; 0 = force NetSurf; 1 = force Nami (CLI/tooltype) */
static int cli_ui_style_override = -1;

#define USERS_DIR "PROGDIR:Users"
static char *users_dir = NULL;
static char *current_user_dir;
static char *current_user_faviconcache;

/* Stack for the NetSurf task. QuickJS work uses a lean context
 * (NewContextRaw + base/eval/json) so it fits without a huge cookie. */
static const char *stack_cookie = "\0$STACK:196608\0";

const char * const versvn;

static void ami_switch_tab(struct gui_window_2 *gwin, bool redraw);
static void ami_change_tab(struct gui_window_2 *gwin, int direction);
static void ami_get_hscroll_pos(struct gui_window_2 *gwin, ULONG *xs);
static void ami_get_vscroll_pos(struct gui_window_2 *gwin, ULONG *ys);
static void ami_gui_scroll_viewport(struct gui_window_2 *gwin, int xs, int ys);
static void ami_quit_netsurf_delayed(void);
static BOOL ami_clicktab_has_close(void);
static void ami_gui_relabel_all_tabs(struct gui_window_2 *gwin);
static void ami_gui_nami_fix_chrome_images(struct gui_window_2 *gwin);
static void ami_gui_nami_create_border_scrollers(struct gui_window_2 *gwin);
static void ami_gui_nami_destroy_border_scrollers(struct gui_window_2 *gwin);
static void ami_gui_nami_set_prop(struct gui_window_2 *gwin, Object *gad,
		UBYTE *shift_io, ULONG total, ULONG vis, ULONG top, BOOL set_top);
static Object *ami_gui_splash_open(void);
static void ami_gui_splash_close(Object *win_obj);
static bool ami_gui_map_filename(char **remapped, const char *restrict path,
	const char *restrict file, const char *restrict map);
static void ami_gui_window_update_box_deferred(struct gui_window *g, bool draw);
static void ami_do_redraw(struct gui_window_2 *g);
static void ami_schedule_redraw_remove(struct gui_window_2 *gwin);
static BOOL ami_handle_msg(void);
static void ami_gui_hotlist_toggle_from_url(struct gui_window_2 *gwin);
static void gui_window_set_icon(struct gui_window *g, struct hlcache_handle *icon);

static int ami_gui_paint_depth; /* >0 while inside ami_do_redraw_tiled */

/**
 * Drain pending window IDCMP via Reaction so resize/scroll/close stay live
 * during long paints. Nested paints are refused (depth>1).
 */
static void ami_gui_yield_input(void)
{
	if(ami_gui_paint_depth > 1)
		return;

	/* Single pass — do not spin if a window marks closed mid-paint */
	ami_handle_msg();
}

static bool gui_window_get_scroll(struct gui_window *g, int *restrict sx, int *restrict sy);
static nserror gui_window_set_scroll(struct gui_window *g, const struct rect *rect);
static void gui_window_remove_caret(struct gui_window *g);
static void gui_window_place_caret(struct gui_window *g, int x, int y, int height, const struct rect *clip);

HOOKF(uint32, ami_set_favicon_render_hook, APTR, space, struct gpRender *);
HOOKF(uint32, ami_set_throbber_render_hook, APTR, space, struct gpRender *);
HOOKF(uint32, ami_gui_browser_render_hook, APTR, space, struct gpRender *);
HOOKF(uint32, ami_gui_chrome_sysi_render_hook, APTR, space, struct gpRender *);
HOOKF(uint32, ami_gui_chrome_drag_render_hook, APTR, space, struct gpRender *);
static BOOL ami_gui_nami_chrome_down(struct gui_window_2 *gwin);
static BOOL ami_gui_nami_chrome_up(struct gui_window_2 *gwin, BOOL *win_closed);
static void ami_gui_nami_drag_zoom(struct gui_window_2 *gwin);
static void ami_gui_nami_sidebar_set_expanded(struct gui_window_2 *gwin, bool expanded);
static void ami_gui_nami_sidebar_toggle_cb(void *p);
static void ami_gui_nami_new_tab_cb(void *p);
static void ami_gui_nami_close_tab_cb(void *p);
static void ami_gui_nami_sidebar_sync_selection(struct gui_window_2 *gwin);
static void ami_gui_nami_sidebar_create_tab_btn(struct gui_window *g);
static void ami_gui_nami_sidebar_destroy_tab_btn(struct gui_window *g);
static void ami_gui_nami_sidebar_apply_label(struct gui_window *g);
static void ami_gui_nami_sidebar_refresh_hotlist(struct gui_window_2 *gwin);
static void ami_gui_nami_gtdrag_restore_applied(void);
static void ami_gui_set_gadget_help(Object *gad, const char *text);
static void ami_switch_tab_to(struct gui_window_2 *gwin, struct gui_window *new_gw,
		bool redraw);
char *ami_gui_get_cache_favicon_name(nsurl *url, bool only_if_avail);
bool ami_locate_resource(char *fullpath, const char *file);

/* accessors for default options - user option is updated if it is set as per default */
#define nsoption_default_set_int(OPTION, VALUE)				\
	if (nsoptions_default[NSOPTION_##OPTION].value.i == nsoptions[NSOPTION_##OPTION].value.i)	\
		nsoptions[NSOPTION_##OPTION].value.i = VALUE;	\
	nsoptions_default[NSOPTION_##OPTION].value.i = VALUE

/* Functions documented in gui.h */
struct MsgPort *ami_gui_get_shared_msgport(void)
{
	assert(sport != NULL);
	return sport;
}

struct gui_window *ami_gui_get_active_gw(void)
{
	return cur_gw;
}

struct Screen *ami_gui_get_screen(void)
{
	return scrn;
}

struct MinList *ami_gui_get_window_list(void)
{
	assert(window_list != NULL);
	return window_list;
}

void ami_gui_beep(void)
{
	DisplayBeep(scrn);
}

struct browser_window *ami_gui_get_browser_window(struct gui_window *gw)
{
	assert(gw != NULL);
	return gw->bw;
}

struct browser_window *ami_gui2_get_browser_window(struct gui_window_2 *gwin)
{
	assert(gwin != NULL);
	return ami_gui_get_browser_window(gwin->gw);
}

struct List *ami_gui_get_download_list(struct gui_window *gw)
{
	assert(gw != NULL);
	return &gw->dllist;
}

struct gui_window_2 *ami_gui_get_gui_window_2(struct gui_window *gw)
{
	assert(gw != NULL);
	return gw->shared;
}

struct gui_window *ami_gui2_get_gui_window(struct gui_window_2 *gwin)
{
	assert(gwin != NULL);
	return gwin->gw;
}

const char *ami_gui_get_win_title(struct gui_window *gw)
{
	assert(gw != NULL);
	assert(gw->shared != NULL);
	return (const char *)gw->shared->wintitle;
}

const char *ami_gui_get_tab_title(struct gui_window *gw)
{
	assert(gw != NULL);
	return (const char *)gw->tabtitle;
}

struct Node *ami_gui_get_tab_node(struct gui_window *gw)
{
	assert(gw != NULL);
	return gw->tab_node;
}

ULONG ami_gui2_get_tabs(struct gui_window_2 *gwin)
{
	assert(gwin != NULL);
	return gwin->tabs;
}

struct List *ami_gui2_get_tab_list(struct gui_window_2 *gwin)
{
	assert(gwin != NULL);
	return &gwin->tab_list;
}

struct hlcache_handle *ami_gui_get_favicon(struct gui_window *gw)
{
	assert(gw != NULL);
	return gw->favicon;
}

struct ami_history_local_window *ami_gui_get_history_window(struct gui_window *gw)
{
	assert(gw != NULL);
	return gw->hw;
}

void ami_gui_set_history_window(struct gui_window *gw, struct ami_history_local_window *hw)
{
	assert(gw != NULL);
	gw->hw = hw;
}

void ami_gui_set_find_window(struct gui_window *gw, struct find_window *fw)
{
	/* This needs to be in gui_window_2 as it is shared amongst tabs (I think),
	 * it just happens that the find code only knows of the gui_window
	 */
	assert(gw != NULL);
	assert(gw->shared != NULL);
	gw->shared->searchwin = fw;
}

bool ami_gui_get_throbbing(struct gui_window *gw)
{
	assert(gw != NULL);
	return gw->throbbing;
}

void ami_gui_set_throbbing(struct gui_window *gw, bool throbbing)
{
	assert(gw != NULL);
	gw->throbbing = throbbing;
}

int ami_gui_get_throbber_frame(struct gui_window *gw)
{
	assert(gw != NULL);
	assert(gw->shared != NULL);
	return gw->shared->throbber_frame;
}

void ami_gui_set_throbber_frame(struct gui_window *gw, int frame)
{
	assert(gw != NULL);
	assert(gw->shared != NULL);
	gw->shared->throbber_frame = frame;
}

Object *ami_gui2_get_throbber_led(struct gui_window_2 *gwin)
{
	if(gwin == NULL)
		return NULL;
	return gwin->throbber_led;
}

Object *ami_gui2_get_throbber_boing(struct gui_window_2 *gwin)
{
	if(gwin == NULL)
		return NULL;
	return gwin->throbber_boing;
}

WORD *ami_gui2_get_throbber_led_vals(struct gui_window_2 *gwin)
{
	if(gwin == NULL)
		return NULL;
	return gwin->throbber_led_vals;
}

Object *ami_gui2_get_object(struct gui_window_2 *gwin, int object_type)
{
	ULONG obj = 0;

	assert(gwin != NULL);

	switch(object_type) {
		case AMI_WIN_MAIN:
			obj = OID_MAIN;
		break;

		case AMI_GAD_THROBBER:
			obj = GID_THROBBER;
		break;

		case AMI_GAD_TABS:
			obj = GID_TABS;
		break;

		case AMI_GAD_URL:
			obj = GID_URL;
		break;

		case AMI_GAD_SEARCH:
			obj = GID_SEARCHSTRING;
		break;

		default:
			return NULL;
		break;
	}

	return gwin->objects[obj];
}


struct Window *ami_gui2_get_window(struct gui_window_2 *gwin)
{
	assert(gwin != NULL);
	return gwin->win;
}

struct Window *ami_gui_get_window(struct gui_window *gw)
{
	assert(gw != NULL);
	return ami_gui2_get_window(gw->shared);
}

struct Menu *ami_gui_get_menu(struct gui_window *gw)
{
	assert(gw != NULL);
	assert(gw->shared != NULL);
	return gw->shared->imenu;
}

void ami_gui2_set_menu(struct gui_window_2 *gwin, struct Menu *menu)
{
	if(menu != NULL) {
		gwin->imenu = menu;
	} else {
		ami_gui_menu_freemenus(gwin->imenu, gwin->menu_data);
	}
}

struct ami_menu_data **ami_gui2_get_menu_data(struct gui_window_2 *gwin)
{
	assert(gwin != NULL);
	return gwin->menu_data;
}

void ami_gui2_set_ctxmenu_history_tmp(struct gui_window_2 *gwin, int temp)
{
	assert(gwin != NULL);
	gwin->temp = temp;
}

int ami_gui2_get_ctxmenu_history_tmp(struct gui_window_2 *gwin)
{
	assert(gwin != NULL);
	return gwin->temp;
}

Object *ami_gui2_get_ctxmenu_history(struct gui_window_2 *gwin, ULONG direction)
{
	assert(gwin != NULL);
	return gwin->history_ctxmenu[direction];
}

void ami_gui2_set_ctxmenu_history(struct gui_window_2 *gwin, ULONG direction, Object *ctx_hist)
{
	assert(gwin != NULL);
	gwin->history_ctxmenu[direction] = ctx_hist;
}

void ami_gui2_set_closed(struct gui_window_2 *gwin, bool closed)
{
	assert(gwin != NULL);
	gwin->closed = closed;
}

void ami_gui2_set_new_content(struct gui_window_2 *gwin, bool new_content)
{
	assert(gwin != NULL);
	gwin->new_content = new_content;
}

/** undocumented, or internal, or documented elsewhere **/

static void *ami_find_gwin_by_id(struct Window *win, uint32 type)
{
	struct nsObject *node, *nnode;
	struct gui_window_2 *gwin;

	if(!IsMinListEmpty(window_list))
	{
		node = (struct nsObject *)GetHead((struct List *)window_list);

		do
		{
			nnode=(struct nsObject *)GetSucc((struct Node *)node);

			if(node->Type == type)
			{
				gwin = node->objstruct;
				if(win == ami_gui2_get_window(gwin)) return gwin;
			}
		} while((node = nnode));
	}
	return NULL;
}

void *ami_window_at_pointer(int type)
{
	struct Layer *layer;
	struct Screen *scrn = ami_gui_get_screen();

	LockLayerInfo(&scrn->LayerInfo);

	layer = WhichLayer(&scrn->LayerInfo, scrn->MouseX, scrn->MouseY);

	UnlockLayerInfo(&scrn->LayerInfo);

	if(layer) return ami_find_gwin_by_id(layer->Window, type);
		else return NULL;
}

void ami_set_pointer(struct gui_window_2 *gwin, gui_pointer_shape shape, bool update)
{
	if(gwin->mouse_pointer == shape) return;
	ami_update_pointer(ami_gui2_get_window(gwin), shape);
	if(update == true) gwin->mouse_pointer = shape;
}

/* reset the mouse pointer back to what NetSurf last set it as */
void ami_reset_pointer(struct gui_window_2 *gwin)
{
	ami_update_pointer(ami_gui2_get_window(gwin), gwin->mouse_pointer);
}


STRPTR ami_locale_langs(int *codeset)
{
	struct Locale *locale;
	STRPTR acceptlangs = NULL;
	char *remapped = NULL;

	if((locale = OpenLocale(NULL)))
	{
		if(codeset != NULL) *codeset = locale->loc_CodeSet;

		for(int i = 0; i < 10; i++)
		{
			if(locale->loc_PrefLanguages[i])
			{
				if(ami_gui_map_filename(&remapped, "PROGDIR:Resources",
					locale->loc_PrefLanguages[i], "LangNames"))
				{
					if(acceptlangs)
					{
						STRPTR acceptlangs2 = acceptlangs;
						acceptlangs = ASPrintf("%s, %s",acceptlangs2, remapped);
						FreeVec(acceptlangs2);
						acceptlangs2 = NULL;
					}
					else
					{
						acceptlangs = ASPrintf("%s", remapped);
					}
				}
				if(remapped != NULL) free(remapped);
			}
			else
			{
				continue;
			}
		}
		CloseLocale(locale);
	}
	return acceptlangs;
}

/* OS4 clicktab V53+ and OS3.2 V47.5+ provide per-tab close gadgets. */
static BOOL ami_clicktab_has_close(void)
{
	if(ClickTabBase->lib_Version >= 53)
		return TRUE;
	if((ClickTabBase->lib_Version == 47) &&
	   (ClickTabBase->lib_Revision >= 5))
		return TRUE;
	return FALSE;
}

/* Close the browser tab identified by a clicktab node (embedded close). */
static void ami_gui_close_clicktab_node(struct gui_window_2 *gwin,
		struct Node *tabnode)
{
	struct gui_window *closedgw;

	if((gwin == NULL) || (tabnode == NULL))
		return;
	if(tabnode == gwin->new_tab_tab)
		return;

	closedgw = NULL;
	GetClickTabNodeAttrs(tabnode,
		TNA_UserData, &closedgw,
		TAG_DONE);
	if((closedgw != NULL) && (closedgw->bw != NULL))
		browser_window_destroy(closedgw->bw);
}

/**
 * Max characters for a Nami sidebar tab label from the tab strip width.
 */
static size_t ami_gui_nami_tab_label_max_chars(struct gui_window_2 *gwin)
{
	struct Gadget *gad;
	ULONG width;
	size_t max_chars;

	max_chars = 10;
	if(gwin == NULL)
		return max_chars;

	gad = NULL;
	if(gwin->objects[GID_SIDE_TABLIST] != NULL)
		gad = (struct Gadget *)gwin->objects[GID_SIDE_TABLIST];
	else if(gwin->objects[GID_SIDELAYOUT] != NULL)
		gad = (struct Gadget *)gwin->objects[GID_SIDELAYOUT];

	width = 96;
	if(gad != NULL && gad->Width > 0)
		width = (ULONG)gad->Width;

	/* Favicon + gap + padding leave less than full width for text */
	if(width > 36)
		max_chars = (size_t)((width - 36) / 7);
	else
		max_chars = 4;
	if(max_chars < 4)
		max_chars = 4;
	if(max_chars > 32)
		max_chars = 32;
	return max_chars;
}

/**
 * Max characters for a tab label from clicktab width and tab count.
 * CLICKTAB_LabelTruncate is OS4-only, so OS3 needs explicit shortening.
 */
static size_t ami_gui_tab_label_max_chars(struct gui_window_2 *gwin)
{
	struct Gadget *gad;
	ULONG width;
	ULONG tabs;
	ULONG per;
	size_t max_chars;

	max_chars = 12;
	if((gwin == NULL) || (gwin->objects[GID_TABS] == NULL))
		return max_chars;

	gad = (struct Gadget *)gwin->objects[GID_TABS];
	width = gad->Width;
	if(width < 64)
		width = 64;

	tabs = gwin->tabs;
	if(tabs < 1)
		tabs = 1;

	/* Leave room for "+", close gadgets (right), and padding */
	per = (width > 80) ? ((width - 80) / tabs) : 12;
	max_chars = (size_t)(per / 9);
	if(max_chars < 6)
		max_chars = 6;
	if(max_chars > 16)
		max_chars = 16;
	return max_chars;
}

/** Ellipsize a UTF-8 string to at most max_chars code points (plus "..."). */
static char *ami_gui_utf8_ellipsize(const char *s, size_t max_chars)
{
	size_t len;
	size_t chars;
	size_t off;
	size_t next;
	size_t i;
	size_t keep;
	char *out;

	if(s == NULL)
		return NULL;

	chars = utf8_length(s);
	if(chars <= max_chars)
		return strdup(s);

	if(max_chars < 4)
		max_chars = 4;
	keep = max_chars - 3;
	len = strlen(s);
	off = 0;
	for(i = 0; i < keep; i++) {
		next = utf8_next(s, len, off);
		if(next <= off)
			break;
		off = next;
	}

	out = malloc(off + 4);
	if(out == NULL)
		return NULL;
	memcpy(out, s, off);
	out[off] = '.';
	out[off + 1] = '.';
	out[off + 2] = '.';
	out[off + 3] = '\0';
	return out;
}

/*
 * Nami sidebar: full-width tab buttons (favicon + title) and a HotlistToolbar
 * favicon cluster.  ClickTab nodes remain in tab_list for NetSurf/ARexx.
 */

/* Empty HintInfo terminator — enables per-gadget GA_GadgetHelpText on OS3.2 */
static struct HintInfo ami_empty_hintinfo[] = {
	{ -1, -1, NULL, 0 }
};

static void ami_gui_set_gadget_help(Object *gad, const char *text)
{
	if(gad == NULL)
		return;
	SetAttrs(gad, AMI_GA_HELP, (text != NULL) ? text : (STRPTR)"", TAG_DONE);
}

static void ami_gui_nami_sidebar_ensure_drop_targets(struct gui_window_2 *gwin);
static void ami_gui_nami_sidebar_drop_wells_refresh_all(void);
static void ami_gui_nami_gtdrag_restore_all(struct gui_window_2 *gwin);
static void ami_gui_nami_sidebar_refresh_virt(struct gui_window_2 *gwin);

/*
 * LAYOUT_BackFill / GA_BackFill (layout_gc.doc + NDK Examples/Backfill).
 *
 * Do not set LAYOUT_FillPen to BACKGROUNDPEN: bevel.image then RectFills a
 * solid that matches the window and never EraseRects, so this hook is never
 * called and nothing visible appears.  Leave FillPen at ~0 so the bevel
 * EraseRects the interior and ClassAct invokes this hook (same as the NDK
 * Backfill example: LAYOUT_BackFill + bevel, no FillPen).
 *
 * Paint msg->Bounds only — same contract as ImageBackFill.c.  Do not clip
 * to Gadget LeftEdge/Width: those can be parent-relative while Bounds are
 * layer coords, which emptied the intersection and skipped every fill.
 */
HOOKF(void, ami_gui_nami_sidebar_backfill, struct RastPort *, rp,
		struct BackFillMessage *)
{
	struct DrawInfo *dri;
	WORD apen;
	WORD bpen;
	struct Screen *s;
	struct RastPort crp;
	UWORD *old_pattern;
	UBYTE old_ptsz;

	(void)hook;

	if(rp == NULL || msg == NULL)
		return;
	if(msg->Bounds.MaxX < msg->Bounds.MinX ||
	   msg->Bounds.MaxY < msg->Bounds.MinY)
		return;

	/* NDK ImageBackFill: copy RP and drop Layer so clip does not fight Bounds. */
	crp = *rp;
	crp.Layer = NULL;

	old_pattern = crp.AreaPtrn;
	old_ptsz = crp.AreaPtSz;

	apen = 0;
	bpen = 1;
	s = scrn;
	if(s != NULL) {
		dri = GetScreenDrawInfo(s);
		if(dri != NULL) {
			apen = dri->dri_Pens[BACKGROUNDPEN];
			bpen = dri->dri_Pens[SHADOWPEN];
			FreeScreenDrawInfo(s, dri);
		}
	}

	SetABPenDrMd(&crp, apen, bpen, JAM2);
	SetAfPt(&crp, ami_nami_sidebar_checker, 1);
	RectFill(&crp,
			msg->Bounds.MinX, msg->Bounds.MinY,
			msg->Bounds.MaxX, msg->Bounds.MaxY);
	SetAfPt(&crp, old_pattern, old_ptsz);
}

/*
 * Nami tab strip: LayoutV of rows (icon Button + title Button with GA_Text).
 * button.gadget always centres RenderImage — title uses GA_Text + BCJ_LEFT
 * so short names stay left-aligned.  side_tab_gw[] stays dense in 0..n-1.
 */
static void ami_gui_nami_sidebar_tab_rethink(struct gui_window_2 *gwin)
{
	Object *side;
	Object *browser;
	ULONG panel_w;
	ULONG browser_w;

	if(gwin == NULL || gwin->win == NULL)
		return;
	/* Collapsed: SIDELAYOUT is detached — never Rethink/Refresh it or a
	 * window move (NEWSIZE/damage) will blit the old box over the browser. */
	if(gwin->sidebar_expanded == false)
		return;
	side = gwin->objects[GID_SIDELAYOUT];
	if(side == NULL)
		return;

	browser = gwin->objects[GID_BROWSERCOL];

	/* Remember weight-bar split so hide/show restores the grabber. */
	if(browser != NULL) {
		panel_w = 0;
		browser_w = 0;
		GetAttr(GA_Width, side, &panel_w);
		GetAttr(GA_Width, browser, &browser_w);
		if(panel_w >= 96 && browser_w >= 32) {
			gwin->sidebar_weight =
					(WORD)((panel_w * 100UL) / browser_w);
			if(gwin->sidebar_weight < 8)
				gwin->sidebar_weight = 8;
			if(gwin->sidebar_weight > 80)
				gwin->sidebar_weight = 80;
		}
	}

	ami_gui_nami_sidebar_ensure_drop_targets(gwin);

	FlushLayoutDomainCache((struct Gadget *)side);
	RethinkLayout((struct Gadget *)side, gwin->win, NULL, TRUE);
	RefreshGList((struct Gadget *)side, gwin->win, NULL, -1);
}

static void ami_gui_nami_sidebar_refresh_virt(struct gui_window_2 *gwin)
{
	Object *side;

	if(gwin == NULL || gwin->win == NULL)
		return;
	if(gwin->sidebar_expanded == false)
		return;
	side = gwin->objects[GID_SIDELAYOUT];
	if(side == NULL)
		return;
	RefreshGList((struct Gadget *)side, gwin->win, NULL, -1);
}

/* Second-tick sidebar refresh after tab pool / face changes settle. */
static void ami_gui_nami_sidebar_refresh_cb(void *p)
{
	struct gui_window_2 *gwin = p;

	if(gwin == NULL || gwin->win == NULL || gwin->ui_nami == false)
		return;
	ami_gui_nami_sidebar_tab_rethink(gwin);
}

static void ami_gui_nami_sidebar_tab_set_visible(struct gui_window_2 *gwin,
		int slot, BOOL visible)
{
	Object *lay;
	Object *row;
	Object *btn;
	Object *icon_btn;
	ULONG minh;
	ULONG maxh;

	if(gwin == NULL || slot < 0 || slot >= AMI_SIDE_TAB_MAX)
		return;
	lay = gwin->objects[GID_SIDE_TABLIST];
	row = gwin->side_tab_row[slot];
	btn = gwin->side_tab_btn[slot];
	icon_btn = gwin->side_tab_icon_btn[slot];
	if(lay == NULL || row == NULL || btn == NULL)
		return;

	if(!visible) {
		if(gwin->win != NULL) {
			SetGadgetAttrs((struct Gadget *)btn, gwin->win, NULL,
					GA_Text, (STRPTR)"",
					BUTTON_RenderImage, NULL,
					BUTTON_SelectImage, NULL,
					GA_Selected, FALSE,
					TAG_DONE);
			if(icon_btn != NULL)
				SetGadgetAttrs((struct Gadget *)icon_btn, gwin->win, NULL,
						BUTTON_RenderImage, NULL,
						BUTTON_SelectImage, NULL,
						GA_Selected, FALSE,
						TAG_DONE);
			if(row != NULL)
				SetGadgetAttrs((struct Gadget *)row, gwin->win, NULL,
						LAYOUT_FillPen, (ULONG)BACKGROUNDPEN,
						TAG_DONE);
		} else {
			SetAttrs(btn,
					GA_Text, (STRPTR)"",
					BUTTON_RenderImage, NULL,
					BUTTON_SelectImage, NULL,
					GA_Selected, FALSE,
					TAG_DONE);
			if(icon_btn != NULL)
				SetAttrs(icon_btn,
						BUTTON_RenderImage, NULL,
						BUTTON_SelectImage, NULL,
						GA_Selected, FALSE,
						TAG_DONE);
			if(row != NULL)
				SetAttrs(row,
						LAYOUT_FillPen, (ULONG)BACKGROUNDPEN,
						TAG_DONE);
		}
	}

	minh = visible ? 22 : 0;
	maxh = visible ? 22 : 0;

	/*
	 * Row and both cells must get a real height — CHILD_MaxHeight 0 from
	 * create-time means "zero pixels" on ClassAct and clips GA_Text away
	 * while RenderImage still spills (favicon-only look).
	 */
	if(gwin->win != NULL) {
		SetGadgetAttrs((struct Gadget *)btn, gwin->win, NULL,
				GA_Hidden, visible ? FALSE : TRUE,
				BUTTON_Transparent, FALSE,
				BUTTON_Justification, BCJ_LEFT,
				TAG_DONE);
		if(icon_btn != NULL)
			SetGadgetAttrs((struct Gadget *)icon_btn, gwin->win, NULL,
					GA_Hidden, visible ? FALSE : TRUE,
					BUTTON_Transparent, TRUE,
					TAG_DONE);
		if(icon_btn != NULL) {
			SetGadgetAttrs((struct Gadget *)row, gwin->win, NULL,
					LAYOUT_ModifyChild, icon_btn,
						CHILD_MinWidth, visible ? 20 : 0,
						CHILD_MaxWidth, visible ? 20 : 0,
						CHILD_MinHeight, minh,
						CHILD_MaxHeight, maxh,
						CHILD_WeightedWidth, 0,
						CHILD_WeightedHeight, 0,
					LAYOUT_ModifyChild, btn,
						CHILD_MinWidth, visible ? 40 : 0,
						CHILD_MinHeight, minh,
						CHILD_MaxHeight, maxh,
						CHILD_WeightedWidth, 100,
						CHILD_WeightedHeight, 0,
					TAG_DONE);
		} else {
			SetGadgetAttrs((struct Gadget *)row, gwin->win, NULL,
					LAYOUT_ModifyChild, btn,
						CHILD_MinWidth, visible ? 40 : 0,
						CHILD_MinHeight, minh,
						CHILD_MaxHeight, maxh,
						CHILD_WeightedWidth, 100,
						CHILD_WeightedHeight, 0,
					TAG_DONE);
		}
		SetGadgetAttrs((struct Gadget *)lay, gwin->win, NULL,
				LAYOUT_ModifyChild, row,
					CHILD_MinHeight, minh,
					CHILD_MaxHeight, maxh,
					CHILD_WeightedHeight, 0,
					CHILD_WeightedWidth, 100,
					CHILD_MinWidth, 1,
				TAG_DONE);
	} else {
		SetAttrs(btn,
				GA_Hidden, visible ? FALSE : TRUE,
				BUTTON_Transparent, FALSE,
				BUTTON_Justification, BCJ_LEFT,
				TAG_DONE);
		if(icon_btn != NULL)
			SetAttrs(icon_btn,
					GA_Hidden, visible ? FALSE : TRUE,
					BUTTON_Transparent, TRUE,
					TAG_DONE);
		if(icon_btn != NULL) {
			SetAttrs(row,
					LAYOUT_ModifyChild, icon_btn,
						CHILD_MinWidth, visible ? 20 : 0,
						CHILD_MaxWidth, visible ? 20 : 0,
						CHILD_MinHeight, minh,
						CHILD_MaxHeight, maxh,
						CHILD_WeightedWidth, 0,
						CHILD_WeightedHeight, 0,
					LAYOUT_ModifyChild, btn,
						CHILD_MinWidth, visible ? 40 : 0,
						CHILD_MinHeight, minh,
						CHILD_MaxHeight, maxh,
						CHILD_WeightedWidth, 100,
						CHILD_WeightedHeight, 0,
					TAG_DONE);
		} else {
			SetAttrs(row,
					LAYOUT_ModifyChild, btn,
						CHILD_MinWidth, visible ? 40 : 0,
						CHILD_MinHeight, minh,
						CHILD_MaxHeight, maxh,
						CHILD_WeightedWidth, 100,
						CHILD_WeightedHeight, 0,
					TAG_DONE);
		}
		SetAttrs(lay,
				LAYOUT_ModifyChild, row,
					CHILD_MinHeight, minh,
					CHILD_MaxHeight, maxh,
					CHILD_WeightedHeight, 0,
					CHILD_WeightedWidth, 100,
					CHILD_MinWidth, 1,
				TAG_DONE);
	}
}

static void ami_gui_nami_sidebar_tab_set_selected(struct gui_window_2 *gwin,
		int slot, BOOL selected)
{
	Object *btn;
	Object *icon_btn;
	Object *row;
	ULONG fillpen;

	if(gwin == NULL || slot < 0 || slot >= AMI_SIDE_TAB_MAX)
		return;
	btn = gwin->side_tab_btn[slot];
	icon_btn = gwin->side_tab_icon_btn[slot];
	row = gwin->side_tab_row[slot];
	if(btn == NULL)
		return;

	/*
	 * One highlight for the whole icon|title row: FILLPEN on the LayoutH,
	 * both cells transparent + not GA_Selected.  Per-cell GA_Selected drew
	 * two abutting fills with a visible seam.  LAYOUT_FillPen takes the
	 * DrawInfo pen index (FILLPEN), not dri_Pens[…].
	 */
	fillpen = selected ? (ULONG)FILLPEN : (ULONG)BACKGROUNDPEN;

	if(gwin->win != NULL) {
		if(row != NULL) {
			SetGadgetAttrs((struct Gadget *)row, gwin->win, NULL,
					LAYOUT_FillPen, fillpen,
					TAG_DONE);
		}
		SetGadgetAttrs((struct Gadget *)btn, gwin->win, NULL,
				GA_Selected, FALSE,
				BUTTON_BevelStyle, BVS_NONE,
				BUTTON_Transparent, selected ? TRUE : FALSE,
				BUTTON_Justification, BCJ_LEFT,
				TAG_DONE);
		if(icon_btn != NULL)
			SetGadgetAttrs((struct Gadget *)icon_btn, gwin->win, NULL,
					GA_Selected, FALSE,
					BUTTON_BevelStyle, BVS_NONE,
					BUTTON_Transparent, TRUE,
					TAG_DONE);
	} else {
		if(row != NULL)
			SetAttrs(row, LAYOUT_FillPen, fillpen, TAG_DONE);
		SetAttrs(btn,
				GA_Selected, FALSE,
				BUTTON_BevelStyle, BVS_NONE,
				BUTTON_Transparent, selected ? TRUE : FALSE,
				BUTTON_Justification, BCJ_LEFT,
				TAG_DONE);
		if(icon_btn != NULL)
			SetAttrs(icon_btn,
					GA_Selected, FALSE,
					BUTTON_BevelStyle, BVS_NONE,
					BUTTON_Transparent, TRUE,
					TAG_DONE);
	}
	ami_gui_nami_sidebar_refresh_virt(gwin);
}

/*
 * Favicon on the fixed-width icon cell; title via GA_Text + BCJ_LEFT on the
 * weighted title button.  button.gadget centres RenderImage, so the title
 * must not go through BUTTON_RenderImage or short names appear centred.
 */
static void ami_gui_nami_sidebar_tab_set_face(struct gui_window *g)
{
	struct gui_window_2 *gwin;
	int slot;
	Object *icon;
	Object *btn;
	Object *icon_btn;
	const char *label;
	const char *help;

	if(g == NULL || g->shared == NULL)
		return;
	gwin = g->shared;
	slot = g->sidebar_tab_slot;
	if(slot < 0 || slot >= AMI_SIDE_TAB_MAX)
		return;
	btn = gwin->side_tab_btn[slot];
	icon_btn = gwin->side_tab_icon_btn[slot];
	if(btn == NULL)
		return;

	icon = g->sidebar_icon;
	if(icon == NULL)
		icon = gwin->sidebar_def_icon;

	/* Truncation is done only in ami_gui_apply_tab_label (owned string). */
	label = g->tab_label;
	if(label == NULL)
		label = (g->tabtitle != NULL) ? g->tabtitle : messages_get("NetSurf");
	help = (g->sidebar_help != NULL) ? g->sidebar_help : label;

	if(gwin->win != NULL) {
		if(icon_btn != NULL)
			SetGadgetAttrs((struct Gadget *)icon_btn, gwin->win, NULL,
					BUTTON_RenderImage, icon,
					BUTTON_SelectImage, icon,
					BUTTON_BevelStyle, BVS_NONE,
					BUTTON_Transparent, TRUE,
					AMI_GA_HELP, help,
					TAG_DONE);
		SetGadgetAttrs((struct Gadget *)btn, gwin->win, NULL,
				BUTTON_RenderImage, NULL,
				BUTTON_SelectImage, NULL,
				GA_Text, (STRPTR)label,
				BUTTON_Justification, BCJ_LEFT,
				BUTTON_BevelStyle, BVS_NONE,
				BUTTON_Transparent, FALSE,
				AMI_GA_HELP, help,
				TAG_DONE);
		if(icon_btn != NULL)
			RefreshGList((struct Gadget *)icon_btn, gwin->win, NULL, 1);
		RefreshGList((struct Gadget *)btn, gwin->win, NULL, 1);
	} else {
		if(icon_btn != NULL)
			SetAttrs(icon_btn,
					BUTTON_RenderImage, icon,
					BUTTON_SelectImage, icon,
					BUTTON_BevelStyle, BVS_NONE,
					BUTTON_Transparent, TRUE,
					AMI_GA_HELP, help,
					TAG_DONE);
		SetAttrs(btn,
				BUTTON_RenderImage, NULL,
				BUTTON_SelectImage, NULL,
				GA_Text, (STRPTR)label,
				BUTTON_Justification, BCJ_LEFT,
				BUTTON_BevelStyle, BVS_NONE,
				BUTTON_Transparent, FALSE,
				AMI_GA_HELP, help,
				TAG_DONE);
	}
}

/*
 * Pack side_tab_gw[] to 0..n-1 and rebuild every button face.  Clearing all
 * faces first stops a reorder/close from leaving a stale title/icon ghost.
 */
static void ami_gui_nami_sidebar_compact_tabs(struct gui_window_2 *gwin)
{
	struct gui_window *ordered[AMI_SIDE_TAB_MAX];
	Object *btn;
	Object *icon_btn;
	int n;
	int i;

	if(gwin == NULL || gwin->ui_nami == false)
		return;

	n = 0;
	for(i = 0; i < AMI_SIDE_TAB_MAX; i++) {
		if(gwin->side_tab_gw[i] != NULL)
			ordered[n++] = gwin->side_tab_gw[i];
	}

	for(i = 0; i < AMI_SIDE_TAB_MAX; i++) {
		btn = gwin->side_tab_btn[i];
		icon_btn = gwin->side_tab_icon_btn[i];
		gwin->side_tab_gw[i] = NULL;
		if(btn != NULL) {
			if(gwin->win != NULL) {
				SetGadgetAttrs((struct Gadget *)btn, gwin->win, NULL,
						GA_Text, (STRPTR)"",
						BUTTON_RenderImage, NULL,
						BUTTON_SelectImage, NULL,
						GA_Selected, FALSE,
						BUTTON_BevelStyle, BVS_NONE,
						TAG_DONE);
			} else {
				SetAttrs(btn,
						GA_Text, (STRPTR)"",
						BUTTON_RenderImage, NULL,
						BUTTON_SelectImage, NULL,
						GA_Selected, FALSE,
						BUTTON_BevelStyle, BVS_NONE,
						TAG_DONE);
			}
		}
		if(icon_btn != NULL) {
			if(gwin->win != NULL) {
				SetGadgetAttrs((struct Gadget *)icon_btn, gwin->win, NULL,
						BUTTON_RenderImage, NULL,
						BUTTON_SelectImage, NULL,
						GA_Selected, FALSE,
						TAG_DONE);
			} else {
				SetAttrs(icon_btn,
						BUTTON_RenderImage, NULL,
						BUTTON_SelectImage, NULL,
						GA_Selected, FALSE,
						TAG_DONE);
			}
		}
	}

	for(i = 0; i < n; i++) {
		ordered[i]->sidebar_tab_slot = i;
		gwin->side_tab_gw[i] = ordered[i];
		ami_gui_nami_sidebar_tab_set_face(ordered[i]);
		ami_gui_nami_sidebar_tab_set_visible(gwin, i, TRUE);
	}
	for(i = n; i < AMI_SIDE_TAB_MAX; i++)
		ami_gui_nami_sidebar_tab_set_visible(gwin, i, FALSE);

	ami_gui_nami_sidebar_sync_selection(gwin);
}

static void ami_gui_nami_sidebar_destroy_tab_btn(struct gui_window *g)
{
	struct gui_window_2 *gwin;
	int slot;
	Object *btn;
	Object *icon_btn;

	if(g == NULL || g->shared == NULL)
		return;
	gwin = g->shared;
	slot = g->sidebar_tab_slot;
	if(slot < 0 || slot >= AMI_SIDE_TAB_MAX)
		return;

	btn = gwin->side_tab_btn[slot];
	icon_btn = gwin->side_tab_icon_btn[slot];
	gwin->side_tab_gw[slot] = NULL;
	g->sidebar_tab_slot = -1;

	if(btn != NULL) {
		if(gwin->win != NULL) {
			SetGadgetAttrs((struct Gadget *)btn, gwin->win, NULL,
					GA_Text, (STRPTR)"",
					BUTTON_RenderImage, NULL,
					BUTTON_SelectImage, NULL,
					GA_Selected, FALSE,
					BUTTON_BevelStyle, BVS_NONE,
					TAG_DONE);
		} else {
			SetAttrs(btn,
					GA_Text, (STRPTR)"",
					BUTTON_RenderImage, NULL,
					BUTTON_SelectImage, NULL,
					GA_Selected, FALSE,
					BUTTON_BevelStyle, BVS_NONE,
					TAG_DONE);
		}
	}
	if(icon_btn != NULL) {
		if(gwin->win != NULL) {
			SetGadgetAttrs((struct Gadget *)icon_btn, gwin->win, NULL,
					BUTTON_RenderImage, NULL,
					BUTTON_SelectImage, NULL,
					GA_Selected, FALSE,
					TAG_DONE);
		} else {
			SetAttrs(icon_btn,
					BUTTON_RenderImage, NULL,
					BUTTON_SelectImage, NULL,
					GA_Selected, FALSE,
					TAG_DONE);
		}
	}

	/* Close must not leave a hole — remaining tabs pack upward. */
	ami_gui_nami_sidebar_compact_tabs(gwin);
	ami_gui_nami_sidebar_tab_rethink(gwin);
	ami_schedule(0, ami_gui_nami_sidebar_refresh_cb, gwin);

	if(g->sidebar_icon != NULL) {
		DisposeObject(g->sidebar_icon);
		g->sidebar_icon = NULL;
	}
	if(g->sidebar_help != NULL) {
		free(g->sidebar_help);
		g->sidebar_help = NULL;
	}
}

static void ami_gui_nami_sidebar_apply_label(struct gui_window *g)
{
	struct gui_window_2 *gwin;
	BOOL selected;

	if(g == NULL || g->shared == NULL)
		return;
	if(g->sidebar_tab_slot < 0)
		return;
	gwin = g->shared;

	ami_gui_nami_sidebar_tab_set_face(g);
	selected = (gwin->gw == g) ? TRUE : FALSE;
	ami_gui_nami_sidebar_tab_set_selected(gwin, g->sidebar_tab_slot, selected);
}

static void ami_gui_nami_sidebar_create_tab_btn(struct gui_window *g)
{
	struct gui_window_2 *gwin;
	int slot;
	int i;

	if(g == NULL || g->shared == NULL || g->shared->ui_nami == false)
		return;
	gwin = g->shared;
	if(gwin->objects[GID_SIDE_TABLIST] == NULL)
		return;

	if(g->sidebar_tab_slot >= 0)
		ami_gui_nami_sidebar_destroy_tab_btn(g);

	slot = -1;
	for(i = 0; i < AMI_SIDE_TAB_MAX; i++) {
		if(gwin->side_tab_gw[i] == NULL &&
		   gwin->side_tab_btn[i] != NULL) {
			slot = i;
			break;
		}
	}
	if(slot < 0)
		return;

	g->sidebar_tab_slot = slot;
	gwin->side_tab_gw[slot] = g;

	ami_gui_nami_sidebar_tab_set_face(g);
	ami_gui_nami_sidebar_tab_set_selected(gwin, slot,
			(gwin->gw == g) ? TRUE : FALSE);
	ami_gui_nami_sidebar_tab_set_visible(gwin, slot, TRUE);
	ami_gui_nami_sidebar_tab_rethink(gwin);
	ami_schedule(0, ami_gui_nami_sidebar_refresh_cb, gwin);
}

static void ami_gui_nami_sidebar_sync_selection(struct gui_window_2 *gwin)
{
	int i;
	BOOL selected;

	if(gwin == NULL || gwin->ui_nami == false)
		return;

	for(i = 0; i < AMI_SIDE_TAB_MAX; i++) {
		if(gwin->side_tab_gw[i] == NULL)
			continue;
		selected = (gwin->side_tab_gw[i] == gwin->gw) ? TRUE : FALSE;
		ami_gui_nami_sidebar_tab_set_selected(gwin, i, selected);
	}
}

static void ami_gui_nami_sidebar_hot_set_visible(struct gui_window_2 *gwin,
		int slot, BOOL visible)
{
	Object *row;
	Object *btn;
	ULONG minw;
	ULONG maxw;
	int row_i;

	if(gwin == NULL || slot < 0 || slot >= AMI_SIDE_HOTLIST_MAX)
		return;
	row_i = slot / AMI_SIDE_HOT_COLS;
	if(row_i < 0 || row_i >= AMI_SIDE_HOT_ROWS)
		return;
	row = gwin->side_hot_row[row_i];
	btn = gwin->side_hot_btn[slot];
	if(row == NULL || btn == NULL)
		return;

	minw = visible ? 22 : 0;
	maxw = visible ? 22 : 0;

	if(gwin->win != NULL) {
		SetGadgetAttrs((struct Gadget *)btn, gwin->win, NULL,
				GA_Hidden, visible ? FALSE : TRUE,
				TAG_DONE);
		SetGadgetAttrs((struct Gadget *)row, gwin->win, NULL,
				LAYOUT_ModifyChild, btn,
					CHILD_MinWidth, minw,
					CHILD_MaxWidth, maxw,
					CHILD_MinHeight, visible ? 22 : 0,
					CHILD_MaxHeight, visible ? 22 : 0,
					CHILD_WeightedWidth, 0,
					CHILD_WeightedHeight, 0,
				TAG_DONE);
	} else {
		SetAttrs(btn, GA_Hidden, visible ? FALSE : TRUE, TAG_DONE);
		SetAttrs(row,
				LAYOUT_ModifyChild, btn,
					CHILD_MinWidth, minw,
					CHILD_MaxWidth, maxw,
					CHILD_MinHeight, visible ? 22 : 0,
					CHILD_MaxHeight, visible ? 22 : 0,
					CHILD_WeightedWidth, 0,
					CHILD_WeightedHeight, 0,
				TAG_DONE);
	}
}

/* Collapse unused favicon rows so the shelf cannot overlap the tab list. */
static void ami_gui_nami_sidebar_hot_rows_rethink(struct gui_window_2 *gwin,
		int drop_slot)
{
	Object *lay;
	Object *row;
	Object *side;
	int ri;
	int ci;
	int slot;
	BOOL row_on;
	ULONG minh;
	ULONG maxh;
	int open_rows;

	if(gwin == NULL)
		return;
	lay = gwin->objects[GID_SIDE_HOTLAYOUT];
	if(lay == NULL)
		return;

	open_rows = 0;
	for(ri = 0; ri < AMI_SIDE_HOT_ROWS; ri++) {
		row = gwin->side_hot_row[ri];
		if(row == NULL)
			continue;
		row_on = FALSE;
		for(ci = 0; ci < AMI_SIDE_HOT_COLS; ci++) {
			slot = ri * AMI_SIDE_HOT_COLS + ci;
			if(slot >= AMI_SIDE_HOTLIST_MAX)
				break;
			if(gwin->side_hot_url[slot] != NULL || slot == drop_slot)
				row_on = TRUE;
		}
		minh = row_on ? 22 : 0;
		maxh = row_on ? 22 : 0;
		if(row_on)
			open_rows++;
		if(gwin->win != NULL) {
			SetGadgetAttrs((struct Gadget *)lay, gwin->win, NULL,
					LAYOUT_ModifyChild, row,
						CHILD_MinHeight, minh,
						CHILD_MaxHeight, maxh,
						CHILD_WeightedHeight, 0,
						CHILD_WeightedWidth, 100,
					TAG_DONE);
		} else {
			SetAttrs(lay,
					LAYOUT_ModifyChild, row,
						CHILD_MinHeight, minh,
						CHILD_MaxHeight, maxh,
						CHILD_WeightedHeight, 0,
						CHILD_WeightedWidth, 100,
					TAG_DONE);
		}
	}

	/* Cap HOTLAYOUT height to open rows so it cannot paint over tabs. */
	side = gwin->objects[GID_SIDELAYOUT];
	if(side != NULL) {
		minh = (open_rows > 0) ? (ULONG)(open_rows * 26) : 0;
		maxh = minh;
		if(gwin->win != NULL) {
			SetGadgetAttrs((struct Gadget *)side, gwin->win, NULL,
					LAYOUT_ModifyChild, lay,
						CHILD_MinHeight, minh,
						CHILD_MaxHeight, maxh,
						CHILD_WeightedHeight, 0,
						CHILD_WeightedWidth, 100,
					TAG_DONE);
		} else {
			SetAttrs(side,
					LAYOUT_ModifyChild, lay,
						CHILD_MinHeight, minh,
						CHILD_MaxHeight, maxh,
						CHILD_WeightedHeight, 0,
						CHILD_WeightedWidth, 100,
					TAG_DONE);
		}
	}
}

static void ami_gui_nami_sidebar_clear_hotlist(struct gui_window_2 *gwin)
{
	int i;

	if(gwin == NULL)
		return;

	/* Pool stays in HOTLAYOUT — only clear faces/URLs and hide slots.
	 * Avoid LAYOUT_AddChild/RemoveChild at runtime (locks layout.gadget). */
	for(i = 0; i < AMI_SIDE_HOTLIST_MAX; i++) {
		if(gwin->side_hot_btn[i] != NULL) {
			if(gwin->win != NULL) {
				SetGadgetAttrs((struct Gadget *)gwin->side_hot_btn[i],
						gwin->win, NULL,
						BUTTON_RenderImage, NULL,
						GA_Selected, FALSE,
						AMI_GA_HELP, "",
						TAG_DONE);
			} else {
				SetAttrs(gwin->side_hot_btn[i],
						BUTTON_RenderImage, NULL,
						GA_Selected, FALSE,
						AMI_GA_HELP, "",
						TAG_DONE);
			}
			ami_gui_nami_sidebar_hot_set_visible(gwin, i, FALSE);
		}
		if(gwin->side_hot_bm[i] != NULL) {
			DisposeObject(gwin->side_hot_bm[i]);
			gwin->side_hot_bm[i] = NULL;
		}
		if(gwin->side_hot_url[i] != NULL) {
			nsurl_unref(gwin->side_hot_url[i]);
			gwin->side_hot_url[i] = NULL;
		}
	}
	gwin->side_hot_count = 0;
}

struct ami_side_hot_ctx {
	struct gui_window_2 *gwin;
	int items;
};

static bool ami_gui_nami_side_hot_add(void *userdata, int level, int item,
		const char *title, nsurl *url, bool is_folder)
{
	struct ami_side_hot_ctx *ctx = (struct ami_side_hot_ctx *)userdata;
	struct gui_window_2 *gwin;
	char menu_icon[1024];
	char *iconname;
	Object *bm;
	Object *btn;
	const char *help;
	int idx;

	(void)item;
	(void)title;

	if(ctx == NULL || ctx->gwin == NULL)
		return false;
	gwin = ctx->gwin;

	if(level != 1)
		return false;
	if(is_folder == true)
		return false;
	if(ctx->items >= AMI_SIDE_HOTLIST_MAX)
		return false;
	if(url == NULL)
		return false;

	idx = ctx->items;
	btn = gwin->side_hot_btn[idx];
	if(btn == NULL)
		return false;

	iconname = ami_gui_get_cache_favicon_name(url, true);
	if(iconname == NULL)
		iconname = ASPrintf("icons/content.png");
	ami_locate_resource(menu_icon, iconname);

	bm = BitMapObj,
			BITMAP_SourceFile, menu_icon,
			BITMAP_Screen, scrn,
			BITMAP_Masking, TRUE,
			BITMAP_Width, 16,
			BITMAP_Height, 16,
		BitMapEnd;
	if(bm == NULL)
		return true;

	help = nsurl_access(url);
	nsurl_ref(url);
	gwin->side_hot_bm[idx] = bm;
	gwin->side_hot_url[idx] = url;

	if(gwin->win != NULL) {
		SetGadgetAttrs((struct Gadget *)btn, gwin->win, NULL,
				BUTTON_RenderImage, bm,
				BUTTON_Transparent, FALSE,
				AMI_GA_HELP, (help != NULL) ? help : "",
				TAG_DONE);
	} else {
		SetAttrs(btn,
				BUTTON_RenderImage, bm,
				BUTTON_Transparent, FALSE,
				AMI_GA_HELP, (help != NULL) ? help : "",
				TAG_DONE);
	}
	ami_gui_nami_sidebar_hot_set_visible(gwin, idx, TRUE);

	ctx->items++;
	gwin->side_hot_count = ctx->items;
	return true;
}

/*
 * Open a hotlist URL: no-op if already the current tab; switch if another
 * open tab has the same URL; otherwise navigate the current tab.
 */
static struct gui_window *ami_gui_nami_find_tab_by_url(struct gui_window_2 *gwin,
		nsurl *url)
{
	struct Node *node;
	struct gui_window *gw;
	nsurl *tab_url;

	if(gwin == NULL || url == NULL)
		return NULL;

	node = GetHead(&gwin->tab_list);
	while(node != NULL) {
		if(node != gwin->new_tab_tab) {
			gw = NULL;
			GetClickTabNodeAttrs(node, TNA_UserData, &gw, TAG_DONE);
			if(gw != NULL && gw->bw != NULL) {
				tab_url = NULL;
				if(browser_window_get_url(gw->bw, false, &tab_url) ==
						NSERROR_OK && tab_url != NULL) {
					if(nsurl_compare(url, tab_url, NSURL_COMPLETE)) {
						nsurl_unref(tab_url);
						return gw;
					}
					nsurl_unref(tab_url);
				}
			}
		}
		node = GetSucc(node);
	}
	return NULL;
}

static void ami_gui_nami_hotlist_open(struct gui_window_2 *gwin, nsurl *url)
{
	struct gui_window *match;
	nsurl *cur;

	if(gwin == NULL || url == NULL)
		return;
	if(gwin->gw == NULL || gwin->gw->bw == NULL)
		return;

	cur = NULL;
	if(browser_window_get_url(gwin->gw->bw, false, &cur) == NSERROR_OK &&
	   cur != NULL) {
		if(nsurl_compare(url, cur, NSURL_COMPLETE)) {
			nsurl_unref(cur);
			return;
		}
		nsurl_unref(cur);
	}

	match = ami_gui_nami_find_tab_by_url(gwin, url);
	if(match != NULL) {
		ami_switch_tab_to(gwin, match, true);
		return;
	}

	browser_window_navigate(gwin->gw->bw,
			url,
			NULL,
			BW_NAVIGATE_HISTORY,
			NULL,
			NULL,
			NULL);
}

static void ami_gui_nami_sidebar_refresh_hotlist(struct gui_window_2 *gwin)
{
	struct ami_side_hot_ctx ctx;

	if(gwin == NULL || gwin->ui_nami == false)
		return;
	if(gwin->objects[GID_SIDE_HOTLAYOUT] == NULL)
		return;

	ami_gui_nami_sidebar_clear_hotlist(gwin);

	ctx.gwin = gwin;
	ctx.items = 0;
	ami_hotlist_scan((void *)&ctx, 0, messages_get("HotlistToolbar"),
			ami_gui_nami_side_hot_add);

	if(gwin->win == NULL)
		return;

	ami_gui_nami_sidebar_tab_rethink(gwin);
}

/*
 * Drop destinations: filled tab buttons (reorder / cross-window move) and the
 * hotlist strip (bookmark).  Never reveal an empty favicon well — that paints
 * as a spurious/extra hotlist row.  Bookmark drops hit HOTLAYOUT instead.
 */
static void ami_gui_nami_sidebar_ensure_drop_targets(struct gui_window_2 *gwin)
{
	int i;
	int first_empty_hot;
	struct Gadget *gad;

	if(gwin == NULL || gwin->ui_nami == false)
		return;

	first_empty_hot = -1;
	for(i = 0; i < AMI_SIDE_HOTLIST_MAX; i++) {
		if(gwin->side_hot_url[i] == NULL && first_empty_hot < 0)
			first_empty_hot = i;
	}

	for(i = 0; i < AMI_SIDE_TAB_MAX; i++) {
		gad = (struct Gadget *)gwin->side_tab_btn[i];
		if(gad == NULL)
			continue;
		if(gwin->side_tab_gw[i] != NULL) {
			ami_gtdrag_set_accept(gad, AMI_GTD_TAB);
			if(gwin->side_tab_icon_btn[i] != NULL)
				ami_gtdrag_set_accept(
						(struct Gadget *)gwin->side_tab_icon_btn[i],
						AMI_GTD_TAB);
			ami_gui_nami_sidebar_tab_set_visible(gwin, i, TRUE);
		} else {
			ami_gtdrag_set_accept(gad, 0UL);
			if(gwin->side_tab_icon_btn[i] != NULL)
				ami_gtdrag_set_accept(
						(struct Gadget *)gwin->side_tab_icon_btn[i],
						0UL);
			ami_gui_nami_sidebar_tab_set_visible(gwin, i, FALSE);
		}
	}

	for(i = 0; i < AMI_SIDE_HOTLIST_MAX; i++) {
		gad = (struct Gadget *)gwin->side_hot_btn[i];
		if(gad == NULL)
			continue;
		if(gwin->side_hot_url[i] != NULL) {
			ami_gtdrag_set_accept(gad, 0UL);
			continue;
		}
		/* Keep empty wells hidden — do not open a blank favicon cell. */
		ami_gui_nami_sidebar_hot_set_visible(gwin, i, FALSE);
		ami_gtdrag_set_accept(gad, 0UL);
	}

	if(gwin->objects[GID_SIDE_HOTLAYOUT] != NULL) {
		ami_gtdrag_set_accept(
				(struct Gadget *)gwin->objects[GID_SIDE_HOTLAYOUT],
				(first_empty_hot >= 0) ? AMI_GTD_TAB : 0UL);
	}

	ami_gui_nami_sidebar_hot_rows_rethink(gwin, -1);
}

/* After drag end: restore abs boxes then repaint so gtdrag ghosts cannot linger. */
static void ami_gui_nami_sidebar_drop_wells_refresh_all(void)
{
	struct nsObject *node;
	struct nsObject *nnode;
	struct gui_window_2 *gwin;
	Object *side;
	Object *body;

	if(window_list == NULL || IsMinListEmpty(window_list))
		return;

	node = (struct nsObject *)GetHead((struct List *)window_list);
	while(node != NULL) {
		nnode = (struct nsObject *)GetSucc((struct Node *)node);
		if(node->Type == AMINS_WINDOW) {
			gwin = node->objstruct;
			if(gwin != NULL && gwin->ui_nami && gwin->win != NULL) {
				ami_gui_nami_gtdrag_restore_all(gwin);
				ami_gui_nami_sidebar_tab_rethink(gwin);
				side = gwin->objects[GID_SIDELAYOUT];
				body = gwin->objects[GID_BODYLAYOUT];
				if(side != NULL)
					RefreshGList((struct Gadget *)side,
							gwin->win, NULL, -1);
				if(body != NULL)
					RefreshGList((struct Gadget *)body,
							gwin->win, NULL, -1);
			}
		}
		node = nnode;
	}
}

/*
 * gtdrag: map a gadget to a tab slot in this window (-1 if not a tab).
 */
static int ami_gui_nami_gtdrag_tab_slot(struct gui_window_2 *gwin,
		struct Gadget *gad)
{
	int i;

	if(gwin == NULL || gad == NULL)
		return -1;
	for(i = 0; i < AMI_SIDE_TAB_MAX; i++) {
		if(gwin->side_tab_btn[i] == (Object *)gad)
			return i;
		if(gwin->side_tab_icon_btn[i] == (Object *)gad)
			return i;
	}
	return -1;
}

static bool ami_gui_nami_gtdrag_is_hot(struct gui_window_2 *gwin,
		struct Gadget *gad)
{
	int i;

	if(gwin == NULL || gad == NULL)
		return false;
	if(gwin->objects[GID_SIDE_HOTLAYOUT] == (Object *)gad)
		return true;
	for(i = 0; i < AMI_SIDE_HOTLIST_MAX; i++) {
		if(gwin->side_hot_btn[i] == (Object *)gad)
			return true;
	}
	return false;
}

/*
 * Cached hit boxes for sidebar gadgets.  apply/restore around FilterIMsg
 * only — never leave abs coords in Gadget LeftEdge permanently.
 */
struct ami_gtd_box {
	WORD abs_l, abs_t, abs_w, abs_h;
	WORD save_l, save_t, save_w, save_h;
	bool applied;
	bool valid;
};

static struct ami_gtd_box ami_gtd_tab_box[AMI_SIDE_TAB_MAX];
static struct ami_gtd_box ami_gtd_hot_box[AMI_SIDE_HOTLIST_MAX];
static struct ami_gtd_box ami_gtd_hotlay_box;
/* Only one window may hold absolute gadget coords at a time. */
static struct gui_window_2 *ami_gtd_abs_owner = NULL;

static bool ami_gui_nami_gtdrag_box_hit_cache(const struct ami_gtd_box *box,
		int mx, int my);
static void ami_gui_nami_gtdrag_apply_abs(struct gui_window_2 *gwin);
static void ami_gui_nami_gtdrag_restore_all(struct gui_window_2 *gwin);
static void ami_gui_nami_gtdrag_restore_applied(void);

/* Refresh ImageNode text before gtdrag starts the ghost.
 * Do not put BitMapObj pointers in in_Image — RenderHook calls DrawImage
 * on a classic struct Image and will crash on BOOPSI objects.
 */
static ASM void ami_gui_nami_gtdrag_tab_objfunc(
		REG(a0, struct Window *win),
		REG(a1, struct Gadget *gad),
		REG(a2, struct ObjectDescription *od),
		REG(d0, LONG pos))
{
	struct gui_window_2 *gwin;
	struct gui_window *gw;
	struct Gadget *gg;
	int slot;
	const char *label;

	(void)pos;

	gwin = ami_find_gwin_by_id(win, AMINS_WINDOW);
	if(gwin == NULL || gwin->ui_nami == false)
		return;
	slot = ami_gui_nami_gtdrag_tab_slot(gwin, gad);
	if(slot < 0)
		return;
	gw = gwin->side_tab_gw[slot];
	if(gw == NULL)
		return;

	label = gw->tab_label;
	if(label == NULL)
		label = (gw->tabtitle != NULL) ? gw->tabtitle : messages_get("NetSurf");

	memset(&gwin->side_tab_inode[slot], 0, sizeof(struct ImageNode));
	gwin->side_tab_inode[slot].in_Name = (STRPTR)label;
	/* in_Image left NULL — text-only ghost (see comment above) */

	gg = (struct Gadget *)gwin->side_tab_btn[slot];
	ami_gtdrag_set_tab_object(gg, &gwin->side_tab_inode[slot],
			(UWORD)(gg->Width > 0 ? gg->Width : 96),
			(UWORD)(gg->Height > 0 ? gg->Height : 22));

	if(od != NULL) {
		od->od_Object = &gwin->side_tab_inode[slot];
		od->od_Type = ODT_IMAGENODE;
		od->od_InternalType = AMI_GTD_TAB;
	}
}

static void ami_gui_nami_gtdrag_reorder_tabs(struct gui_window_2 *gwin,
		int from, int insert_before);
static void ami_gui_nami_gtdrag_move_to_window(
		struct gui_window_2 *dst_gwin, struct gui_window *src_gw);
static void ami_gui_nami_gtdrag_tearoff_window(struct gui_window *src_gw);
static void ami_gui_nami_gtdrag_pending_cb(void *p);

/* Deferred cross-window / reorder work — must not run inside IDCMP/Forbid. */
static struct gui_window_2 *ami_gtd_pend_dst = NULL;
static struct gui_window *ami_gtd_pend_src_gw = NULL;
static struct gui_window_2 *ami_gtd_pend_swap_win = NULL;
static int ami_gtd_pend_swap_a = -1; /* from slot */
static int ami_gtd_pend_swap_b = -1; /* insert-before index */
/* Destroy source after dest has created+painted (avoids layers lockup). */
static struct gui_window *ami_gtd_pend_destroy_gw = NULL;

static void ami_gui_nami_gtdrag_cancel_pending(void)
{
	ami_schedule(-1, ami_gui_nami_gtdrag_pending_cb, NULL);
	ami_gtd_pend_dst = NULL;
	ami_gtd_pend_src_gw = NULL;
	ami_gtd_pend_swap_win = NULL;
	ami_gtd_pend_swap_a = -1;
	ami_gtd_pend_swap_b = -1;
	ami_gtd_pend_destroy_gw = NULL;
}

static void ami_gui_nami_gtdrag_refresh_window(struct gui_window_2 *gwin);
static void ami_gui_nami_gtdrag_unlock_poll_cb(void *p);

static void ami_gui_nami_gtdrag_refresh_cb(void *p)
{
	struct gui_window_2 *gwin = p;

	if(gwin == NULL || gwin->win == NULL || gwin->ui_nami == false)
		return;
	ami_gui_nami_gtdrag_refresh_window(gwin);
}

/*
 * CreateDragObj LockLayers can leave us with no SELECTUP IDCMP (especially
 * after cross-window moves).  Poll hardware LMB on the timer so FreeDragObj
 * still runs.
 */
static void ami_gui_nami_gtdrag_unlock_poll_cb(void *p)
{
	struct Window *arm_win;
	struct gui_window_2 *arm_gwin;

	(void)p;

	if(!ami_gtdrag_available())
		return;

	if(ami_gtdrag_armed()) {
		if(ami_gtdrag_lmb_physically_down()) {
			ami_schedule(50, ami_gui_nami_gtdrag_unlock_poll_cb, NULL);
			return;
		}
		arm_win = ami_gtdrag_arm_window();
		arm_gwin = (arm_win != NULL) ?
				ami_find_gwin_by_id(arm_win, AMINS_WINDOW) : NULL;
		if(arm_gwin != NULL)
			ami_gui_nami_gtdrag_apply_abs(arm_gwin);
		(void)ami_gtdrag_select_up(arm_win);
		ami_gui_nami_gtdrag_restore_applied();
		ami_gtdrag_discard_drops();
		ami_gui_nami_sidebar_drop_wells_refresh_all();
		NSLOG(netsurf, INFO,
				"gtdrag: poll UnlockLayers; await real release");
	}

	if(ami_gtdrag_should_poll_drop(0)) {
		ami_gui_nami_gtdrag_restore_applied();
		ami_gui_nami_gtdrag_finish_drop();
		ami_gtdrag_drop_polled();
	} else if(ami_gtdrag_drop_waiting()) {
		ami_schedule(50, ami_gui_nami_gtdrag_unlock_poll_cb, NULL);
	}
}

static void ami_gui_nami_gtdrag_refresh_window(struct gui_window_2 *gwin)
{
	Object *side;

	if(gwin == NULL || gwin->win == NULL)
		return;
	ami_gui_nami_gtdrag_restore_applied();
	ami_gui_nami_sidebar_tab_rethink(gwin);
	ami_gui_nami_sidebar_sync_selection(gwin);

	side = gwin->objects[GID_SIDELAYOUT];
	if(side != NULL)
		RefreshGList((struct Gadget *)side, gwin->win, NULL, -1);

	ami_schedule_redraw(gwin, true);
}

static void ami_gui_nami_gtdrag_pending_cb(void *p)
{
	struct gui_window_2 *dst;
	struct gui_window *src_gw;
	struct gui_window_2 *swap_win;
	struct gui_window *destroy_gw;
	struct gui_window_2 *src_shared;
	int a, b;

	(void)p;

	destroy_gw = ami_gtd_pend_destroy_gw;
	if(destroy_gw != NULL) {
		ami_gtd_pend_destroy_gw = NULL;
		NSLOG(netsurf, INFO, "gtdrag: deferred destroy source tab");
		src_shared = destroy_gw->shared;
		a = (src_shared != NULL) ? src_shared->tabs : 0;
		if(destroy_gw->bw != NULL)
			browser_window_destroy(destroy_gw->bw);
		/* Last tab destroys the whole shared window — do not touch it. */
		if(a > 1 && src_shared != NULL && src_shared->win != NULL) {
			ami_gui_nami_gtdrag_refresh_window(src_shared);
			ami_schedule(0, ami_gui_nami_gtdrag_refresh_cb, src_shared);
		}
		return;
	}

	dst = ami_gtd_pend_dst;
	src_gw = ami_gtd_pend_src_gw;
	swap_win = ami_gtd_pend_swap_win;
	a = ami_gtd_pend_swap_a;
	b = ami_gtd_pend_swap_b;

	ami_gtd_pend_dst = NULL;
	ami_gtd_pend_src_gw = NULL;
	ami_gtd_pend_swap_win = NULL;
	ami_gtd_pend_swap_a = -1;
	ami_gtd_pend_swap_b = -1;

	ami_gui_nami_gtdrag_restore_applied();

	if(swap_win != NULL && a >= 0 && b >= 0) {
		NSLOG(netsurf, INFO, "gtdrag: deferred reorder %d → insert %d", a, b);
		ami_gui_nami_gtdrag_reorder_tabs(swap_win, a, b);
		ami_gui_nami_gtdrag_refresh_window(swap_win);
		ami_schedule(0, ami_gui_nami_gtdrag_refresh_cb, swap_win);
		return;
	}

	if(dst != NULL && src_gw != NULL && src_gw->bw != NULL &&
	   dst->gw != NULL && dst->gw->bw != NULL) {
		NSLOG(netsurf, INFO, "gtdrag: deferred move to other window");
		ami_gui_nami_gtdrag_move_to_window(dst, src_gw);
	}
}

static void ami_gui_nami_gtdrag_schedule_move(struct gui_window_2 *dst,
		struct gui_window *src_gw)
{
	ami_gui_nami_gtdrag_cancel_pending();
	ami_gtd_pend_dst = dst;
	ami_gtd_pend_src_gw = src_gw;
	ami_schedule(0, ami_gui_nami_gtdrag_pending_cb, NULL);
}

static void ami_gui_nami_gtdrag_schedule_reorder(struct gui_window_2 *gwin,
		int from, int insert_before)
{
	ami_gui_nami_gtdrag_cancel_pending();
	ami_gtd_pend_swap_win = gwin;
	ami_gtd_pend_swap_a = from;
	ami_gtd_pend_swap_b = insert_before;
	ami_schedule(0, ami_gui_nami_gtdrag_pending_cb, NULL);
}

/*
 * Same-window tab reorder: remove `from`, insert before `insert_before`
 * (0..n, where n = append).  No-op when the tab would not move.  Faces are
 * rebuilt via compact so the vacated slot cannot keep a ghost Label.
 */
static void ami_gui_nami_gtdrag_reorder_tabs(struct gui_window_2 *gwin,
		int from, int insert_before)
{
	struct gui_window *ordered[AMI_SIDE_TAB_MAX];
	struct gui_window *gw;
	int n;
	int i;
	int j;

	if(gwin == NULL || from < 0)
		return;

	n = 0;
	for(i = 0; i < AMI_SIDE_TAB_MAX; i++) {
		if(gwin->side_tab_gw[i] != NULL)
			ordered[n++] = gwin->side_tab_gw[i];
	}
	if(from >= n)
		return;
	if(insert_before < 0)
		insert_before = 0;
	if(insert_before > n)
		insert_before = n;
	/* Dropping on self or the gap immediately after self is a no-op. */
	if(insert_before == from || insert_before == (from + 1))
		return;

	gw = ordered[from];
	for(j = from; j < (n - 1); j++)
		ordered[j] = ordered[j + 1];
	n--;
	if(insert_before > from)
		insert_before--;
	for(j = n; j > insert_before; j--)
		ordered[j] = ordered[j - 1];
	ordered[insert_before] = gw;
	n++;

	for(i = 0; i < AMI_SIDE_TAB_MAX; i++)
		gwin->side_tab_gw[i] = (i < n) ? ordered[i] : NULL;

	ami_gui_nami_sidebar_compact_tabs(gwin);
	ami_gui_nami_gtdrag_refresh_window(gwin);
	ami_schedule(0, ami_gui_nami_gtdrag_refresh_cb, gwin);
}

static void ami_gui_nami_gtdrag_move_to_window(
		struct gui_window_2 *dst_gwin, struct gui_window *src_gw)
{
	nsurl *url;
	nserror error;

	if(dst_gwin == NULL || src_gw == NULL || src_gw->bw == NULL)
		return;
	if(dst_gwin->gw == NULL || dst_gwin->gw->bw == NULL)
		return;
	if(src_gw->shared == dst_gwin)
		return;

	/* Restore gadget boxes before layout/create. */
	ami_gui_nami_gtdrag_restore_applied();

	url = NULL;
	if(browser_window_get_url(src_gw->bw, false, &url) != NSERROR_OK ||
	   url == NULL)
		return;

	/*
	 * Foreground so the destination switches to the new tab and paints.
	 * Destroy the source on the next schedule tick so dest rethink/redraw
	 * is not nested inside source-window teardown.
	 */
	error = browser_window_create(
			BW_CREATE_TAB | BW_CREATE_HISTORY | BW_CREATE_FOREGROUND,
			url, NULL, dst_gwin->gw->bw, NULL);
	nsurl_unref(url);
	if(error != NSERROR_OK)
		return;

	ami_gui_nami_gtdrag_refresh_window(dst_gwin);
	/* Tab faces settle after create — second sidebar refresh next tick. */
	ami_schedule(0, ami_gui_nami_gtdrag_refresh_cb, dst_gwin);

	ami_gtd_pend_destroy_gw = src_gw;
	ami_schedule(0, ami_gui_nami_gtdrag_pending_cb, NULL);
}

/*
 * Drop outside every Nami window: open a new window on that tab's URL and
 * remove the source tab (browser tear-off).  Refuses when this is the only
 * tab in the source window (would leave nothing to keep the window open).
 */
static void ami_gui_nami_gtdrag_tearoff_window(struct gui_window *src_gw)
{
	nsurl *url;
	nserror error;

	if(src_gw == NULL || src_gw->bw == NULL || src_gw->shared == NULL)
		return;
	if(src_gw->shared->tabs <= 1) {
		NSLOG(netsurf, INFO, "gtdrag: tear-off refused (sole tab)");
		return;
	}

	ami_gui_nami_gtdrag_restore_applied();

	url = NULL;
	if(browser_window_get_url(src_gw->bw, false, &url) != NSERROR_OK ||
	   url == NULL)
		return;

	NSLOG(netsurf, INFO, "gtdrag: tear-off → new window %s",
			nsurl_access(url));

	error = browser_window_create(
			BW_CREATE_HISTORY | BW_CREATE_FOREGROUND | BW_CREATE_CLONE,
			url, NULL, src_gw->bw, NULL);
	nsurl_unref(url);
	if(error != NSERROR_OK) {
		amiga_warn_user(messages_get_errorcode(error), 0);
		return;
	}

	ami_gtd_pend_destroy_gw = src_gw;
	ami_schedule(0, ami_gui_nami_gtdrag_pending_cb, NULL);
}

/*
 * Insert index from pointer Y: 0..n where n means append.  Midpoint of each
 * filled tab decides "before this tab" vs "after".
 */
static int ami_gui_nami_gtdrag_insert_index(struct gui_window_2 *gwin, int my)
{
	int i;
	int n;
	const struct ami_gtd_box *box;
	int mid;

	if(gwin == NULL)
		return 0;

	n = 0;
	for(i = 0; i < AMI_SIDE_TAB_MAX; i++) {
		if(gwin->side_tab_gw[i] == NULL)
			break;
		box = &ami_gtd_tab_box[i];
		n++;
		if(!box->valid)
			continue;
		mid = (int)box->abs_t + ((int)box->abs_h / 2);
		if(my < mid)
			return i;
	}
	return n;
}

/* True if the pointer is on a filled tab or in the append band below the last. */
static bool ami_gui_nami_gtdrag_tabstrip_hit(struct gui_window_2 *gwin,
		int mx, int my)
{
	int i;
	int n;
	int top;
	int bottom;
	int left;
	int right;
	const struct ami_gtd_box *box;
	struct Gadget *side;

	if(gwin == NULL)
		return false;

	n = 0;
	top = 0;
	bottom = 0;
	left = 0;
	right = 0;
	for(i = 0; i < AMI_SIDE_TAB_MAX; i++) {
		if(gwin->side_tab_gw[i] == NULL)
			break;
		box = &ami_gtd_tab_box[i];
		if(!box->valid)
			continue;
		if(ami_gui_nami_gtdrag_box_hit_cache(box, mx, my))
			return true;
		if(n == 0) {
			top = (int)box->abs_t;
			left = (int)box->abs_l;
			right = (int)box->abs_l + (int)box->abs_w;
		}
		bottom = (int)box->abs_t + (int)box->abs_h;
		if((int)box->abs_l < left)
			left = (int)box->abs_l;
		if(((int)box->abs_l + (int)box->abs_w) > right)
			right = (int)box->abs_l + (int)box->abs_w;
		n++;
	}
	if(n == 0) {
		/*
		 * Empty tab list (destination with no tabs yet should not happen
		 * for a live window) — accept the tab-list region of the sidebar.
		 */
		side = (struct Gadget *)gwin->objects[GID_SIDE_TABLIST];
		if(side != NULL && side->Width > 0 && side->Height > 0 &&
		   mx >= side->LeftEdge &&
		   mx <= (side->LeftEdge + side->Width) &&
		   my >= side->TopEdge &&
		   my <= (side->TopEdge + side->Height))
			return true;
		return false;
	}
	/* Title boxes sit right of the 20px favicon cell — include it. */
	left -= 22;
	if(mx >= left && mx <= right && my >= top && my <= (bottom + 12))
		return true;
	return false;
}

/*
 * Cross-window: any point in the sidebar below the hotlist (tab list + empty
 * checker) is a move/append target.  Same-window keeps tabstrip_hit only.
 */
static bool ami_gui_nami_gtdrag_sidebar_move_zone(struct gui_window_2 *gwin,
		int mx, int my)
{
	struct Gadget *side;
	struct Gadget *hot;
	int hot_bottom;

	if(gwin == NULL)
		return false;
	if(ami_gui_nami_gtdrag_tabstrip_hit(gwin, mx, my))
		return true;

	side = (struct Gadget *)gwin->objects[GID_SIDELAYOUT];
	if(side == NULL || side->Width < 1 || side->Height < 1)
		return false;
	if(mx < side->LeftEdge || mx > (side->LeftEdge + side->Width))
		return false;
	if(my < side->TopEdge || my > (side->TopEdge + side->Height))
		return false;

	/* Exclude the hotlist shelf — that remains bookmark-only. */
	hot = (struct Gadget *)gwin->objects[GID_SIDE_HOTLAYOUT];
	if(hot != NULL && hot->Height > 0) {
		hot_bottom = (int)hot->TopEdge + (int)hot->Height;
		if(my >= (int)hot->TopEdge && my <= hot_bottom)
			return false;
	}
	return true;
}

void ami_gui_nami_gtdrag_finish_drop(void)
{
	struct gui_window *src_gw;
	struct gui_window_2 *src_gwin;
	struct gui_window_2 *dst_gwin;
	struct Gadget *tgt_gad;
	struct Screen *scr;
	struct nsObject *node;
	struct nsObject *nnode;
	struct Window *win;
	struct gui_window_2 *cand;
	int src_slot;
	int insert;
	int i;
	int mx, my;
	int smx, smy;
	nsurl *url;

	src_gw = ami_gtdrag_get_drag_source();
	ami_gtdrag_clear_drag_source();
	ami_gtdrag_discard_drops();

	if(src_gw == NULL || src_gw->bw == NULL || src_gw->shared == NULL)
		return;
	src_gwin = src_gw->shared;
	src_slot = src_gw->sidebar_tab_slot;
	if(src_slot < 0 || src_slot >= AMI_SIDE_TAB_MAX)
		return;
	if(src_gwin->side_tab_gw[src_slot] != src_gw)
		return;

	/*
	 * Do not call WhichLayer/LockLayerInfo here — gtdrag may have just
	 * touched LayerInfo.  Hit-test window bounds against screen mouse.
	 */
	dst_gwin = NULL;
	scr = ami_gui_get_screen();
	if(scr == NULL)
		return;
	smx = scr->MouseX;
	smy = scr->MouseY;
	if(window_list != NULL && !IsMinListEmpty(window_list)) {
		node = (struct nsObject *)GetHead((struct List *)window_list);
		while(node != NULL) {
			nnode = (struct nsObject *)GetSucc((struct Node *)node);
			if(node->Type == AMINS_WINDOW) {
				cand = node->objstruct;
				win = (cand != NULL) ? cand->win : NULL;
				if(win != NULL && cand->ui_nami &&
				   smx >= win->LeftEdge &&
				   smx < (win->LeftEdge + win->Width) &&
				   smy >= win->TopEdge &&
				   smy < (win->TopEdge + win->Height)) {
					/* Prefer the frontmost match (later in list often). */
					dst_gwin = cand;
				}
			}
			node = nnode;
		}
	}
	if(dst_gwin == NULL || dst_gwin->win == NULL) {
		/*
		 * Tear-off only when the source window still has other tabs.
		 * Dragging the sole tab into empty space would close the old
		 * window — refuse that; move to another owned window instead.
		 */
		if(src_gwin->tabs <= 1) {
			NSLOG(netsurf, INFO,
					"gtdrag: finish_drop — sole tab, ignore empty-space drop");
			ami_gui_nami_sidebar_drop_wells_refresh_all();
			return;
		}
		NSLOG(netsurf, INFO,
				"gtdrag: finish_drop — outside all windows → tear-off");
		ami_gui_nami_gtdrag_tearoff_window(src_gw);
		ami_gui_nami_sidebar_drop_wells_refresh_all();
		return;
	}

	ami_gui_nami_gtdrag_restore_applied();
	ami_gui_nami_gtdrag_sync_bounds(dst_gwin);
	mx = dst_gwin->win->MouseX;
	my = dst_gwin->win->MouseY;
	tgt_gad = NULL;

	/*
	 * Favicon destinations first (empty wells / full strip), then tab
	 * insert-by-Y among filled tabs (browser-style reorder).
	 */
	for(i = 0; i < AMI_SIDE_HOTLIST_MAX; i++) {
		if(dst_gwin->side_hot_btn[i] == NULL)
			continue;
		if(dst_gwin->side_hot_url[i] != NULL)
			continue;
		if(ami_gui_nami_gtdrag_box_hit_cache(&ami_gtd_hot_box[i], mx, my)) {
			tgt_gad = (struct Gadget *)dst_gwin->side_hot_btn[i];
			break;
		}
	}
	if(tgt_gad == NULL) {
		/* Bookmark onto the shelf whenever a favicon slot is free. */
		for(i = 0; i < AMI_SIDE_HOTLIST_MAX; i++) {
			if(dst_gwin->side_hot_url[i] == NULL)
				break;
		}
		if(i < AMI_SIDE_HOTLIST_MAX &&
		   dst_gwin->objects[GID_SIDE_HOTLAYOUT] != NULL &&
		   ami_gui_nami_gtdrag_box_hit_cache(&ami_gtd_hotlay_box, mx, my))
			tgt_gad = (struct Gadget *)dst_gwin->objects[GID_SIDE_HOTLAYOUT];
	}

	if(tgt_gad != NULL && ami_gui_nami_gtdrag_is_hot(dst_gwin, tgt_gad)) {
		url = NULL;
		if(browser_window_get_url(src_gw->bw, false, &url) == NSERROR_OK &&
		   url != NULL) {
			NSLOG(netsurf, INFO,
					"gtdrag: finish_drop → HotlistToolbar bookmark");
			(void)hotlist_add_url_to_folder(url,
					messages_get("HotlistToolbar"));
			nsurl_unref(url);
			ami_gui_hotlist_update_all();
			ami_gui_nami_gtdrag_refresh_window(dst_gwin);
			ami_schedule(0, ami_gui_nami_gtdrag_refresh_cb, dst_gwin);
		}
		ami_gui_nami_sidebar_drop_wells_refresh_all();
		return;
	}

	/*
	 * Same window: reorder only on the filled tab strip.
	 * Other window: whole sidebar below the hotlist accepts move/append
	 * (empty checker = next empty slot).
	 */
	if(src_gwin == dst_gwin) {
		if(!ami_gui_nami_gtdrag_tabstrip_hit(dst_gwin, mx, my)) {
			NSLOG(netsurf, INFO,
					"gtdrag: finish_drop — no same-win reorder at %d,%d",
					mx, my);
			ami_gui_nami_sidebar_drop_wells_refresh_all();
			return;
		}
	} else if(!ami_gui_nami_gtdrag_sidebar_move_zone(dst_gwin, mx, my)) {
		NSLOG(netsurf, INFO,
				"gtdrag: finish_drop — no tab target at %d,%d", mx, my);
		ami_gui_nami_sidebar_drop_wells_refresh_all();
		return;
	}

	insert = ami_gui_nami_gtdrag_insert_index(dst_gwin, my);
	NSLOG(netsurf, INFO, "gtdrag: finish_drop → tab insert %d (src %d)",
			insert, src_slot);

	if(src_gwin == dst_gwin) {
		if(insert != src_slot && insert != (src_slot + 1))
			ami_gui_nami_gtdrag_schedule_reorder(dst_gwin, src_slot, insert);
		ami_gui_nami_sidebar_drop_wells_refresh_all();
		return;
	}

	ami_gui_nami_gtdrag_schedule_move(dst_gwin, src_gw);
	ami_gui_nami_sidebar_drop_wells_refresh_all();
}

void ami_gui_nami_gtdrag_drop(struct gui_window_2 *gwin,
		struct DropMessage *dm)
{
	/*
	 * Stale OBJECTDROPs from FakeInputEvent MakeDropMessage are discarded.
	 * Real actions go through ami_gui_nami_gtdrag_finish_drop().
	 */
	(void)gwin;
	(void)dm;
}

/*
 * Window-absolute hit boxes for sidebar gadgets.  apply/restore around
 * FilterIMsg only — never leave abs coords in Gadget LeftEdge permanently.
 */
static void ami_gui_nami_gtdrag_compute_box(struct ami_gtd_box *box,
		struct Gadget *gad, int prefer_mx, int prefer_my)
{
	ULONG left, top, width, height;
	WORD a_l, a_t, w, h;

	if(box == NULL || gad == NULL) {
		if(box != NULL)
			box->valid = false;
		return;
	}

	left = gad->LeftEdge;
	top = gad->TopEdge;
	width = gad->Width;
	height = gad->Height;

	GetAttr(GA_Left, (Object *)gad, &left);
	GetAttr(GA_Top, (Object *)gad, &top);
	GetAttr(GA_Width, (Object *)gad, &width);
	GetAttr(GA_Height, (Object *)gad, &height);

	w = (WORD)width;
	h = (WORD)height;
	a_l = (WORD)left;
	a_t = (WORD)top;

	/*
	 * Prefer GetAttr coords; if the pointer hits the raw Gadget edges
	 * instead, use those (some ReAction versions disagree).
	 */
	if(prefer_mx >= 0 && prefer_my >= 0 && w > 0 && h > 0) {
		if(!(prefer_mx >= a_l && prefer_mx <= (a_l + w) &&
		     prefer_my >= a_t && prefer_my <= (a_t + h)) &&
		   prefer_mx >= gad->LeftEdge &&
		   prefer_mx <= (gad->LeftEdge + gad->Width) &&
		   prefer_my >= gad->TopEdge &&
		   prefer_my <= (gad->TopEdge + gad->Height)) {
			a_l = gad->LeftEdge;
			a_t = gad->TopEdge;
			w = gad->Width;
			h = gad->Height;
		}
	}

	box->abs_l = a_l;
	box->abs_t = a_t;
	box->abs_w = w;
	box->abs_h = h;
	box->valid = (w > 0 && h > 0) ? true : false;
}

static void ami_gui_nami_gtdrag_apply_one(struct ami_gtd_box *box,
		struct Gadget *gad)
{
	if(box == NULL || gad == NULL || !box->valid)
		return;
	if(!box->applied) {
		box->save_l = gad->LeftEdge;
		box->save_t = gad->TopEdge;
		box->save_w = gad->Width;
		box->save_h = gad->Height;
		box->applied = true;
	}
	gad->LeftEdge = box->abs_l;
	gad->TopEdge = box->abs_t;
	gad->Width = box->abs_w;
	gad->Height = box->abs_h;
}

static void ami_gui_nami_gtdrag_restore_one(struct ami_gtd_box *box,
		struct Gadget *gad)
{
	if(box == NULL || gad == NULL || !box->applied)
		return;
	gad->LeftEdge = box->save_l;
	gad->TopEdge = box->save_t;
	gad->Width = box->save_w;
	gad->Height = box->save_h;
	box->applied = false;
}

static void ami_gui_nami_gtdrag_restore_all(struct gui_window_2 *gwin)
{
	int i;

	if(gwin == NULL)
		return;
	for(i = 0; i < AMI_SIDE_TAB_MAX; i++)
		ami_gui_nami_gtdrag_restore_one(&ami_gtd_tab_box[i],
				(struct Gadget *)gwin->side_tab_btn[i]);
	for(i = 0; i < AMI_SIDE_HOTLIST_MAX; i++)
		ami_gui_nami_gtdrag_restore_one(&ami_gtd_hot_box[i],
				(struct Gadget *)gwin->side_hot_btn[i]);
	if(gwin->objects[GID_SIDE_HOTLAYOUT] != NULL)
		ami_gui_nami_gtdrag_restore_one(&ami_gtd_hotlay_box,
				(struct Gadget *)gwin->objects[GID_SIDE_HOTLAYOUT]);
	if(ami_gtd_abs_owner == gwin)
		ami_gtd_abs_owner = NULL;
}

static void ami_gui_nami_gtdrag_restore_applied(void)
{
	if(ami_gtd_abs_owner != NULL)
		ami_gui_nami_gtdrag_restore_all(ami_gtd_abs_owner);
}

void ami_gui_nami_gtdrag_sync_bounds(struct gui_window_2 *gwin)
{
	int i;
	int mx, my;

	if(gwin == NULL || !ami_gtdrag_available())
		return;

	mx = -1;
	my = -1;
	if(gwin->win != NULL) {
		mx = gwin->win->MouseX;
		my = gwin->win->MouseY;
	}

	for(i = 0; i < AMI_SIDE_TAB_MAX; i++) {
		/*
		 * Hit-test the whole icon|title LayoutH row.  Title-only boxes
		 * were ~68px wide and missed favicon / right-side clicks that
		 * still look like the tab face (see ns.log "sidebar miss").
		 */
		if(gwin->side_tab_row[i] != NULL)
			ami_gui_nami_gtdrag_compute_box(&ami_gtd_tab_box[i],
					(struct Gadget *)gwin->side_tab_row[i],
					mx, my);
		else if(gwin->side_tab_btn[i] != NULL)
			ami_gui_nami_gtdrag_compute_box(&ami_gtd_tab_box[i],
					(struct Gadget *)gwin->side_tab_btn[i],
					mx, my);
		else
			ami_gtd_tab_box[i].valid = false;
	}
	for(i = 0; i < AMI_SIDE_HOTLIST_MAX; i++) {
		if(gwin->side_hot_btn[i] != NULL)
			ami_gui_nami_gtdrag_compute_box(&ami_gtd_hot_box[i],
					(struct Gadget *)gwin->side_hot_btn[i],
					mx, my);
		else
			ami_gtd_hot_box[i].valid = false;
	}
	if(gwin->objects[GID_SIDE_HOTLAYOUT] != NULL)
		ami_gui_nami_gtdrag_compute_box(&ami_gtd_hotlay_box,
				(struct Gadget *)gwin->objects[GID_SIDE_HOTLAYOUT],
				mx, my);
	else
		ami_gtd_hotlay_box.valid = false;
}

/* Apply cached absolute boxes onto gadgets for gtdrag PointIn* / FilterIMsg. */
static void ami_gui_nami_gtdrag_apply_abs(struct gui_window_2 *gwin)
{
	int i;

	if(gwin == NULL)
		return;
	/* Cross-window drag: restore the previous owner before mutating another. */
	if(ami_gtd_abs_owner != NULL && ami_gtd_abs_owner != gwin)
		ami_gui_nami_gtdrag_restore_all(ami_gtd_abs_owner);

	ami_gui_nami_gtdrag_sync_bounds(gwin);
	for(i = 0; i < AMI_SIDE_TAB_MAX; i++)
		ami_gui_nami_gtdrag_apply_one(&ami_gtd_tab_box[i],
				(struct Gadget *)gwin->side_tab_btn[i]);
	for(i = 0; i < AMI_SIDE_HOTLIST_MAX; i++)
		ami_gui_nami_gtdrag_apply_one(&ami_gtd_hot_box[i],
				(struct Gadget *)gwin->side_hot_btn[i]);
	if(gwin->objects[GID_SIDE_HOTLAYOUT] != NULL)
		ami_gui_nami_gtdrag_apply_one(&ami_gtd_hotlay_box,
				(struct Gadget *)gwin->objects[GID_SIDE_HOTLAYOUT]);
	ami_gtd_abs_owner = gwin;
}

static bool ami_gui_nami_gtdrag_box_hit_cache(const struct ami_gtd_box *box,
		int mx, int my)
{
	if(box == NULL || !box->valid)
		return false;
	if(mx < box->abs_l || my < box->abs_t)
		return false;
	if(mx > (box->abs_l + box->abs_w))
		return false;
	if(my > (box->abs_t + box->abs_h))
		return false;
	return true;
}

/*
 * RelVerify sidebar buttons may not deliver WMHI_GADGETDOWN.  Arm from
 * LMB+MOUSEMOVE (IDCMP hook) or SELECTDOWN when it does arrive.  Absolute
 * boxes are applied for the drag duration and restored when it ends.
 */
static bool ami_gui_nami_gtdrag_try_arm_ex(struct gui_window_2 *gwin,
		bool diagnose)
{
	int i;
	int mx, my;
	struct Gadget *gad;
	struct Gadget *side;

	if(gwin == NULL || gwin->win == NULL || gwin->ui_nami == false)
		return false;
	if(!ami_gtdrag_available() || !gwin->gtdrag_registered)
		return false;
	if(ami_gtdrag_armed())
		return true;

	mx = gwin->win->MouseX;
	my = gwin->win->MouseY;
	ami_gui_nami_gtdrag_sync_bounds(gwin);

	for(i = 0; i < AMI_SIDE_TAB_MAX; i++) {
		gad = (struct Gadget *)gwin->side_tab_btn[i];
		if(gad == NULL || gwin->side_tab_gw[i] == NULL)
			continue;
		/* Full icon|title row (see sync_bounds). */
		if(ami_gui_nami_gtdrag_box_hit_cache(&ami_gtd_tab_box[i], mx, my)) {
			NSLOG(netsurf, INFO,
					"gtdrag: arm tab slot %d at %d,%d box=%d,%d %dx%d",
					i, mx, my,
					(int)ami_gtd_tab_box[i].abs_l,
					(int)ami_gtd_tab_box[i].abs_t,
					(int)ami_gtd_tab_box[i].abs_w,
					(int)ami_gtd_tab_box[i].abs_h);
			/* Abs boxes before arm — never let arm mutate LeftEdge. */
			ami_gui_nami_gtdrag_apply_abs(gwin);
			ami_gtdrag_arm(gad, gwin->win);
			ami_gtdrag_set_drag_source(gwin->side_tab_gw[i]);
			ami_schedule(50, ami_gui_nami_gtdrag_unlock_poll_cb, NULL);
			return true;
		}
	}

	if(diagnose) {
		side = (struct Gadget *)gwin->objects[GID_SIDELAYOUT];
		if(side != NULL &&
		   mx >= side->LeftEdge &&
		   mx <= (side->LeftEdge + side->Width) &&
		   my >= side->TopEdge &&
		   my <= (side->TopEdge + side->Height)) {
			NSLOG(netsurf, INFO,
					"gtdrag: sidebar miss at %d,%d side=%d,%d %dx%d "
					"(click not on a filled tab row)",
					mx, my,
					(int)side->LeftEdge, (int)side->TopEdge,
					(int)side->Width, (int)side->Height);
			for(i = 0; i < AMI_SIDE_TAB_MAX; i++) {
				if(gwin->side_tab_gw[i] == NULL)
					break;
				if(!ami_gtd_tab_box[i].valid)
					continue;
				NSLOG(netsurf, INFO,
						"gtdrag:   tab%d box=%d,%d %dx%d",
						i,
						(int)ami_gtd_tab_box[i].abs_l,
						(int)ami_gtd_tab_box[i].abs_t,
						(int)ami_gtd_tab_box[i].abs_w,
						(int)ami_gtd_tab_box[i].abs_h);
			}
		}
	}
	return false;
}

static void ami_gui_nami_gtdrag_window_add(struct gui_window_2 *gwin)
{
	int i;
	struct Gadget *gad;

	if(gwin == NULL || gwin->ui_nami == false || gwin->win == NULL)
		return;
	if(!ami_gtdrag_available())
		return;
	if(gwin->gtdrag_registered)
		return;

	ami_gtdrag_add_window(gwin->win);

	for(i = 0; i < AMI_SIDE_TAB_MAX; i++) {
		gad = (struct Gadget *)gwin->side_tab_btn[i];
		if(gad == NULL)
			continue;
		memset(&gwin->side_tab_inode[i], 0, sizeof(struct ImageNode));
		ami_gtdrag_add_tab_button(gad, gwin->win,
				&gwin->side_tab_inode[i],
				ami_gui_nami_gtdrag_tab_objfunc);
		/* Icon cell: drop target only (drag arms via the title button). */
		if(gwin->side_tab_icon_btn[i] != NULL)
			ami_gtdrag_add_hot_button(
					(struct Gadget *)gwin->side_tab_icon_btn[i],
					gwin->win);
	}

	for(i = 0; i < AMI_SIDE_HOTLIST_MAX; i++) {
		gad = (struct Gadget *)gwin->side_hot_btn[i];
		if(gad == NULL)
			continue;
		ami_gtdrag_add_hot_button(gad, gwin->win);
	}

	if(gwin->objects[GID_SIDE_HOTLAYOUT] != NULL) {
		ami_gtdrag_add_hot_button(
				(struct Gadget *)gwin->objects[GID_SIDE_HOTLAYOUT],
				gwin->win);
	}

	gwin->gtdrag_registered = true;
	NSLOG(netsurf, INFO, "gtdrag: window registered (%d tab slots)",
			AMI_SIDE_TAB_MAX);
}

static void ami_gui_nami_gtdrag_window_rem(struct gui_window_2 *gwin)
{
	if(gwin == NULL || !gwin->gtdrag_registered)
		return;
	if(ami_gtd_pend_dst == gwin || ami_gtd_pend_swap_win == gwin ||
	   (ami_gtd_pend_src_gw != NULL && ami_gtd_pend_src_gw->shared == gwin) ||
	   (ami_gtd_pend_destroy_gw != NULL &&
	    ami_gtd_pend_destroy_gw->shared == gwin))
		ami_gui_nami_gtdrag_cancel_pending();
	ami_gui_nami_gtdrag_restore_all(gwin);
	if(gwin->win != NULL)
		ami_gtdrag_rem_window(gwin->win);
	gwin->gtdrag_registered = false;
}

static void ami_gui_nami_sidebar_toggle_cb(void *p)
{
	struct gui_window_2 *gwin = (struct gui_window_2 *)p;

	if(gwin == NULL || gwin->win == NULL || gwin->ui_nami == false)
		return;
	ami_gui_nami_sidebar_set_expanded(gwin,
			gwin->sidebar_expanded ? false : true);
}

static void ami_gui_nami_new_tab_cb(void *p)
{
	struct gui_window_2 *gwin = (struct gui_window_2 *)p;

	if(gwin == NULL || gwin->win == NULL)
		return;
	ami_gui_new_blank_tab(gwin);
}

static void ami_gui_nami_close_tab_cb(void *p)
{
	struct gui_window_2 *gwin = (struct gui_window_2 *)p;

	if(gwin == NULL || gwin->win == NULL)
		return;
	if(gwin->gw != NULL && gwin->gw->bw != NULL)
		browser_window_destroy(gwin->gw->bw);
}

static void ami_gui_nami_sidebar_set_expanded(struct gui_window_2 *gwin,
		bool expanded)
{
	Object *body;
	Object *side;
	Object *browser;
	ULONG weight;
	struct Gadget *side_gad;

	if(gwin == NULL || gwin->ui_nami == false)
		return;
	if(gwin->win == NULL)
		return;

	body = gwin->objects[GID_BODYLAYOUT];
	side = gwin->objects[GID_SIDELAYOUT];
	browser = gwin->objects[GID_BROWSERCOL];
	if(body == NULL || side == NULL || browser == NULL)
		return;

	if(gwin->sidebar_expanded == expanded)
		return;

	if(expanded) {
		if(gwin->sidebar_weight < 8)
			gwin->sidebar_weight = 20;
		weight = (ULONG)gwin->sidebar_weight;

		/*
		 * Re-insert sidebar before browser with LAYOUT_WeightBar.
		 * CHILD_NoDispose: RemoveChild must not free side/browser.
		 */
		SetGadgetAttrs((struct Gadget *)body, gwin->win, NULL,
				LAYOUT_RemoveChild, browser,
				TAG_DONE);
		SetGadgetAttrs((struct Gadget *)body, gwin->win, NULL,
				LAYOUT_AddChild, side,
					CHILD_WeightedWidth, weight,
					CHILD_MinWidth, 96,
					CHILD_MaxWidth, ~0,
					CHILD_CacheDomain, FALSE,
					CHILD_NoDispose, TRUE,
				LAYOUT_WeightBar, TRUE,
				LAYOUT_AddChild, browser,
					CHILD_WeightedWidth, 100,
					CHILD_NoDispose, TRUE,
				TAG_DONE);

		FlushLayoutDomainCache((struct Gadget *)body);
		RethinkLayout((struct Gadget *)body, gwin->win, NULL, TRUE);
		gwin->sidebar_expanded = true;
		ami_gui_nami_sidebar_tab_rethink(gwin);
	} else {
		/*
		 * Remove sidebar from BODY — its LAYOUT_WeightBar goes with it.
		 * CHILD_NoDispose keeps SIDELAYOUT alive for re-insert.
		 */
		SetGadgetAttrs((struct Gadget *)body, gwin->win, NULL,
				LAYOUT_RemoveChild, side,
				TAG_DONE);

		FlushLayoutDomainCache((struct Gadget *)body);
		RethinkLayout((struct Gadget *)body, gwin->win, NULL, TRUE);
		gwin->sidebar_expanded = false;

		/*
		 * Detached panel must not keep a drawable box — NEWSIZE / damage
		 * RefreshGList would otherwise blit the old strip over the browser.
		 */
		side_gad = (struct Gadget *)side;
		side_gad->Width = 0;
		side_gad->Height = 0;
		side_gad->LeftEdge = 0;
		side_gad->TopEdge = 0;
	}
}

/*
 * Load a theme BitMapObj; reject zero-size (missing AISS file).
 * fallback_theme may be another messages key (e.g. theme_closetab).
 * If a TBImages:list_FOO path fails, also try TBImages:FOO.
 */
static Object *ami_gui_theme_bitmap(struct Screen *scrn,
		const char *theme_key, const char *fallback_key)
{
	char path[100];
	char alt[100];
	Object *bmo;
	struct Image *im;
	char *listp;
	size_t prefix;

	bmo = NULL;
	im = NULL;
	path[0] = '\0';
	alt[0] = '\0';
	listp = NULL;
	prefix = 0;

	if(theme_key != NULL) {
		ami_get_theme_filename(path, theme_key, false);
		if(path[0] != '\0') {
			bmo = BitMapObj,
					BITMAP_SourceFile, path,
					BITMAP_Screen, scrn,
					BITMAP_Masking, TRUE,
				BitMapEnd;
		}
	}

	im = (struct Image *)bmo;
	if(bmo != NULL && (im == NULL || im->Width < 1 || im->Height < 1)) {
		DisposeObject(bmo);
		bmo = NULL;
	}

	/* list_close missing → try close, etc. */
	if(bmo == NULL && path[0] != '\0') {
		listp = strstr(path, "list_");
		if(listp != NULL) {
			prefix = (size_t)(listp - path);
			if(prefix + strlen(listp + 5) + 1 < sizeof(alt)) {
				memcpy(alt, path, prefix);
				strcpy(alt + prefix, listp + 5);
				bmo = BitMapObj,
						BITMAP_SourceFile, alt,
						BITMAP_Screen, scrn,
						BITMAP_Masking, TRUE,
					BitMapEnd;
				im = (struct Image *)bmo;
				if(bmo != NULL &&
				   (im == NULL || im->Width < 1 || im->Height < 1)) {
					DisposeObject(bmo);
					bmo = NULL;
				}
			}
		}
	}

	/*
	 * AISS: prefer toolbar.  If missing, try selecttoggle then toggle —
	 * never fall back to "tool" (wrong glyph).
	 */
	if(bmo == NULL && path[0] != '\0') {
		listp = strstr(path, "toolbar");
		if(listp != NULL && strcmp(listp, "toolbar") == 0) {
			prefix = (size_t)(listp - path);
			if(prefix + 14 < sizeof(alt)) {
				memcpy(alt, path, prefix);
				strcpy(alt + prefix, "selecttoggle");
				bmo = BitMapObj,
						BITMAP_SourceFile, alt,
						BITMAP_Screen, scrn,
						BITMAP_Masking, TRUE,
					BitMapEnd;
				im = (struct Image *)bmo;
				if(bmo != NULL &&
				   (im == NULL || im->Width < 1 || im->Height < 1)) {
					DisposeObject(bmo);
					bmo = NULL;
				}
			}
			if(bmo == NULL && prefix + 7 < sizeof(alt)) {
				memcpy(alt, path, prefix);
				strcpy(alt + prefix, "toggle");
				bmo = BitMapObj,
						BITMAP_SourceFile, alt,
						BITMAP_Screen, scrn,
						BITMAP_Masking, TRUE,
					BitMapEnd;
				im = (struct Image *)bmo;
				if(bmo != NULL &&
				   (im == NULL || im->Width < 1 || im->Height < 1)) {
					DisposeObject(bmo);
					bmo = NULL;
				}
			}
		}
	}

	if(bmo == NULL && fallback_key != NULL) {
		path[0] = '\0';
		ami_get_theme_filename(path, fallback_key, false);
		if(path[0] != '\0') {
			bmo = BitMapObj,
					BITMAP_SourceFile, path,
					BITMAP_Screen, scrn,
					BITMAP_Masking, TRUE,
				BitMapEnd;
		}
		im = (struct Image *)bmo;
		if(bmo != NULL && (im == NULL || im->Width < 1 || im->Height < 1)) {
			DisposeObject(bmo);
			bmo = NULL;
		}
	}

	return bmo;
}

/** Apply truncated TNA_Text; full title stays in help text / tabtitle. */
static void ami_gui_apply_tab_label(struct gui_window *g, size_t max_chars)
{
	char *ellip_utf8;
	char *local_label;
	const char *utf8_src;
	size_t slen;
	size_t keep;

	if((g == NULL) || (g->tab_node == NULL))
		return;

	local_label = NULL;
	utf8_src = NULL;
	if((g->bw != NULL) && (browser_window_has_content(g->bw) == true))
		utf8_src = browser_window_get_title(g->bw);

	if(utf8_src != NULL) {
		ellip_utf8 = ami_gui_utf8_ellipsize(utf8_src, max_chars);
		if(ellip_utf8 != NULL) {
			local_label = ami_utf8_easy(ellip_utf8);
			free(ellip_utf8);
		}
	} else if(g->tabtitle != NULL) {
		slen = strlen(g->tabtitle);
		if(slen <= max_chars) {
			local_label = strdup(g->tabtitle);
		} else {
			keep = (max_chars > 3) ? (max_chars - 3) : 1;
			local_label = malloc(keep + 4);
			if(local_label != NULL) {
				memcpy(local_label, g->tabtitle, keep);
				local_label[keep] = '.';
				local_label[keep + 1] = '.';
				local_label[keep + 2] = '.';
				local_label[keep + 3] = '\0';
			}
		}
	}

	if(local_label == NULL)
		return;

	{
		char *old_label;

		old_label = g->tab_label;
		g->tab_label = local_label;

		SetClickTabNodeAttrs(g->tab_node,
				TNA_Text, g->tab_label,
				TNA_HelpText, (g->tabtitle != NULL) ? g->tabtitle : g->tab_label,
				TAG_DONE);
#ifdef __amigaos4__
		SetClickTabNodeAttrs(g->tab_node,
				TNA_HintInfo, (g->tabtitle != NULL) ? g->tabtitle : g->tab_label,
				TAG_DONE);
#endif
		/* Update sidebar label before freeing the old string */
		if(g->shared != NULL && g->shared->ui_nami)
			ami_gui_nami_sidebar_apply_label(g);

		if(old_label != NULL)
			free(old_label);
	}
}

/** Recompute every tab label when tab count or width changes. */
static void ami_gui_relabel_all_tabs(struct gui_window_2 *gwin)
{
	struct Node *node;
	struct gui_window *gw;
	size_t max_chars;

	if(gwin == NULL)
		return;
	if(gwin->tabs < 1)
		return;

	/* Nami: sidebar button labels; no ClickTab gadget */
	if(gwin->ui_nami) {
		max_chars = ami_gui_nami_tab_label_max_chars(gwin);
		node = GetHead(&gwin->tab_list);
		while(node != NULL) {
			if(node != gwin->new_tab_tab) {
				gw = NULL;
				GetClickTabNodeAttrs(node, TNA_UserData, &gw, TAG_DONE);
				if(gw != NULL)
					ami_gui_apply_tab_label(gw, max_chars);
			}
			node = GetSucc(node);
		}
		return;
	}

	if(gwin->objects[GID_TABS] == NULL)
		return;

	max_chars = ami_gui_tab_label_max_chars(gwin);

	SetGadgetAttrs((struct Gadget *)gwin->objects[GID_TABS],
			gwin->win, NULL,
			CLICKTAB_Labels, ~0,
			TAG_DONE);

	node = GetHead(&gwin->tab_list);
	while(node != NULL) {
		if(node != gwin->new_tab_tab) {
			gw = NULL;
			GetClickTabNodeAttrs(node, TNA_UserData, &gw, TAG_DONE);
			if(gw != NULL)
				ami_gui_apply_tab_label(gw, max_chars);
		}
		node = GetSucc(node);
	}

	/* Text/tooltip only — skip page/layout rethink (CLICKTAB_MinorLabelChange). */
	RefreshSetGadgetAttrs((struct Gadget *)gwin->objects[GID_TABS],
			gwin->win, NULL,
			CLICKTAB_Labels, &gwin->tab_list,
			CLICKTAB_MinorLabelChange, TRUE,
			TAG_DONE);
}

/* System sysiclass image for window chrome / clicktab close. */
static Object *ami_gui_make_sysi_image(struct Screen *scrn, ULONG which)
{
	Object *img;
	struct DrawInfo *dri;

	img = NULL;
	dri = GetScreenDrawInfo(scrn);
	if(dri != NULL) {
		img = NewObject(NULL, "sysiclass",
				SYSIA_Which, which,
				SYSIA_DrawInfo, dri,
				TAG_DONE);
		FreeScreenDrawInfo(scrn, dri);
	}
	return img;
}

/*
 * Title-bar sysiclass images keep non-zero LeftEdge/TopEdge for
 * GFLG_RELRIGHT / border placement.  Zero those for in-layout buttons.
 * Decor may restore them on later draws — re-apply via ami_gui_nami_fix_chrome_images.
 */
static Object *ami_gui_make_chrome_sysi_image(struct Screen *scrn, ULONG which)
{
	Object *img;
	struct Image *im;

	img = ami_gui_make_sysi_image(scrn, which);
	if(img != NULL) {
		im = (struct Image *)img;
		im->LeftEdge = 0;
		im->TopEdge = 0;
	}
	return img;
}

/* Keep chrome sysi LeftEdge/TopEdge at 0 after layout/ClickTab refresh. */
static void ami_gui_nami_fix_chrome_images(struct gui_window_2 *gwin)
{
	struct Image *im;
	int i;
	Object *objs[3];

	if(gwin == NULL || gwin->ui_nami == false)
		return;

	objs[0] = gwin->objects[GID_WIN_CLOSE_BM];
	objs[1] = gwin->objects[GID_WIN_ZOOM_BM];
	objs[2] = gwin->objects[GID_WIN_DEPTH_BM];
	for(i = 0; i < 3; i++) {
		if(objs[i] != NULL) {
			im = (struct Image *)objs[i];
			im->LeftEdge = 0;
			im->TopEdge = 0;
		}
	}
}

/* Fill a rect with the active/inactive window title-bar chrome pen. */
static void ami_gui_chrome_fill_rect(struct RastPort *rp, struct Window *win,
		WORD x1, WORD y1, WORD x2, WORD y2)
{
	struct DrawInfo *dri;
	ULONG penidx;

	if(rp == NULL || win == NULL || win->WScreen == NULL)
		return;
	if(x2 < x1 || y2 < y1)
		return;

	dri = GetScreenDrawInfo(win->WScreen);
	if(dri == NULL)
		return;

	/* Active title bars use FILLPEN; inactive use BACKGROUNDPEN */
	penidx = FILLPEN;
	if((win->Flags & WFLG_WINDOWACTIVE) == 0)
		penidx = BACKGROUNDPEN;

	SetAPen(rp, dri->dri_Pens[penidx]);
	RectFill(rp, x1, y1, x2, y2);
	FreeScreenDrawInfo(win->WScreen, dri);
}

/**
 * Y just below Nami's in-layout chrome row (close/tabs/zoom/depth).
 * Without a system DragBar, BorderTop is only a few pixels, so SizeBRight
 * runs beside the chrome — a real title border would avoid that.
 */
static WORD ami_gui_nami_chrome_bottom(struct gui_window_2 *gwin)
{
	struct Gadget *gad;
	WORD bottom;
	WORD gbottom;

	if(gwin == NULL || gwin->win == NULL)
		return 0;

	bottom = gwin->win->BorderTop;
	if(gwin->objects[GID_CHROMELAYOUT] != NULL) {
		gad = (struct Gadget *)gwin->objects[GID_CHROMELAYOUT];
		if(gad->Height > 0)
			bottom = (WORD)(gad->TopEdge + gad->Height);
	}
	/* Border-mounted zoom/depth define the true title-bar bottom */
	if(gwin->objects[GID_WIN_DEPTH] != NULL) {
		gad = (struct Gadget *)gwin->objects[GID_WIN_DEPTH];
		if(gad->Height > 0) {
			gbottom = (WORD)(gad->TopEdge + gad->Height);
			if(gbottom > bottom)
				bottom = gbottom;
		}
	}
	if(gwin->objects[GID_WIN_ZOOM] != NULL) {
		gad = (struct Gadget *)gwin->objects[GID_WIN_ZOOM];
		if(gad->Height > 0) {
			gbottom = (WORD)(gad->TopEdge + gad->Height);
			if(gbottom > bottom)
				bottom = gbottom;
		}
	}
	if(bottom < gwin->win->BorderTop)
		bottom = gwin->win->BorderTop;
	return bottom;
}

/* GFLG_RELRIGHT stores a negative LeftEdge until Intuition resolves it. */
static void ami_gui_nami_gadget_xy(struct Window *win, struct Gadget *gad,
		WORD *x, WORD *y)
{
	*x = 0;
	*y = 0;
	if(win == NULL || gad == NULL)
		return;
	*x = gad->LeftEdge;
	*y = gad->TopEdge;
	if((gad->Flags & GFLG_RELRIGHT) != 0 && *x <= 0)
		*x = (WORD)(win->Width + gad->LeftEdge - 1);
	if((gad->Flags & GFLG_RELBOTTOM) != 0 && *y <= 0)
		*y = (WORD)(win->Height + gad->TopEdge - 1);
}

/* Inactive border image over the buttongclass glyph.
 * Do not wipe the rect first: a BACKGROUNDPEN fill is the same grey as
 * the toolbar, and IDS_INACTIVENORMAL then has nothing left to show.
 * Do not rewrite the image's LeftEdge — AISS uses that origin. */
static void ami_gui_nami_draw_corner_inactive(struct gui_window_2 *gwin,
		Object *gadobj, Object *img, struct DrawInfo *dri)
{
	struct Window *win;
	struct Gadget *gad;
	struct Image *im;
	ULONG state;
	WORD x;
	WORD y;
	WORD gid;

	if(gwin == NULL || gadobj == NULL || img == NULL)
		return;
	win = gwin->win;
	if(win == NULL || win->RPort == NULL)
		return;
	gad = (struct Gadget *)gadobj;
	im = (struct Image *)img;
	if(gad->Width < 1 || gad->Height < 1)
		return;
	if(im->Width < 1 || im->Height < 1)
		return;

	ami_gui_nami_gadget_xy(win, gad, &x, &y);
	state = IDS_INACTIVENORMAL;
	gid = 0;
	if(gadobj == gwin->objects[GID_WIN_ZOOM])
		gid = GID_WIN_ZOOM;
	else if(gadobj == gwin->objects[GID_WIN_DEPTH])
		gid = GID_WIN_DEPTH;
	if(gid != 0 && gwin->chrome_armed == gid)
		state = IDS_INACTIVESELECTED;
	DrawImageState(win->RPort, im, x, y, state, dri);
}

/* Redraw zoom and depth after the chrome row.  They are not layout
 * children, so a toolbar fill covers them.  Intuition's own refresh
 * puts GFLG_RELRIGHT back on top.  An inactive window then gets the
 * clear border image, because buttongclass always draws the blue one. */
void ami_gui_nami_refresh_corner_gadgets(struct gui_window_2 *gwin)
{
	struct Window *win;
	struct DrawInfo *dri;

	if(gwin == NULL || gwin->nami_border_depth == false)
		return;
	win = gwin->win;
	if(win == NULL)
		return;

	if(gwin->objects[GID_WIN_ZOOM] != NULL)
		RefreshGList((struct Gadget *)gwin->objects[GID_WIN_ZOOM],
				win, NULL, 1);
	if(gwin->objects[GID_WIN_DEPTH] != NULL)
		RefreshGList((struct Gadget *)gwin->objects[GID_WIN_DEPTH],
				win, NULL, 1);

	if((win->Flags & WFLG_WINDOWACTIVE) != 0)
		return;

	dri = NULL;
	if(win->WScreen != NULL)
		dri = GetScreenDrawInfo(win->WScreen);
	ami_gui_nami_draw_corner_inactive(gwin,
			gwin->objects[GID_WIN_ZOOM],
			gwin->objects[GID_WIN_ZOOM_BM], dri);
	ami_gui_nami_draw_corner_inactive(gwin,
			gwin->objects[GID_WIN_DEPTH],
			gwin->objects[GID_WIN_DEPTH_BM], dri);
	if(dri != NULL)
		FreeScreenDrawInfo(win->WScreen, dri);
}

/**
 * Cover SizeBRight fill beside the chrome so the title bar reads full-width.
 * The fill starts below the corner zoom/depth glyphs so it cannot erase
 * them; those two are redrawn afterwards (active/inactive image included).
 */
static void ami_gui_nami_paint_chrome_rborder(struct gui_window_2 *gwin)
{
	struct Window *win;
	struct Gadget *gad;
	WORD x1, y1, x2, y2;
	WORD br;
	WORD below;

	if(gwin == NULL || gwin->ui_nami == false)
		return;
	win = gwin->win;
	if(win == NULL || win->RPort == NULL)
		return;

	br = win->BorderRight;
	if(br < 2)
		return;

	y2 = (WORD)(ami_gui_nami_chrome_bottom(gwin) - 1);
	if(y2 < 0)
		return;

	x1 = (WORD)(win->Width - br);
	x2 = (WORD)(win->Width - 2);
	y1 = 0;

	/* Leave the top-right glyphs alone; fill only the strip under them. */
	if(gwin->nami_border_depth) {
		gad = (struct Gadget *)gwin->objects[GID_WIN_ZOOM];
		if(gad != NULL && gad->Height > 0) {
			below = (WORD)(gad->TopEdge + gad->Height);
			if(below > y1)
				y1 = below;
		}
		gad = (struct Gadget *)gwin->objects[GID_WIN_DEPTH];
		if(gad != NULL && gad->Height > 0) {
			below = (WORD)(gad->TopEdge + gad->Height);
			if(below > y1)
				y1 = below;
		}
	}

	if(x2 >= x1 && y2 >= y1)
		ami_gui_chrome_fill_rect(win->RPort, win, x1, y1, x2, y2);

	ami_gui_nami_refresh_corner_gadgets(gwin);
}

/* Drag strip: opaque chrome fill (title-bar colour). */
HOOKF(uint32, ami_gui_chrome_drag_render_hook, APTR, space, struct gpRender *)
{
	struct gui_window_2 *gwin;
	struct Gadget *gad;

	if(msg->gpr_Redraw != GREDRAW_REDRAW)
		return 0;

	gwin = (struct gui_window_2 *)hook->h_Data;
	gad = (struct Gadget *)space;
	if(gwin == NULL || gwin->win == NULL || gad == NULL || msg->gpr_RPort == NULL)
		return 0;

	ami_gui_chrome_fill_rect(msg->gpr_RPort, gwin->win,
			gad->LeftEdge, gad->TopEdge,
			(WORD)(gad->LeftEdge + gad->Width - 1),
			(WORD)(gad->TopEdge + gad->Height - 1));
	/* Keep SizeBRight strip matched to title chrome beside depth */
	ami_gui_nami_paint_chrome_rborder(gwin);
	return 0;
}

/* Draw a sysiclass title-bar image inside a SpaceObj (offsets forced to 0). */
HOOKF(uint32, ami_gui_chrome_sysi_render_hook, APTR, space, struct gpRender *)
{
	struct gui_window_2 *gwin;
	struct Image *im;
	struct Gadget *gad;
	struct DrawInfo *dri;
	Object *img;
	ULONG state;
	WORD gid;
	BOOL border_gad;

	if(msg->gpr_Redraw != GREDRAW_REDRAW)
		return 0;

	gwin = (struct gui_window_2 *)hook->h_Data;
	gad = (struct Gadget *)space;
	if(gwin == NULL || gad == NULL || msg->gpr_RPort == NULL)
		return 0;

	img = NULL;
	gid = 0;
	border_gad = FALSE;
	if(space == gwin->objects[GID_WIN_CLOSE]) {
		img = gwin->objects[GID_WIN_CLOSE_BM];
		gid = GID_WIN_CLOSE;
	} else if(space == gwin->objects[GID_WIN_ZOOM]) {
		img = gwin->objects[GID_WIN_ZOOM_BM];
		gid = GID_WIN_ZOOM;
		border_gad = TRUE;
	} else if(space == gwin->objects[GID_WIN_DEPTH]) {
		img = gwin->objects[GID_WIN_DEPTH_BM];
		gid = GID_WIN_DEPTH;
		border_gad = TRUE;
	}
	/* Do not call ami_gui_nami_paint_chrome_rborder here — it RefreshGLists
	 * the depth SpaceObj and would recurse through this hook. */
	if(img == NULL)
		return 0;

	/*
	 * Close sits in the interior chrome row: opaque FILLPEN plate, and
	 * always the active image.  Zoom and depth sit in the window border
	 * (outside that chrome).  AISS paints the interior plate if we fill
	 * FILLPEN behind them — leave the border alone and use the border
	 * image state (active FILLPEN glyph, inactive BACKGROUNDPEN glyph).
	 */
	if(border_gad == FALSE && gwin->win != NULL) {
		ami_gui_chrome_fill_rect(msg->gpr_RPort, gwin->win,
				gad->LeftEdge, gad->TopEdge,
				(WORD)(gad->LeftEdge + gad->Width - 1),
				(WORD)(gad->TopEdge + gad->Height - 1));
	}

	im = (struct Image *)img;
	if(im->Width < 1 || im->Height < 1)
		return 0;
	im->LeftEdge = 0;
	im->TopEdge = 0;
	state = IDS_NORMAL;
	if(border_gad && gwin->win != NULL &&
	   (gwin->win->Flags & WFLG_WINDOWACTIVE) == 0)
		state = IDS_INACTIVENORMAL;
	if(gwin->chrome_armed == gid) {
		if(state == IDS_INACTIVENORMAL)
			state = IDS_INACTIVESELECTED;
		else
			state = IDS_SELECTED;
	}

	dri = NULL;
	if(gwin->win != NULL && gwin->win->WScreen != NULL)
		dri = GetScreenDrawInfo(gwin->win->WScreen);
	DrawImageState(msg->gpr_RPort, im, gad->LeftEdge, gad->TopEdge,
			state, dri);
	if(dri != NULL)
		FreeScreenDrawInfo(gwin->win->WScreen, dri);
	return 0;
}

/* Refresh one chrome gadget after arm/disarm. */
static void ami_gui_nami_chrome_refresh(struct gui_window_2 *gwin, WORD gid)
{
	if(gwin == NULL || gwin->win == NULL || gid <= 0)
		return;
	if(gwin->objects[gid] == NULL)
		return;

	/* Border zoom/depth: buttongclass would redraw the active glyph. */
	if(gwin->nami_border_depth &&
	   (gid == GID_WIN_ZOOM || gid == GID_WIN_DEPTH)) {
		ami_gui_nami_refresh_corner_gadgets(gwin);
		return;
	}

	RefreshGList((struct Gadget *)gwin->objects[gid], gwin->win, NULL, 1);
}

/* Fire chrome action for an armed gadget (mouse still over it). */
static void ami_gui_nami_chrome_activate(struct gui_window_2 *gwin,
		WORD gid, BOOL *win_closed)
{
	if(gwin == NULL)
		return;

	switch(gid) {
	case GID_WIN_CLOSE:
		ami_gui_close_window(gwin);
		if(win_closed != NULL)
			*win_closed = TRUE;
		break;
	case GID_WIN_ZOOM:
		SetAttrs(gwin->objects[OID_MAIN], WINDOW_Zoom, TRUE, TAG_DONE);
		break;
	case GID_WIN_DEPTH:
		/* Front-most window goes back; any other comes forward. */
		{
			struct Screen *scr;
			struct Window *front;
			ULONG ilock;

			scr = NULL;
			front = NULL;
			if(gwin->win != NULL)
				scr = gwin->win->WScreen;
			ilock = LockIBase(0);
			if(scr != NULL)
				front = scr->FirstWindow;
			UnlockIBase(ilock);
			if(front == gwin->win) {
				WindowToBack(gwin->win);
				gwin->chrome_depth_back = true;
			} else {
				WindowToFront(gwin->win);
				gwin->chrome_depth_back = false;
			}
		}
		break;
	default:
		break;
	}
}

/*
 * True if window dimensions match a target within a small tolerance
 * (borders / Intuition sizing can be off by a pixel or two).
 */
static BOOL ami_gui_nami_dims_near(WORD left, WORD top, WORD width, WORD height,
		WORD tleft, WORD ttop, WORD twidth, WORD theight)
{
	WORD dl;
	WORD dt;
	WORD dw;
	WORD dh;

	dl = left - tleft;
	dt = top - ttop;
	dw = width - twidth;
	dh = height - theight;
	if(dl < 0)
		dl = (WORD)(-dl);
	if(dt < 0)
		dt = (WORD)(-dt);
	if(dw < 0)
		dw = (WORD)(-dw);
	if(dh < 0)
		dh = (WORD)(-dh);
	if(dl > 2 || dt > 2 || dw > 2 || dh > 2)
		return FALSE;
	return TRUE;
}

/*
 * Double-click on the Nami drag strip:
 *  1) Normal size → fill screen below the screen title bar
 *  2) Already below-bar → cover the title bar (true fullscreen)
 *  3) Already true fullscreen → restore the pre-zoom size (if saved)
 */
static void ami_gui_nami_drag_zoom(struct gui_window_2 *gwin)
{
	struct Window *win;
	struct Screen *screen;
	WORD bar_h;
	WORD below_left;
	WORD below_top;
	WORD below_w;
	WORD below_h;
	WORD full_left;
	WORD full_top;
	WORD full_w;
	WORD full_h;
	BOOL at_below;
	BOOL at_full;

	if(gwin == NULL || gwin->win == NULL)
		return;

	win = gwin->win;
	screen = win->WScreen;
	if(screen == NULL)
		return;

	bar_h = (WORD)screen->BarHeight;
	if(bar_h < 1)
		bar_h = 11; /* fallback if screen bar metrics are unset */

	below_left = 0;
	below_top = bar_h;
	below_w = screen->Width;
	below_h = (WORD)(screen->Height - bar_h);
	if(below_h < 1)
		below_h = screen->Height;

	full_left = 0;
	full_top = 0;
	full_w = screen->Width;
	full_h = screen->Height;

	at_below = ami_gui_nami_dims_near(win->LeftEdge, win->TopEdge,
			win->Width, win->Height,
			below_left, below_top, below_w, below_h);
	at_full = ami_gui_nami_dims_near(win->LeftEdge, win->TopEdge,
			win->Width, win->Height,
			full_left, full_top, full_w, full_h);

	if(at_full) {
		/* Third step / already covering the bar: restore if we have a box */
		if(gwin->chrome_zoom_have_rest) {
			ChangeWindowBox(win,
					gwin->chrome_zoom_rest.Left,
					gwin->chrome_zoom_rest.Top,
					gwin->chrome_zoom_rest.Width,
					gwin->chrome_zoom_rest.Height);
			gwin->chrome_zoom_have_rest = false;
		} else {
			ChangeWindowBox(win, below_left, below_top, below_w, below_h);
		}
		return;
	}

	if(at_below) {
		/* Already filling below the bar — escalate to cover the title bar */
		ChangeWindowBox(win, full_left, full_top, full_w, full_h);
		return;
	}

	/* First zoom: remember current box, then fill below the screen bar */
	gwin->chrome_zoom_rest.Left = win->LeftEdge;
	gwin->chrome_zoom_rest.Top = win->TopEdge;
	gwin->chrome_zoom_rest.Width = win->Width;
	gwin->chrome_zoom_rest.Height = win->Height;
	gwin->chrome_zoom_have_rest = true;
	ChangeWindowBox(win, below_left, below_top, below_w, below_h);
}

/*
 * Nami chrome SELECTDOWN: arm sysi zoom/depth SpaceObjs, or start drag.
 * Double-click on the drag strip runs ami_gui_nami_drag_zoom instead.
 * Close stays ButtonObj (GADGETUP).  Toolbar hits are never drag.
 */
static BOOL ami_gui_nami_chrome_down(struct gui_window_2 *gwin)
{
	WORD mx;
	WORD my;
	struct timeval curtime;

	if(gwin == NULL || gwin->ui_nami == false || gwin->win == NULL)
		return FALSE;

	mx = gwin->win->MouseX;
	my = gwin->win->MouseY;

	/* In-layout SpaceObjs only.  Border zoom/depth arrive as GADGETUP. */
	if(gwin->nami_border_depth == false &&
	   gwin->objects[GID_WIN_ZOOM] != NULL &&
	   ami_gadget_hit(gwin->objects[GID_WIN_ZOOM], mx, my)) {
		gwin->chrome_armed = GID_WIN_ZOOM;
		ami_gui_nami_chrome_refresh(gwin, GID_WIN_ZOOM);
		return TRUE;
	}
	if(gwin->nami_border_depth == false &&
	   gwin->objects[GID_WIN_DEPTH] != NULL &&
	   ami_gadget_hit(gwin->objects[GID_WIN_DEPTH], mx, my)) {
		gwin->chrome_armed = GID_WIN_DEPTH;
		ami_gui_nami_chrome_refresh(gwin, GID_WIN_DEPTH);
		return TRUE;
	}

	if(gwin->objects[GID_WIN_DRAG] != NULL &&
	   ami_gadget_hit(gwin->objects[GID_WIN_DRAG], mx, my)) {
		CurrentTime((ULONG *)&curtime.tv_sec, (ULONG *)&curtime.tv_usec);
		if(gwin->chrome_drag_click.tv_sec != 0 &&
		   DoubleClick(gwin->chrome_drag_click.tv_sec,
				gwin->chrome_drag_click.tv_usec,
				curtime.tv_sec, curtime.tv_usec)) {
			/* Second click of a double-click: zoom, do not drag */
			gwin->chrome_drag_click.tv_sec = 0;
			gwin->chrome_drag_click.tv_usec = 0;
			gwin->chrome_dragging = false;
			ami_gui_nami_drag_zoom(gwin);
			return TRUE;
		}
		gwin->chrome_drag_click.tv_sec = curtime.tv_sec;
		gwin->chrome_drag_click.tv_usec = curtime.tv_usec;
		gwin->chrome_dragging = true;
		gwin->chrome_drag_offx = mx;
		gwin->chrome_drag_offy = my;
		gwin->chrome_drag_orig_left = gwin->win->LeftEdge;
		gwin->chrome_drag_orig_top = gwin->win->TopEdge;
		return TRUE;
	}
	return FALSE;
}

/* Nami chrome SELECTUP: activate if still over armed gadget. */
static BOOL ami_gui_nami_chrome_up(struct gui_window_2 *gwin, BOOL *win_closed)
{
	WORD mx;
	WORD my;
	WORD armed;
	BOOL was_dragging;
	WORD dleft;
	WORD dtop;

	if(gwin == NULL || gwin->ui_nami == false)
		return FALSE;

	was_dragging = gwin->chrome_dragging;
	gwin->chrome_dragging = false;

	/*
	 * If the window was moved while dragging, cancel a pending double-click
	 * and drop any saved zoom restore (user chose a new placement).
	 */
	if(was_dragging && gwin->win != NULL) {
		dleft = (WORD)(gwin->win->LeftEdge - gwin->chrome_drag_orig_left);
		dtop = (WORD)(gwin->win->TopEdge - gwin->chrome_drag_orig_top);
		if(dleft < 0)
			dleft = (WORD)(-dleft);
		if(dtop < 0)
			dtop = (WORD)(-dtop);
		if(dleft > 2 || dtop > 2) {
			gwin->chrome_drag_click.tv_sec = 0;
			gwin->chrome_drag_click.tv_usec = 0;
			gwin->chrome_zoom_have_rest = false;
		}
	}

	armed = gwin->chrome_armed;
	if(armed == 0)
		return was_dragging;

	mx = gwin->win->MouseX;
	my = gwin->win->MouseY;
	gwin->chrome_armed = 0;
	ami_gui_nami_chrome_refresh(gwin, armed);

	if(gwin->objects[armed] != NULL &&
	   ami_gadget_hit(gwin->objects[armed], mx, my)) {
		ami_gui_nami_chrome_activate(gwin, armed, win_closed);
	}
	return TRUE;
}

/*
 * Tab close image for clicktab: use the theme's grey/disabled glyph.
 * Avoid AllocBitMap snapshots wrapped in BitMapObj — that path has been a
 * reliable Exec MemList corrupt on exit under Hyperion bitmap.image.
 */
static Object *ami_gui_make_tab_close_image(struct Screen *scrn,
		const char *grey_file)
{
	Object *bmo;

	(void)scrn;
	if(grey_file == NULL || grey_file[0] == '\0')
		return NULL;

	bmo = BitMapObj,
		BITMAP_SourceFile, grey_file,
		BITMAP_Screen, scrn,
		BITMAP_Masking, TRUE,
	BitMapEnd;
	return bmo;
}

static bool ami_gui_map_filename(char **remapped, const char *restrict path,
		const char *restrict file, const char *restrict map)
{
	BPTR fh = 0;
	char *mapfile = NULL;
	size_t mapfile_size = 0;
	char buffer[1024];
	char *restrict realfname;
	bool found = false;

	netsurf_mkpath(&mapfile, &mapfile_size, 2, path, map);

	if(mapfile == NULL) return false;

	fh = FOpen(mapfile, MODE_OLDFILE, 0);
	if(fh)
	{
		while(FGets(fh, buffer, 1024) != 0)
		{
			if((buffer[0] == '#') ||
				(buffer[0] == '\n') ||
				(buffer[0] == '\0')) continue;

			realfname = strchr(buffer, ':');
			if(realfname)
			{
				if(strncmp(buffer, file, strlen(file)) == 0)
				{
					if(realfname[strlen(realfname)-1] == '\n')
						realfname[strlen(realfname)-1] = '\0';
					*remapped = strdup(realfname + 1);
					found = true;
					break;
				}
			}
		}
		FClose(fh);
	}

	if(found == false) *remapped = strdup(file);
		else NSLOG(netsurf, INFO,
			   "Remapped %s to %s in path %s using %s", file,
			   *remapped, path, map);

	free(mapfile);

	return found;
}

static bool ami_gui_check_resource(char *fullpath, const char *file)
{
	bool found = false;
	char *remapped;
	BPTR lock = 0;
	size_t fullpath_len = 1024;

	ami_gui_map_filename(&remapped, fullpath, file, "Resource.map");
	netsurf_mkpath(&fullpath, &fullpath_len, 2, fullpath, remapped);

	lock = Lock(fullpath, ACCESS_READ);
	if(lock) {
		UnLock(lock);
		found = true;
	}

	if(found) NSLOG(netsurf, INFO, "Found %s", fullpath);
	free(remapped);

	return found;
}

bool ami_locate_resource(char *fullpath, const char *file)
{
	struct Locale *locale;
	int i;
	bool found = false;
	char *remapped = NULL;
	size_t fullpath_len = 1024;

	/* Check NetSurf user data area first */

	if(current_user_dir != NULL) {
		strcpy(fullpath, current_user_dir);
		found = ami_gui_check_resource(fullpath, file);
		if(found) return true;
	}

	/* Check current theme directory */
	if(nsoption_charp(theme)) {
		strcpy(fullpath, nsoption_charp(theme));
		found = ami_gui_check_resource(fullpath, file);
		if(found) return true;
	}

	/* If not found, start on the user's preferred languages */

	locale = OpenLocale(NULL);

	for(i=0;i<10;i++) {
		strcpy(fullpath, "PROGDIR:Resources/");

		if(locale->loc_PrefLanguages[i]) {
			if(ami_gui_map_filename(&remapped, "PROGDIR:Resources",
					locale->loc_PrefLanguages[i], "LangNames") == true) {
				netsurf_mkpath(&fullpath, &fullpath_len, 2, fullpath, remapped);
				found = ami_gui_check_resource(fullpath, file);
				free(remapped);
			}
		} else {
			continue;
		}

		if(found) break;
	}

	if(!found) {
		/* If not found yet, check in PROGDIR:Resources/en,
		 * might not be in user's preferred languages */

		strcpy(fullpath, "PROGDIR:Resources/en/");
		found = ami_gui_check_resource(fullpath, file);
	}

	CloseLocale(locale);

	if(!found) {
		/* Lastly check directly in PROGDIR:Resources */

		strcpy(fullpath, "PROGDIR:Resources/");
		found = ami_gui_check_resource(fullpath, file);
	}

	return found;
}

static void ami_gui_resources_free(void)
{
	ami_schedule_free();
	ami_object_fini();

	FreeSysObject(ASOT_PORT, appport);
	FreeSysObject(ASOT_PORT, sport);
	FreeSysObject(ASOT_PORT, schedulermsgport);
}

static bool ami_gui_resources_open(void)
{
#ifdef __amigaos4__
	urlStringClass = MakeStringClass();
#endif

    if(!(appport = AllocSysObjectTags(ASOT_PORT,
							ASO_NoTrack, FALSE,
							TAG_DONE))) return false;

    if(!(sport = AllocSysObjectTags(ASOT_PORT,
							ASO_NoTrack, FALSE,
							TAG_DONE))) return false;

    if(!(schedulermsgport = AllocSysObjectTags(ASOT_PORT,
							ASO_NoTrack, FALSE,
							TAG_DONE))) return false;

	if(ami_schedule_create(schedulermsgport) != NSERROR_OK) {
		ami_misc_fatal_error("Failed to initialise scheduler");
		return false;
	}

	ami_object_init();

	return true;
}

static UWORD ami_system_colour_scrollbar_fgpen(struct DrawInfo *drinfo)
{
	LONG scrollerfillpen = FALSE;
#ifdef __amigaos4__
	GetGUIAttrs(NULL, drinfo, GUIA_PropKnobColor, &scrollerfillpen, TAG_DONE);

	if(scrollerfillpen) return FILLPEN;
		else return FOREGROUNDPEN;
#else
	return FILLPEN;
#endif

}

#ifdef __amigaos4__

/**
 * convert an amiga pen to a netsurf colour
 */
static colour
nscolour_from_pen(struct Screen *screen, UWORD pen, colour sel_colour)
{
	ULONG colr[3];
	struct DrawInfo *drinfo;

	drinfo = GetScreenDrawInfo(screen);

	if (drinfo != NULL) {
		if (pen == AMINS_SCROLLERPEN) {
			pen = ami_system_colour_scrollbar_fgpen(drinfo);
		}

		/* Get the colour of the pen being used for "pen" */
		GetRGB32(screen->ViewPort.ColorMap,
			 drinfo->dri_Pens[pen],
			 1,
			 (ULONG *)&colr);

		/* convert it to a color */
		sel_colour = ((colr[0] & 0xff000000) >> 24) |
			((colr[1] & 0xff000000) >> 16) |
			((colr[2] & 0xff000000) >> 8);

		FreeScreenDrawInfo(screen, drinfo);
	}
	return sel_colour;
}


/**
 * set system colour options from amiga pen
 */
static nserror system_colours_from_pen(struct Screen *screen)
{
        #define MAP_SIZE (19)
	int mapidx;
	colour sel_colour;
	struct pcm {
		enum nsoption_e option;
		UWORD pen;
		colour def_colour;
	};
	struct pcm pen_colour_map[MAP_SIZE] = {
		{NSOPTION_sys_colour_AccentColor, FILLPEN, 0x00000000},
		{NSOPTION_sys_colour_AccentColorText, TEXTPEN, 0x00000000},
		{NSOPTION_sys_colour_ActiveText, TEXTPEN, 0x00000000},
		{NSOPTION_sys_colour_ButtonBorder, FILLPEN, 0x00000000},
		{NSOPTION_sys_colour_ButtonFace, FOREGROUNDPEN, 0x00aaaaaa},
		{NSOPTION_sys_colour_ButtonText, TEXTPEN, 0x00000000},
		{NSOPTION_sys_colour_Canvas, BACKGROUNDPEN, 0x00aaaaaa},
		{NSOPTION_sys_colour_CanvasText, TEXTPEN, 0x00000000},
		{NSOPTION_sys_colour_Field, BACKGROUNDPEN, 0x00aaaaaa},
		{NSOPTION_sys_colour_FieldText, TEXTPEN, 0x00000000},
		{NSOPTION_sys_colour_GrayText, DISABLEDTEXTPEN, 0x00777777},
		{NSOPTION_sys_colour_Highlight, SELECTPEN, 0x00ee0000},
		{NSOPTION_sys_colour_HighlightText, SELECTTEXTPEN, 0x00000000},
		{NSOPTION_sys_colour_LinkText, TEXTPEN, 0x00000000},
		{NSOPTION_sys_colour_Mark, FILLPEN, 0x00000000},
		{NSOPTION_sys_colour_MarkText, TEXTPEN, 0x00000000},
		{NSOPTION_sys_colour_SelectedItem, FILLPEN, 0x00000000},
		{NSOPTION_sys_colour_SelectedItemText, TEXTPEN, 0x00000000},
		{NSOPTION_sys_colour_VisitedText, TEXTPEN, 0x00000000},
	};
	struct pcm *entry;

	for (mapidx=0; mapidx < MAP_SIZE; mapidx++) {
		entry = &pen_colour_map[mapidx];
		sel_colour = nscolour_from_pen(screen, entry->pen, entry->def_colour);

		if (nsoptions_default[entry->option].value.c == nsoptions[entry->option].value.c) {
			nsoptions[entry->option].value.c = sel_colour;
		}
		nsoptions_default[entry->option].value.c = sel_colour;
	}

	return NSERROR_OK;
}

#endif /* __amigaos4__ */

/* exported interface documented in amiga/gui.h */
STRPTR ami_gui_get_screen_title(void)
{
	if(nsscreentitle == NULL) {
		nsscreentitle = ASPrintf("Nami %s", netsurf_version);
		/* If this fails it will be NULL, which means we'll get the screen's
		 * default titlebar text instead - so no need to check for error. */
	}

	return nsscreentitle;
}

/**
 * Format a memory size for the screen title (KB / MB).
 */
static void ami_gui_memsize_fmt(ULONG bytes, char *out, size_t outlen)
{
	ULONG n;
	const char *unit;

	if(bytes >= (1024UL * 1024UL)) {
		n = bytes / (1024UL * 1024UL);
		unit = "MB";
	} else {
		n = (bytes + 1023UL) / 1024UL;
		unit = "KB";
	}
	snprintf(out, outlen, "%lu%s", (unsigned long)n, unit);
}

/**
 * scheme://host for the active tab, or "-" if unknown.
 */
static void ami_gui_doc_origin(struct gui_window_2 *gwin, char *out, size_t outlen)
{
	nsurl *url = NULL;
	lwc_string *scheme = NULL;
	lwc_string *host = NULL;
	const char *full;

	out[0] = '-';
	out[1] = '\0';
	if(gwin == NULL || gwin->gw == NULL || gwin->gw->bw == NULL)
		return;
	if(browser_window_get_url(gwin->gw->bw, false, &url) != NSERROR_OK)
		return;
	if(url == NULL)
		return;

	scheme = nsurl_get_component(url, NSURL_SCHEME);
	host = nsurl_get_component(url, NSURL_HOST);
	if(scheme != NULL && host != NULL) {
		snprintf(out, outlen, "%s://%s",
			lwc_string_data(scheme), lwc_string_data(host));
	} else {
		full = nsurl_access(url);
		if(full != NULL) {
			strncpy(out, full, outlen - 1);
			out[outlen - 1] = '\0';
		}
	}
	if(scheme != NULL)
		lwc_string_unref(scheme);
	if(host != NULL)
		lwc_string_unref(host);
	nsurl_unref(url);
}

/**
 * Build normal Nami screen-title telemetry into ami_screentitle_buf.
 * Format: Nami <ver>  chip <n> other <n>  <origin>  <a>/<q>
 */
static void ami_gui_make_screentitle(struct gui_window_2 *gwin)
{
	char chipbuf[16];
	char otherbuf[16];
	char origin[96];
	char connbuf[32];
	ULONG chip;
	ULONG other;
	ULONG active;
	ULONG queued;

	chip = AvailMem(MEMF_CHIP);
	other = AvailMem(MEMF_FAST);
	if(other == 0)
		other = AvailMem(MEMF_ANY);
	ami_gui_memsize_fmt(chip, chipbuf, sizeof(chipbuf));
	ami_gui_memsize_fmt(other, otherbuf, sizeof(otherbuf));
	ami_gui_doc_origin(gwin, origin, sizeof(origin));

	active = 0;
	queued = 0;
#ifdef WITH_AMIHTTP
	fetch_amihttp_counts(&active, &queued);
#endif
	snprintf(connbuf, sizeof(connbuf), "%lu/%lu",
		(unsigned long)active, (unsigned long)queued);

#ifdef __amigaos4__
	/* OS4 omits chip; Fast/other availability is the useful figure */
	snprintf(ami_screentitle_buf, sizeof(ami_screentitle_buf),
		"Nami %s  other %s  %s  %s",
		netsurf_version, otherbuf, origin, connbuf);
#else
	snprintf(ami_screentitle_buf, sizeof(ami_screentitle_buf),
		"Nami %s  chip %s other %s  %s  %s",
		netsurf_version, chipbuf, otherbuf, origin, connbuf);
#endif
}

/**
 * Temporary status overlay for the screen title (modern/Nami layout).
 */
/**
 * Rewrite TBImages:name → TBImages:list_name for AISS 16×16 list glyphs.
 * No-op if the path is not a TBImages: reference or already uses list_.
 */
static void ami_gui_prefer_list_tbimage(char *path)
{
	char *tb;
	char name[100];
	size_t prefix_len;
	size_t name_len;

	if(path == NULL || path[0] == '\0')
		return;

	tb = strstr(path, "TBImages:");
	if(tb == NULL)
		tb = strstr(path, "TBimages:");
	if(tb == NULL)
		return;

	prefix_len = (size_t)(tb - path) + 9; /* include TBImages: */
	if(strncmp(path + prefix_len, "list_", 5) == 0)
		return;

	name_len = strlen(path + prefix_len);
	if(name_len == 0 || name_len + 5 >= sizeof(name))
		return;

	strcpy(name, path + prefix_len);
	path[prefix_len] = '\0';
	strcat(path, "list_");
	strcat(path, name);
}

static void ami_gui_make_status_screentitle(struct gui_window_2 *gwin)
{
	const char *st;

	st = gwin->status;
	if(st == NULL)
		st = "";
	snprintf(ami_screentitle_buf, sizeof(ami_screentitle_buf),
		"Nami %s  %s", netsurf_version, st);
}

/**
 * Apply dynamic screen title while a Nami window is active.
 * Uses SetWindowTitles(win, ~0, stitle) so the window drag-bar title is untouched.
 */
static void ami_gui_update_screentitle(struct gui_window_2 *gwin)
{
	ULONG secs;
	ULONG micros;
	BOOL showstatus;
	char *copy;

	if(gwin == NULL || gwin->win == NULL || gwin->ui_nami == false)
		return;

	CurrentTime(&secs, &micros);

	showstatus = FALSE;
	if(gwin->status_screentime != 0 &&
	   (secs - gwin->status_screentime) < AMI_SCREENTITLE_STATUS_SECS &&
	   gwin->status != NULL && gwin->status[0] != '\0') {
		showstatus = TRUE;
	} else if(gwin->status_screentime != 0 &&
		  (secs - gwin->status_screentime) >= AMI_SCREENTITLE_STATUS_SECS) {
		gwin->status_screentime = 0;
	}

	/* Throttle normal telemetry to about once per second */
	if(showstatus == FALSE) {
		if(gwin->screentitletime == secs &&
		   ami_last_screentitle_gwin == gwin)
			return;
	}

	if(showstatus)
		ami_gui_make_status_screentitle(gwin);
	else
		ami_gui_make_screentitle(gwin);

	if(ami_last_screentitle_gwin != gwin ||
	   gwin->screentitle == NULL ||
	   strcmp(gwin->screentitle, ami_screentitle_buf) != 0) {
		SetWindowTitles(gwin->win, (STRPTR)~0, ami_screentitle_buf);
		if(gwin->screentitle)
			free(gwin->screentitle);
		copy = strdup(ami_screentitle_buf);
		gwin->screentitle = copy;
		ami_last_screentitle_gwin = gwin;
	}

	if(showstatus == FALSE)
		gwin->screentitletime = secs;
}

static void ami_set_screen_defaults(struct Screen *screen)
{
#ifdef __amigaos4__
	nsoption_default_set_int(redraw_tile_size_x, screen->Width);
	nsoption_default_set_int(redraw_tile_size_y, screen->Height);

	/* set system colours for amiga ui */
	system_colours_from_pen(screen);
#else
	/*
	 * OS3: redraw_tile_size 0 = no tiling (buffer ≥ screen; see guide).
	 * Plenty of Fast is assumed; Chip is only a small TmpRas in
	 * ami_plot_ra_alloc. Force 0 so an old Choices file cannot leave
	 * 160×160 tiles that break large-image blits.
	 */
	{
		nsoption_default_set_int(redraw_tile_size_x, 0);
		nsoption_default_set_int(redraw_tile_size_y, 0);
		nsoption_set_int(redraw_tile_size_x, 0);
		nsoption_set_int(redraw_tile_size_y, 0);
		NSLOG(netsurf, INFO,
		      "OS3 redraw_tile_size 0 (screen %dx%d depth %lu, no tiling)",
		      screen->Width, screen->Height,
		      (unsigned long)(screen->RastPort.BitMap != NULL
				? GetBitMapAttr(screen->RastPort.BitMap, BMA_DEPTH)
				: 8UL));
	}
#endif
}


/**
 * Set option defaults for amiga frontend
 *
 * @param defaults The option table to update.
 * @return error status.
 */
static nserror ami_set_options(struct nsoption_s *defaults)
{
	STRPTR tempacceptlangs;
	char temp[1024];
	int codeset = 0;

	/* The following line disables the popupmenu.class select menu.
	** It's not recommended to use it!
	*/
	nsoption_set_bool(core_select_menu, true);

	/* ClickTab < 53 doesn't work with the auto show/hide tab-bar (for reasons forgotten) */
	if(ClickTabBase->lib_Version < 53)
		nsoption_set_bool(tab_always_show, true);

	if((!nsoption_charp(accept_language)) || 
	   (nsoption_charp(accept_language)[0] == '\0') ||
	   (nsoption_bool(accept_lang_locale) == true))
	{
		if((tempacceptlangs = ami_locale_langs(&codeset)))
		{
			nsoption_set_charp(accept_language,
					   (char *)strdup(tempacceptlangs));
			FreeVec(tempacceptlangs);
		}
	}

	/* Some OS-specific overrides */
#ifdef __amigaos4__
	if(!LIB_IS_AT_LEAST((struct Library *)SysBase, 53, 89)) {
		/* Disable ExtMem usage pre-OS4.1FEU1 */
		nsoption_set_bool(use_extmem, false);
	}

	if(codeset == 0) codeset = 4; /* ISO-8859-1 */
	const char *encname = (const char *)ObtainCharsetInfo(DFCS_NUMBER, codeset,
							DFCS_MIMENAME);
	nsoption_set_charp(local_charset, strdup(encname));
	nsoption_set_int(local_codeset, codeset);
#else
	nsoption_set_bool(download_notify, false);
	nsoption_set_bool(font_antialiasing, false);
	nsoption_set_bool(truecolour_mouse_pointers, false);
	nsoption_set_bool(os_mouse_pointers, true);
	nsoption_set_bool(use_openurl_lib, true);
	/*
	 * Prefer Compugraphic outline fonts via bullet.library. Bitmap
	 * Topaz/Helvetica look poor for web text; users can still force
	 * diskfont via font_engine / bitmap_fonts in prefs.
	 */
	nsoption_set_bool(bitmap_fonts, false);
	/*
	 * fs_backing_store uses POSIX open/mkdir and '/' paths. On OS3 that
	 * path crashes during hlcache init right after resource mapping.
	 * Disable disc cache (same approach as the RISC OS frontend).
	 */
	nsoption_set_uint(disc_cache_size, 0);
	/*
	 * Desktop defaults (12MB cache, 24 fetchers) are hostile on classic
	 * Amiga RAM. Other browsers render amigans.net in under 2MB chip;
	 * keep Fast budgets modest so image decode does not lock the machine.
	 */
	nsoption_set_int(memory_cache_size, 768 * 1024);
	nsoption_set_int(max_fetchers, 8);
	nsoption_set_int(max_fetchers_per_host, 2);
	nsoption_set_int(max_cached_fetch_handles, 2);
	/*
	 * Keep Remapped native BitMaps (All). Scaled plots BitScale from the
	 * full-size cache instead of re-running picture.datatype PROCLAYOUT.
	 */
	nsoption_set_int(cache_bitmaps, 2);
#endif

	/* Always Preferences / WA_PointerType — no theme custom pointers. */
	nsoption_set_bool(truecolour_mouse_pointers, false);
	nsoption_set_bool(os_mouse_pointers, true);

	sprintf(temp, "%s/Cookies", current_user_dir);
	nsoption_setnull_charp(cookie_file, 
			       (char *)strdup(temp));

	sprintf(temp, "%s/Hotlist", current_user_dir);
	nsoption_setnull_charp(hotlist_file, 
			       (char *)strdup(temp));

	sprintf(temp, "%s/URLdb", current_user_dir);
	nsoption_setnull_charp(url_file,
			       (char *)strdup(temp));

	sprintf(temp, "%s/FontGlyphCache", current_user_dir);
	nsoption_setnull_charp(font_unicode_file,
			       (char *)strdup(temp));

	/* AmiHTTP TLS verify: prefer shared CA bundle when present */
	{
		BPTR ca_lock;

		ca_lock = Lock("AWeb:Certs/cacert.pem", ACCESS_READ);
		if (ca_lock != 0) {
			UnLock(ca_lock);
			nsoption_setnull_charp(ca_bundle,
					       (char *)strdup("AWeb:Certs/cacert.pem"));
		} else {
			nsoption_setnull_charp(ca_bundle,
					       (char *)strdup("PROGDIR:Resources/ca-bundle"));
		}
		NSLOG(netsurf, INFO, "ca_bundle=%s",
		      nsoption_charp(ca_bundle) ? nsoption_charp(ca_bundle) : "(null)");
	}

	/* Regular font defaults are set in font.c */

	if (nsoption_charp(font_unicode) == NULL)
	{
		BPTR lock = 0;
		/* Search for some likely candidates */

		if((lock = Lock("FONTS:Code2000.otag", ACCESS_READ)))
		{
			UnLock(lock);
			nsoption_set_charp(font_unicode, 
					   (char *)strdup("Code2000"));
		}
		else if((lock = Lock("FONTS:Bitstream Cyberbit.otag", ACCESS_READ)))
		{
			UnLock(lock);
			nsoption_set_charp(font_unicode,
					   (char *)strdup("Bitstream Cyberbit"));
		}
	}

	if (nsoption_charp(font_surrogate) == NULL) {
		BPTR lock = 0;
		/* Search for some likely candidates -
		 * Ideally we should pick a font during the scan process which announces it
		 * contains UCR_SURROGATES, but nothing appears to have the tag.
		 */
		if((lock = Lock("FONTS:Symbola.otag", ACCESS_READ))) {
			UnLock(lock);
			nsoption_set_charp(font_surrogate, 
					   (char *)strdup("Symbola"));
		}
	}

	return NSERROR_OK;
}

static void ami_amiupdate(void)
{
	/* Create AppPath location for AmiUpdate use */

	BPTR lock = 0;

	if(((lock = Lock("ENVARC:AppPaths",SHARED_LOCK)) == 0))
	{
		lock = CreateDir("ENVARC:AppPaths");
	}
	
	UnLock(lock);

	if((lock = Lock("PROGDIR:", ACCESS_READ)))
	{
		char filename[1024];
		BPTR amiupdatefh;

		DevNameFromLock(lock, (STRPTR)&filename, 1024L, DN_FULLPATH);

		if((amiupdatefh = FOpen("ENVARC:AppPaths/NetSurf", MODE_NEWFILE, 0))) {
			FPuts(amiupdatefh, (CONST_STRPTR)&filename);
			FClose(amiupdatefh);
		}

		UnLock(lock);
	}
}

static nsurl *gui_get_resource_url(const char *path)
{
	char buf[1024];
	nsurl *url = NULL;

	if(ami_locate_resource(buf, path) == false)
		return NULL;

	netsurf_path_to_nsurl(buf, &url);

	return url;
}

HOOKF(void, ami_gui_newprefs_hook, APTR, window, APTR)
{
	ami_set_screen_defaults(scrn);
}

static void ami_openscreen(void)
{
	ULONG id = 0;
	ULONG compositing;
	const char *pub;

	if (nsoption_int(screen_compositing) == -1)
		compositing = ~0UL;
	else compositing = nsoption_int(screen_compositing);

	pub = nsoption_charp(pubscreen_name);

	/*
	 * Own NetSurf public screen only when the user has chosen that in
	 * prefs (pubscreen_name left empty AND a screen_modeid stored).
	 * First run / no Choices: never ASL — use Workbench.
	 */
	if (pub == NULL || pub[0] == '\0') {
		if ((nsoption_charp(screen_modeid) != NULL) &&
		    (strncmp(nsoption_charp(screen_modeid), "0x", 2) == 0)) {
			id = strtoul(nsoption_charp(screen_modeid), NULL, 0);

			if (screen_signal == -1) screen_signal = AllocSignal(-1);
			NSLOG(netsurf, INFO, "Screen signal %d", screen_signal);
			scrn = OpenScreenTags(NULL,
						SA_DisplayID, id,
						SA_Title, ami_gui_get_screen_title(),
						SA_Type, PUBLICSCREEN,
						SA_PubName, "Nami",
						SA_PubSig, screen_signal,
						SA_PubTask, FindTask(0),
						SA_LikeWorkbench, TRUE,
						SA_SharePens, TRUE,
						SA_Compositing, compositing,
						TAG_DONE);

			if (scrn) {
				PubScreenStatus(scrn, 0);
			} else {
				FreeSignal(screen_signal);
				screen_signal = -1;

				if ((scrn = LockPubScreen("Nami"))) {
					locked_screen = TRUE;
				} else {
					nsoption_set_charp(pubscreen_name,
							   strdup("Workbench"));
				}
			}
		} else {
			NSLOG(netsurf, INFO,
			      "No pubscreen/mode configured — using Workbench");
			nsoption_set_charp(pubscreen_name, strdup("Workbench"));
		}
	}

	if (nsoption_charp(pubscreen_name) != NULL) {
		scrn = LockPubScreen(nsoption_charp(pubscreen_name));

		if (scrn == NULL) {
			scrn = LockPubScreen("Workbench");
		}
		locked_screen = TRUE;
	}

	ami_font_setdevicedpi(id);
	ami_set_screen_defaults(scrn);
	ami_help_new_screen(scrn);
}

static void ami_openscreenfirst(void)
{
	NSLOG(netsurf, INFO, "ami_openscreenfirst: opening screen");
	ami_openscreen();
	NSLOG(netsurf, INFO, "ami_openscreenfirst: screen ready, alloc plot rastport");
	if(browserglob == NULL) browserglob = ami_plot_ra_alloc(0, 0, false, false);
	NSLOG(netsurf, INFO, "ami_openscreenfirst: plot rastport %p, throbber setup",
	      (void *)browserglob);
	ami_theme_throbber_setup();
	NSLOG(netsurf, INFO, "ami_openscreenfirst: done");
}

/**
 * True if this argv word must be quoted for ReadArgs.
 *
 * Quoted items are never matched as /S or /K keywords — so NAMI, NETSURF,
 * and USERSDIR=… must stay unquoted.  Quote when whitespace or ReadItem
 * delimiters would split the token (space, ';', quotes, '*', or '=' that
 * is not a known KEYWORD=value).
 */
static int ami_gui_rdargs_needs_quotes(const char *arg)
{
	const char *p;
	const char *eq;

	if(arg == NULL || *arg == '\0')
		return 1;

	/* Bare template keywords / switches / help — never quote */
	if(strcasecmp(arg, "NAMI") == 0 ||
	   strcasecmp(arg, "NETSURF") == 0 ||
	   strcasecmp(arg, "URL") == 0 ||
	   strcasecmp(arg, "USER") == 0 ||
	   strcasecmp(arg, "USERSDIR") == 0 ||
	   strcmp(arg, "?") == 0) {
		return 0;
	}

	eq = strchr(arg, '=');
	if(eq != NULL && eq > arg) {
		/* USER=… / USERSDIR=… stay unquoted so /K matches */
		if((eq - arg == 4 && strncasecmp(arg, "USER", 4) == 0) ||
		   (eq - arg == 8 && strncasecmp(arg, "USERSDIR", 8) == 0)) {
			/* still quote if value has whitespace */
			for(p = eq + 1; *p != '\0'; p++) {
				if(*p == ' ' || *p == '\t' || *p == '"' ||
				   *p == '*' || *p == ';' || *p == '\n')
					return 1;
			}
			return 0;
		}
		/* URL query or other '=' — quote so ReadItem does not split */
		return 1;
	}

	for(p = arg; *p != '\0'; p++) {
		if(*p == ' ' || *p == '\t' || *p == '"' ||
		   *p == '*' || *p == ';' || *p == '\n')
			return 1;
	}
	return 0;
}

/**
 * Append one argv word to a ReadArgs RDA_Source buffer.
 * Quotes only when needed (see ami_gui_rdargs_needs_quotes).
 * Inside quotes, '*' escapes '*' and '"'.
 */
static int ami_gui_rdargs_append_arg(char *buf, int buflen, int used,
		const char *arg)
{
	int need;
	int quote;
	const char *p;
	char *out;

	if(buf == NULL || arg == NULL || used < 0 || used >= buflen)
		return -1;

	quote = ami_gui_rdargs_needs_quotes(arg);

	need = used;
	if(used > 0)
		need++; /* separating space */
	if(quote)
		need += 2;
	for(p = arg; *p != '\0'; p++) {
		need++;
		if(quote && (*p == '*' || *p == '"'))
			need++;
	}
	need++; /* NUL */
	if(need > buflen)
		return -1;

	out = buf + used;
	if(used > 0) {
		*out++ = ' ';
		used++;
	}
	if(quote) {
		*out++ = '"';
		used++;
	}
	for(p = arg; *p != '\0'; p++) {
		if(quote && (*p == '*' || *p == '"')) {
			*out++ = '*';
			used++;
		}
		*out++ = *p;
		used++;
	}
	if(quote) {
		*out++ = '"';
		used++;
	}
	*out = '\0';
	return used;
}

/* Set when Shell "Nami ?" printed usage — main exits without opening UI */
static int cli_rdargs_help_done = 0;

/**
 * Parse Shell command line via ReadArgs.
 *
 * Amiga template (dos.library/ReadArgs):
 *   NAMI/S,NETSURF/S,USER=USERSDIR/K,URL/M
 * so e.g.
 *   Nami https://example.com
 *   Nami NAMI https://a.com
 *   Nami NETSURF Work:Docs/file.html
 *   Nami USERSDIR=RAM: Work:Docs/file.html
 *   Nami ?
 *
 * Switches/keywords stay unquoted in the rebuilt line so ReadArgs can
 * match them; URL/M absorbs everything else.  "Nami ?" prints the
 * template (RDAF_NOPROMPT + custom RDA_Source cannot do interactive ?).
 *
 * NAMI/NETSURF force ui_style for this run only (override Prefs).
 *
 * --option=value core options are peeled off first and returned for
 * nsoption_commandline().  ReadArgs is fed a rebuilt argv string
 * (RDA_Source) because Input() has already been consumed by C startup.
 * RDArgs comes from AllocDosObject(DOS_RDARGS) per dos.doc.
 */
static char **ami_gui_commandline(int *restrict argc, char **argv,
		int *restrict nargc, char ***restrict nargv)
{
	struct RDArgs *rdargs;
	struct RDArgs *args;
	/* Switches before /M so keywords match before leftover strings */
	CONST_STRPTR template = "NAMI/S,NETSURF/S,USER=USERSDIR/K,URL/M";
	LONG rarray[4];
	enum {
		A_NAMI = 0,
		A_NETSURF,
		A_USERSDIR,
		A_URL
	};
	char *buf;
	char **urls;
	char **optv;
	int i;
	int len;
	int used;
	int optc;
	int opti;
	int urli;
	int n;
	int want_help;

	rdargs = NULL;
	args = NULL;
	buf = NULL;
	urls = NULL;
	optv = NULL;
	want_help = 0;

	if(nargc != NULL)
		*nargc = 0;
	if(nargv != NULL)
		*nargv = NULL;

	if(argc == NULL || *argc == 0)
		return NULL; /* Workbench: argv is WBStartup * */

	/* Peel GNU-style --options for nsoption_commandline() */
	optc = 1;
	for(i = 1; i < *argc; i++) {
		if(argv[i] != NULL && argv[i][0] == '-' && argv[i][1] == '-')
			optc++;
	}
	if(optc > 1 && nargv != NULL) {
		optv = malloc((size_t)(optc + 1) * sizeof(char *));
		if(optv == NULL)
			return NULL;
		optv[0] = strdup("Nami");
		opti = 1;
		for(i = 1; i < *argc; i++) {
			if(argv[i] != NULL && argv[i][0] == '-' &&
			   argv[i][1] == '-') {
				optv[opti] = strdup(argv[i]);
				opti++;
			}
		}
		optv[opti] = NULL;
		if(nargc != NULL)
			*nargc = optc;
		*nargv = optv;
	}

	/* Shell "Nami ?" — print usage (cannot use ReadArgs ? with RDA_Source) */
	for(i = 1; i < *argc; i++) {
		if(argv[i] == NULL)
			continue;
		if(argv[i][0] == '-' && argv[i][1] == '-')
			continue;
		if(strcmp(argv[i], "?") == 0)
			want_help = 1;
	}
	if(want_help) {
		Printf((STRPTR)"%s\n", (STRPTR)template);
		Printf((STRPTR)"  NAMI/S      Force Nami UI for this launch\n");
		Printf((STRPTR)"  NETSURF/S   Force NetSurf UI for this launch\n");
		Printf((STRPTR)"  USERSDIR/K  Users drawer (USER= alias)\n");
		Printf((STRPTR)"  URL/M       One or more URLs or Amiga paths\n");
		cli_rdargs_help_done = 1;
		return optv;
	}

	/*
	 * Rebuild remaining args for ReadArgs.  Quote only when needed so
	 * NAMI/NETSURF/USERSDIR= stay unquoted keywords.  Buffer must end
	 * with a newline (dos.doc BUGS, V37 and before).
	 */
	len = 4;
	for(i = 1; i < *argc; i++) {
		if(argv[i] == NULL)
			continue;
		if(argv[i][0] == '-' && argv[i][1] == '-')
			continue;
		len += (int)(strlen(argv[i]) * 2) + 4;
	}
	buf = malloc((size_t)len);
	if(buf == NULL)
		return optv;

	buf[0] = '\0';
	used = 0;
	for(i = 1; i < *argc; i++) {
		if(argv[i] == NULL)
			continue;
		if(argv[i][0] == '-' && argv[i][1] == '-')
			continue;
		n = ami_gui_rdargs_append_arg(buf, len, used, argv[i]);
		if(n < 0) {
			NSLOG(netsurf, WARNING,
			      "ReadArgs line buffer overflow");
			free(buf);
			return optv;
		}
		used = n;
	}
	if(used + 2 > len) {
		free(buf);
		return optv;
	}
	buf[used++] = '\n';
	buf[used] = '\0';

	NSLOG(netsurf, INFO, "ReadArgs line: %s", buf);

	/* dos.doc: AllocDosObject must be used when passing an RDArgs in */
	rdargs = AllocDosObject(DOS_RDARGS, NULL);
	if(rdargs == NULL) {
		NSLOG(netsurf, WARNING, "AllocDosObject(DOS_RDARGS) failed");
		free(buf);
		return optv;
	}

	rdargs->RDA_Source.CS_Buffer = buf;
	rdargs->RDA_Source.CS_Length = (LONG)used;
	rdargs->RDA_Source.CS_CurChr = 0;
	rdargs->RDA_DAList = 0;
	rdargs->RDA_Buffer = NULL;
	rdargs->RDA_BufSiz = 0;
	rdargs->RDA_ExtHelp = NULL;
	/* Custom RDA_Source: no interactive ? prompt (handled above) */
	rdargs->RDA_Flags = RDAF_NOPROMPT;

	rarray[A_NAMI] = 0;
	rarray[A_NETSURF] = 0;
	rarray[A_USERSDIR] = 0;
	rarray[A_URL] = 0;

	args = ReadArgs((STRPTR)template, rarray, rdargs);
	if(args == NULL) {
		NSLOG(netsurf, WARNING,
		      "ReadArgs failed (template \"%s\" line \"%s\") IoErr=%ld",
		      template, buf, (long)IoErr());
		PrintFault(IoErr(), (STRPTR)"Nami");
		FreeArgs(rdargs);
		FreeDosObject(DOS_RDARGS, rdargs);
		free(buf);
		return optv;
	}

	if(rarray[A_USERSDIR]) {
		NSLOG(netsurf, INFO, "USERSDIR %s specified on command line",
		      (char *)rarray[A_USERSDIR]);
		if(users_dir != NULL)
			FreeVec(users_dir);
		users_dir = ASPrintf("%s", (char *)rarray[A_USERSDIR]);
	}

	/* Force UI chrome for this launch only (Prefs ui_style unchanged on disk) */
	if(rarray[A_NAMI] != 0 && rarray[A_NETSURF] != 0) {
		NSLOG(netsurf, WARNING,
		      "NAMI and NETSURF both set; using NAMI");
		cli_ui_style_override = 1;
	} else if(rarray[A_NAMI] != 0) {
		cli_ui_style_override = 1;
		NSLOG(netsurf, INFO, "CLI override: Nami UI");
	} else if(rarray[A_NETSURF] != 0) {
		cli_ui_style_override = 0;
		NSLOG(netsurf, INFO, "CLI override: NetSurf UI");
	}

	urls = (char **)rarray[A_URL];
	if(urls != NULL) {
		urli = 0;
		while(urls[urli] != NULL)
			urli++;
		if(urli > 0) {
			cli_urls = malloc((size_t)(urli + 1) * sizeof(char *));
			if(cli_urls != NULL) {
				cli_url_count = 0;
				for(i = 0; i < urli; i++) {
					cli_urls[cli_url_count] = strdup(urls[i]);
					if(cli_urls[cli_url_count] != NULL) {
						NSLOG(netsurf, INFO,
						      "URL[%d] %s",
						      cli_url_count,
						      cli_urls[cli_url_count]);
						cli_url_count++;
					}
				}
				cli_urls[cli_url_count] = NULL;
				/* ARexx re-open path still uses temp_homepage_url */
				if(cli_url_count > 0 && temp_homepage_url == NULL)
					temp_homepage_url = strdup(cli_urls[0]);
			}
		}
	}

	FreeArgs(rdargs);
	FreeDosObject(DOS_RDARGS, rdargs);
	free(buf);
	return optv;
}

static char *ami_gui_read_tooltypes(struct WBArg *wbarg)
{
	struct DiskObject *dobj;
	STRPTR *toolarray;
	char *s;
	char *current_user = NULL;

	if((*wbarg->wa_Name) && (dobj = GetDiskObject(wbarg->wa_Name))) {
		toolarray = (STRPTR *)dobj->do_ToolTypes;

		if((s = (char *)FindToolType(toolarray,"USERSDIR"))) users_dir = ASPrintf("%s", s);
		if((s = (char *)FindToolType(toolarray,"USER"))) current_user = ASPrintf("%s", s);
		/* Force UI for this launch (same as Shell NAMI/NETSURF switches) */
		if(FindToolType(toolarray, "NAMI") != NULL &&
		   FindToolType(toolarray, "NETSURF") != NULL) {
			cli_ui_style_override = 1; /* NAMI wins, as on Shell */
		} else if(FindToolType(toolarray, "NAMI") != NULL) {
			cli_ui_style_override = 1;
		} else if(FindToolType(toolarray, "NETSURF") != NULL) {
			cli_ui_style_override = 0;
		}

		FreeDiskObject(dobj);
	}
	return current_user;
}

static STRPTR ami_gui_read_all_tooltypes(int argc, char **argv)
{
	struct WBStartup *WBenchMsg;
	struct WBArg *wbarg;
	char i = 0;
	char *current_user = NULL;
	char *cur_user = NULL;

	if(argc == 0) { /* Started from WB */
		WBenchMsg = (struct WBStartup *)argv;
		for(i = 0, wbarg = WBenchMsg->sm_ArgList; i < WBenchMsg->sm_NumArgs; i++,wbarg++) {
			LONG olddir =-1;
			if((wbarg->wa_Lock) && (*wbarg->wa_Name))
				olddir = SetCurrentDir(wbarg->wa_Lock);

			cur_user = ami_gui_read_tooltypes(wbarg);
			if(cur_user != NULL) {
				if(current_user != NULL) FreeVec(current_user);
				current_user = cur_user;
			}

			if(olddir !=-1) SetCurrentDir(olddir);
		}
	}

	return current_user;
}

static void gui_init2(int argc, char** argv)
{
	struct Screen *screen;
	BOOL notalreadyrunning;
	nsurl *url;
	nserror error;
	struct browser_window *bw = NULL;

	notalreadyrunning = ami_arexx_init(&rxsig);

	/* ...and this ensures the treeview at least gets the WB colour palette to work with */
	if(scrn == NULL) {
		if((screen = LockPubScreen("Workbench"))) {
			ami_set_screen_defaults(screen);
			UnlockPubScreen(NULL, screen);
		}
	} else {
		ami_set_screen_defaults(scrn);
	}
	/**/

	hotlist_init(nsoption_charp(hotlist_file),
			nsoption_charp(hotlist_file));
	search_web_select_provider(nsoption_charp(search_web_provider));

	if (notalreadyrunning && 
	    (nsoption_bool(startup_no_window) == false))
		ami_openscreenfirst();

	NSLOG(netsurf, INFO, "gui_init2: after openscreen, creating browser");

	/* Shell URLs from ReadArgs URL/M */
	if(cli_url_count > 0 && notalreadyrunning) {
		int i;
		int first = 0;

		for(i = 0; i < cli_url_count; i++) {
			error = ami_string_to_nsurl(cli_urls[i], &url);
			if(error == NSERROR_OK) {
				if(first == 0) {
					error = browser_window_create(BW_CREATE_HISTORY,
							url, NULL, NULL, &bw);
					first = 1;
				} else {
					error = browser_window_create(
							BW_CREATE_CLONE | BW_CREATE_HISTORY,
							url, NULL, bw, &bw);
				}
				nsurl_unref(url);
			}
			if(error != NSERROR_OK) {
				amiga_warn_user(messages_get_errorcode(error), 0);
			}
			free(cli_urls[i]);
			cli_urls[i] = NULL;
		}
		free(cli_urls);
		cli_urls = NULL;
		cli_url_count = 0;
		if(temp_homepage_url != NULL) {
			free(temp_homepage_url);
			temp_homepage_url = NULL;
		}
	} else if(temp_homepage_url && notalreadyrunning) {
		error = nsurl_create(temp_homepage_url, &url);
		if (error == NSERROR_OK) {
			error = browser_window_create(BW_CREATE_HISTORY,
					url,
					NULL,
					NULL,
					&bw);
			nsurl_unref(url);
		}
		if (error != NSERROR_OK) {
			amiga_warn_user(messages_get_errorcode(error), 0);
		}
		free(temp_homepage_url);
		temp_homepage_url = NULL;
	}

	if(argc == 0) { // WB
		struct WBStartup *WBenchMsg = (struct WBStartup *)argv;
		struct WBArg *wbarg;
		int first=0,i=0;
		char fullpath[1024];

		for(i=0,wbarg=WBenchMsg->sm_ArgList;i<WBenchMsg->sm_NumArgs;i++,wbarg++)
		{
			if(i==0) continue;
			if((wbarg->wa_Lock)&&(*wbarg->wa_Name))
			{
				DevNameFromLock(wbarg->wa_Lock,fullpath,1024,DN_FULLPATH);
				AddPart(fullpath,wbarg->wa_Name,1024);

				if(!temp_homepage_url) {
					nsurl *temp_url;
					if (netsurf_path_to_nsurl(fullpath, &temp_url) == NSERROR_OK) {
						temp_homepage_url = strdup(nsurl_access(temp_url));
						nsurl_unref(temp_url);
					}
				}

				if(notalreadyrunning)
				{
					error = nsurl_create(temp_homepage_url, &url);

					if (error == NSERROR_OK) {
						if(!first)
						{
							error = browser_window_create(BW_CREATE_HISTORY,
										      url,
										      NULL,
										      NULL,
										      &bw);

							first=1;
						}
						else
						{
							error = browser_window_create(BW_CREATE_CLONE | BW_CREATE_HISTORY,
										      url,
										      NULL,
										      bw,
										      &bw);

						}
						nsurl_unref(url);

					}
					if (error != NSERROR_OK) {
						amiga_warn_user(messages_get_errorcode(error), 0);
					}
					free(temp_homepage_url);
					temp_homepage_url = NULL;
				}
			}
			/* this should be where we read tooltypes, but it's too late for that now */
		}
	}

	nsoption_setnull_charp(homepage_url, (char *)strdup(NETSURF_HOMEPAGE));

	/* Built-in home replaces about:welcome / blank in saved Choices. */
	if(nsoption_charp(homepage_url) != NULL &&
	   (strcmp(nsoption_charp(homepage_url), "about:welcome") == 0 ||
	    strcmp(nsoption_charp(homepage_url), "about:blank") == 0 ||
	    strcmp(nsoption_charp(homepage_url), "about:welcome/") == 0)) {
		nsoption_set_charp(homepage_url, (char *)strdup("about:home"));
	}

	if(!notalreadyrunning)
	{
		STRPTR sendcmd = NULL;
		char newtab[11];
		int i;

		/* Hand off to the running instance via ARexx OPEN … NEW/NEWTAB */
		newtab[0] = '\0';
		if(nsoption_bool(tab_new_session) == true)
			strcpy(newtab, "TAB ACTIVE");

		if(cli_url_count > 0) {
			for(i = 0; i < cli_url_count; i++) {
				sendcmd = ASPrintf("OPEN \"%s\" NEW%s",
						cli_urls[i], newtab);
				if(sendcmd != NULL) {
					ami_arexx_self(sendcmd);
					FreeVec(sendcmd);
				}
				free(cli_urls[i]);
				cli_urls[i] = NULL;
			}
			free(cli_urls);
			cli_urls = NULL;
			cli_url_count = 0;
			if(temp_homepage_url != NULL) {
				free(temp_homepage_url);
				temp_homepage_url = NULL;
			}
		} else if(temp_homepage_url != NULL) {
			sendcmd = ASPrintf("OPEN \"%s\" NEW%s",
					temp_homepage_url, newtab);
			free(temp_homepage_url);
			temp_homepage_url = NULL;
			if(sendcmd != NULL) {
				ami_arexx_self(sendcmd);
				FreeVec(sendcmd);
			}
		} else {
			/* No URL: open homepage in a new window/tab (same as
			 * first launch with no args), then bring to front. */
			sendcmd = ASPrintf("OPEN \"%s\" NEW%s",
					nsoption_charp(homepage_url), newtab);
			if(sendcmd != NULL) {
				ami_arexx_self(sendcmd);
				FreeVec(sendcmd);
			}
		}

		ami_arexx_self("TOFRONT");

		ami_quit=true;
		return;
	}
#ifdef __amigaos4__
	if(IApplication)
	{
		if(argc == 0)
		{
			ULONG noicon = TAG_IGNORE;

			if (nsoption_bool(hide_docky_icon)) 
				noicon = REGAPP_NoIcon;

			ami_appid = RegisterApplication(messages_get("NetSurf"),
				REGAPP_URLIdentifier, "amigazen.com",
				REGAPP_WBStartup, (struct WBStartup *)argv,
				noicon, TRUE,
				REGAPP_HasPrefsWindow, TRUE,
				REGAPP_CanCreateNewDocs, TRUE,
				REGAPP_UniqueApplication, TRUE,
				REGAPP_Description, messages_get("NetSurfDesc"),
				TAG_DONE);
		}
		else
		{
/* TODO: Specify icon when run from Shell */
			ami_appid = RegisterApplication(messages_get("NetSurf"),
				REGAPP_URLIdentifier, "amigazen.com",
				REGAPP_FileName, argv[0],
				REGAPP_NoIcon, TRUE,
				REGAPP_HasPrefsWindow, TRUE,
				REGAPP_CanCreateNewDocs, TRUE,
				REGAPP_UniqueApplication, TRUE,
				REGAPP_Description, messages_get("NetSurfDesc"),
				TAG_DONE);
		}

		GetApplicationAttrs(ami_appid, APPATTR_Port, (ULONG)&applibport, TAG_DONE);
		if(applibport) applibsig = (1L << applibport->mp_SigBit);
	}
#endif
	if(!bw && (nsoption_bool(startup_no_window) == false)) {
		NSLOG(netsurf, INFO, "gui_init2: homepage browser_window_create");
		error = nsurl_create(nsoption_charp(homepage_url), &url);
		if (error == NSERROR_OK) {
			error = browser_window_create(BW_CREATE_HISTORY,
						      url,
						      NULL,
						      NULL,
						      NULL);
			nsurl_unref(url);
		}
		NSLOG(netsurf, INFO, "gui_init2: homepage create done (%d)",
				(int)error);
		if (error != NSERROR_OK) {
			amiga_warn_user(messages_get_errorcode(error), 0);
		}
	}
	NSLOG(netsurf, INFO, "gui_init2: finished");
}

/**
 * True if status text is the content "Done (n.ns)" completion message.
 * Nami keeps telemetry in the screen title and skips this flash.
 */
static BOOL ami_gui_status_is_done(const char *text)
{
	const char *done;
	size_t len;

	if(text == NULL || text[0] == '\0')
		return FALSE;
	done = messages_get("Done");
	if(done == NULL || done[0] == '\0')
		return FALSE;
	len = strlen(done);
	if(strncmp(text, done, len) != 0)
		return FALSE;
	if(text[len] == '\0' || text[len] == ' ' || text[len] == '(')
		return TRUE;
	return FALSE;
}

/**
 * Build AISS-style variant path: TBImages:nav_west + "_h" → …:nav_west_h
 * or file.png → file_h.png.
 */
static void ami_gui_path_aiss_suffix(char *dst, size_t dstsz,
		const char *src, const char *suffix)
{
	const char *slash;
	const char *colon;
	const char *base;
	const char *dot;
	size_t prefix_len;
	size_t suffix_len;
	size_t ext_len;

	if(dst == NULL || dstsz == 0)
		return;
	dst[0] = '\0';
	if(src == NULL || src[0] == '\0' || suffix == NULL)
		return;

	slash = strrchr(src, '/');
	colon = strrchr(src, ':');
	base = src;
	if(slash != NULL && slash + 1 > base)
		base = slash + 1;
	if(colon != NULL && colon + 1 > base)
		base = colon + 1;
	dot = strrchr(base, '.');
	if(dot != NULL)
		prefix_len = (size_t)(dot - src);
	else
		prefix_len = strlen(src);
	suffix_len = strlen(suffix);
	ext_len = (dot != NULL) ? strlen(dot) : 0;
	if(prefix_len + suffix_len + ext_len + 1 > dstsz)
		return;
	memcpy(dst, src, prefix_len);
	memcpy(dst + prefix_len, suffix, suffix_len);
	if(dot != NULL)
		memcpy(dst + prefix_len + suffix_len, dot, ext_len);
	dst[prefix_len + suffix_len + ext_len] = '\0';
}

/**
 * Toolbar icon enable/disable + Nami hover/press art.
 * Idle: normal glyph, no selection fill.
 * Hover: button.gadget blue highlight (GA_Selected), normal glyph.
 * Pressed: keep highlight and swap to AISS _s glyph.
 * Ghosted (_g): no hover/press highlight.
 */
static void ami_gui_tb_apply_nami_button(struct gui_window_2 *gwin, ULONG gid,
		ULONG bm_normal, ULONG bm_sel, ULONG bm_hov, ULONG bm_ghost,
		BOOL ghosted)
{
	Object *bm;
	ULONG selected;

	(void)bm_hov;

	if(gwin == NULL || gwin->objects[gid] == NULL)
		return;

	bm = gwin->objects[bm_normal];
	selected = FALSE;

	if(ghosted) {
		if(gwin->objects[bm_ghost] != NULL)
			bm = gwin->objects[bm_ghost];
	} else if(gwin->tb_armed_gid == (WORD)gid) {
		selected = TRUE;
		if(gwin->objects[bm_sel] != NULL)
			bm = gwin->objects[bm_sel];
	} else if(gwin->tb_hover_gid == (WORD)gid) {
		selected = TRUE;
	}

	RefreshSetGadgetAttrs((struct Gadget *)gwin->objects[gid],
			gwin->win, NULL,
			BUTTON_RenderImage, bm,
			BUTTON_BevelStyle, BVS_NONE,
			GA_Selected, selected,
			GA_ReadOnly, ghosted ? TRUE : FALSE,
			GA_Disabled, FALSE,
			TAG_DONE);
}

static void ami_gui_tb_set_ghosted(struct gui_window_2 *gwin, ULONG gid,
		ULONG bm_normal, ULONG bm_ghost, BOOL ghosted)
{
	ULONG bm_sel;
	ULONG bm_hov;

	if(gwin == NULL || gwin->objects[gid] == NULL)
		return;

	if(gwin->ui_nami == false) {
		Object *bm;

		bm = gwin->objects[bm_normal];
		if(ghosted && gwin->objects[bm_ghost] != NULL)
			bm = gwin->objects[bm_ghost];
		RefreshSetGadgetAttrs((struct Gadget *)gwin->objects[gid],
				gwin->win, NULL,
				BUTTON_RenderImage, bm,
				GA_ReadOnly, ghosted ? TRUE : FALSE,
				GA_Disabled, FALSE,
				TAG_DONE);
		return;
	}

	bm_sel = 0;
	bm_hov = 0;
	if(bm_normal == GID_BACK_BM) {
		bm_sel = GID_BACK_BM_S;
		bm_hov = GID_BACK_BM_H;
	} else if(bm_normal == GID_FORWARD_BM) {
		bm_sel = GID_FORWARD_BM_S;
		bm_hov = GID_FORWARD_BM_H;
	} else if(bm_normal == GID_STOP_BM) {
		bm_sel = GID_STOP_BM_S;
		bm_hov = GID_STOP_BM_H;
	} else if(bm_normal == GID_RELOAD_BM) {
		bm_sel = GID_RELOAD_BM_S;
		bm_hov = GID_RELOAD_BM_H;
	} else if(bm_normal == GID_HOME_BM) {
		bm_sel = GID_HOME_BM_S;
		bm_hov = GID_HOME_BM_H;
	}

	if(ghosted) {
		if(gwin->tb_hover_gid == (WORD)gid)
			gwin->tb_hover_gid = 0;
		if(gwin->tb_armed_gid == (WORD)gid)
			gwin->tb_armed_gid = 0;
	}

	ami_gui_tb_apply_nami_button(gwin, gid, bm_normal, bm_sel, bm_hov,
			bm_ghost, ghosted);
}

static BOOL ami_gui_tb_gid_readonly(struct gui_window_2 *gwin, ULONG gid)
{
	ULONG ro;

	ro = FALSE;
	if(gwin == NULL || gid >= (ULONG)GID_LAST)
		return TRUE;
	if(gwin->objects[gid] == NULL)
		return TRUE;
	GetAttr(GA_ReadOnly, gwin->objects[gid], &ro);
	return ro ? TRUE : FALSE;
}

static WORD ami_gui_tb_hit(struct gui_window_2 *gwin)
{
	LONG mx, my;
	int i;

	if(gwin == NULL || gwin->ui_nami == false || gwin->win == NULL)
		return 0;

	mx = gwin->win->MouseX;
	my = gwin->win->MouseY;

	if(gwin->objects[GID_BACK] != NULL &&
	   ami_gui_tb_gid_readonly(gwin, GID_BACK) == FALSE &&
	   ami_gadget_hit(gwin->objects[GID_BACK], mx, my))
		return GID_BACK;
	if(gwin->objects[GID_FORWARD] != NULL &&
	   ami_gui_tb_gid_readonly(gwin, GID_FORWARD) == FALSE &&
	   ami_gadget_hit(gwin->objects[GID_FORWARD], mx, my))
		return GID_FORWARD;
	if(gwin->objects[GID_STOP] != NULL &&
	   ami_gui_tb_gid_readonly(gwin, GID_STOP) == FALSE &&
	   ami_gadget_hit(gwin->objects[GID_STOP], mx, my))
		return GID_STOP;
	if(gwin->objects[GID_RELOAD] != NULL &&
	   ami_gui_tb_gid_readonly(gwin, GID_RELOAD) == FALSE &&
	   ami_gadget_hit(gwin->objects[GID_RELOAD], mx, my))
		return GID_RELOAD;
	if(gwin->objects[GID_PAGEINFO] != NULL &&
	   ami_gadget_hit(gwin->objects[GID_PAGEINFO], mx, my))
		return GID_PAGEINFO;
	if(gwin->objects[GID_ICON] != NULL &&
	   ami_gadget_hit(gwin->objects[GID_ICON], mx, my))
		return GID_ICON;
	if(gwin->objects[GID_WIN_CLOSE] != NULL &&
	   ami_gadget_hit(gwin->objects[GID_WIN_CLOSE], mx, my))
		return GID_WIN_CLOSE;
	if(gwin->objects[GID_SIDE_TOGGLE] != NULL &&
	   ami_gadget_hit(gwin->objects[GID_SIDE_TOGGLE], mx, my))
		return GID_SIDE_TOGGLE;
	if(gwin->objects[GID_SIDE_NEWTAB] != NULL &&
	   ami_gadget_hit(gwin->objects[GID_SIDE_NEWTAB], mx, my))
		return GID_SIDE_NEWTAB;
	if(gwin->objects[GID_SIDE_CLOSETAB] != NULL &&
	   ami_gadget_hit(gwin->objects[GID_SIDE_CLOSETAB], mx, my))
		return GID_SIDE_CLOSETAB;
	for(i = 0; i < AMI_SIDE_HOTLIST_MAX; i++) {
		if(gwin->side_hot_url[i] == NULL)
			continue;
		if(gwin->side_hot_btn[i] != NULL &&
		   ami_gadget_hit(gwin->side_hot_btn[i], mx, my))
			return (WORD)(GID_SIDE_HOT_BASE + i);
	}
	return 0;
}

/* Highlight-only chrome / hotlist buttons (same GA_Selected hover as pageinfo). */
static void ami_gui_tb_apply_chrome_hover(struct gui_window_2 *gwin, WORD gid)
{
	Object *obj;
	Object *chrome;
	ULONG selected;
	int hot_i;

	if(gwin == NULL || gid == 0)
		return;

	obj = NULL;
	if(gid >= (WORD)GID_SIDE_HOT_BASE &&
	   gid < (WORD)(GID_SIDE_HOT_BASE + AMI_SIDE_HOTLIST_MAX)) {
		hot_i = (int)(gid - GID_SIDE_HOT_BASE);
		obj = gwin->side_hot_btn[hot_i];
	} else if(gid > 0 && gid < (WORD)GID_LAST) {
		obj = gwin->objects[gid];
	}
	if(obj == NULL || gwin->win == NULL)
		return;

	selected = (gwin->tb_armed_gid == gid || gwin->tb_hover_gid == gid) ?
			TRUE : FALSE;
	/*
	 * Chrome close/toggle are Transparent by default; force an opaque
	 * select fill so hover is visible (masked glyphs alone hide it).
	 */
	RefreshSetGadgetAttrs((struct Gadget *)obj, gwin->win, NULL,
			BUTTON_BevelStyle, BVS_NONE,
			BUTTON_Transparent, FALSE,
			GA_Selected, selected,
			TAG_DONE);
	if(gid == (WORD)GID_WIN_CLOSE || gid == (WORD)GID_SIDE_TOGGLE) {
		chrome = gwin->objects[GID_CHROMELAYOUT];
		if(chrome != NULL)
			RefreshGList((struct Gadget *)chrome, gwin->win, NULL, 1);
	}
}

static void ami_gui_tb_refresh_gid(struct gui_window_2 *gwin, WORD gid)
{
	BOOL ghosted;

	if(gwin == NULL || gid == 0)
		return;

	ghosted = FALSE;
	if(gid > 0 && gid < (WORD)GID_LAST)
		ghosted = ami_gui_tb_gid_readonly(gwin, (ULONG)gid);

	switch(gid) {
	case GID_BACK:
		ami_gui_tb_apply_nami_button(gwin, GID_BACK, GID_BACK_BM,
				GID_BACK_BM_S, GID_BACK_BM_H, GID_BACK_BM_G, ghosted);
		break;
	case GID_FORWARD:
		ami_gui_tb_apply_nami_button(gwin, GID_FORWARD, GID_FORWARD_BM,
				GID_FORWARD_BM_S, GID_FORWARD_BM_H, GID_FORWARD_BM_G,
				ghosted);
		break;
	case GID_STOP:
		ami_gui_tb_apply_nami_button(gwin, GID_STOP, GID_STOP_BM,
				GID_STOP_BM_S, GID_STOP_BM_H, GID_STOP_BM_G, ghosted);
		break;
	case GID_RELOAD:
		/* May show Stop or Reload art — ami_update_buttons sets image ids;
		 * re-query RenderImage owner via ghost helper using reload slot's
		 * current ReadOnly and whichever BM was last applied. Prefer reload
		 * set; Stop-in-reload-slot still uses STOP_* when loading. */
		{
			ULONG stop_avail;

			stop_avail = FALSE;
			if(gwin->gw != NULL && gwin->gw->bw != NULL &&
			   browser_window_stop_available(gwin->gw->bw))
				stop_avail = TRUE;
			if(stop_avail) {
				ami_gui_tb_apply_nami_button(gwin, GID_RELOAD, GID_STOP_BM,
						GID_STOP_BM_S, GID_STOP_BM_H, GID_STOP_BM_G,
						FALSE);
			} else {
				ami_gui_tb_apply_nami_button(gwin, GID_RELOAD, GID_RELOAD_BM,
						GID_RELOAD_BM_S, GID_RELOAD_BM_H, GID_RELOAD_BM_G,
						ghosted);
			}
		}
		break;
	case GID_PAGEINFO:
		/* Highlight only — no dedicated _s art for padlock glyphs */
		RefreshSetGadgetAttrs((struct Gadget *)gwin->objects[GID_PAGEINFO],
				gwin->win, NULL,
				BUTTON_BevelStyle, BVS_NONE,
				GA_Selected,
					(gwin->tb_armed_gid == GID_PAGEINFO ||
					 gwin->tb_hover_gid == GID_PAGEINFO)
						? TRUE : FALSE,
				TAG_DONE);
		break;
	case GID_ICON:
		/* space.gadget has no select fill; redraw slot contents */
		if(gwin->gw != NULL)
			gui_window_set_icon(gwin->gw, gwin->gw->favicon);
		break;
	case GID_WIN_CLOSE:
	case GID_SIDE_TOGGLE:
	case GID_SIDE_NEWTAB:
	case GID_SIDE_CLOSETAB:
		ami_gui_tb_apply_chrome_hover(gwin, gid);
		break;
	default:
		if(gid >= (WORD)GID_SIDE_HOT_BASE &&
		   gid < (WORD)(GID_SIDE_HOT_BASE + AMI_SIDE_HOTLIST_MAX))
			ami_gui_tb_apply_chrome_hover(gwin, gid);
		break;
	}
}

static void ami_gui_tb_set_hover(struct gui_window_2 *gwin, WORD gid)
{
	WORD prev;

	if(gwin == NULL || gwin->ui_nami == false)
		return;
	if(gid == gwin->tb_hover_gid)
		return;
	prev = gwin->tb_hover_gid;
	gwin->tb_hover_gid = gid;
	if(prev != 0)
		ami_gui_tb_refresh_gid(gwin, prev);
	if(gid != 0)
		ami_gui_tb_refresh_gid(gwin, gid);
	/* Highlight refresh fills the toolbar row over the corner glyphs. */
	ami_gui_nami_refresh_corner_gadgets(gwin);
}

static void ami_gui_tb_set_armed(struct gui_window_2 *gwin, WORD gid)
{
	WORD prev;

	if(gwin == NULL || gwin->ui_nami == false)
		return;
	if(gid == gwin->tb_armed_gid)
		return;
	prev = gwin->tb_armed_gid;
	gwin->tb_armed_gid = gid;
	if(prev != 0)
		ami_gui_tb_refresh_gid(gwin, prev);
	if(gid != 0)
		ami_gui_tb_refresh_gid(gwin, gid);
	ami_gui_nami_refresh_corner_gadgets(gwin);
}

static void ami_update_buttons(struct gui_window_2 *gwin)
{
	long back=FALSE, forward=FALSE, tabclose=FALSE, stop=FALSE, reload=FALSE;
	long s_back, s_forward, s_tabclose, s_stop, s_reload;

	if(!browser_window_back_available(gwin->gw->bw))
		back=TRUE;

	if(!browser_window_forward_available(gwin->gw->bw))
		forward=TRUE;

	if(!browser_window_stop_available(gwin->gw->bw))
		stop=TRUE;

	if(!browser_window_reload_available(gwin->gw->bw))
		reload=TRUE;

	if(nsoption_bool(kiosk_mode) == false) {
		if(gwin->tabs <= 1) {
			tabclose=TRUE;
			ami_gui_menu_set_disabled(gwin->win, gwin->imenu, M_CLOSETAB, true);
		} else {
			ami_gui_menu_set_disabled(gwin->win, gwin->imenu, M_CLOSETAB, false);
		}
	}

	GetAttr(GA_ReadOnly, gwin->objects[GID_BACK], (uint32 *)&s_back);
	GetAttr(GA_ReadOnly, gwin->objects[GID_FORWARD], (uint32 *)&s_forward);

	if(BOOL_MISMATCH(s_back, back))
		ami_gui_tb_set_ghosted(gwin, GID_BACK, GID_BACK_BM, GID_BACK_BM_G, back);

	if(BOOL_MISMATCH(s_forward, forward))
		ami_gui_tb_set_ghosted(gwin, GID_FORWARD, GID_FORWARD_BM,
				GID_FORWARD_BM_G, forward);

	/*
	 * Nami: one toolbar slot — Stop art while loading, else Reload.
	 * Same BUTTON_RenderImage swap as favicon star / page-info padlock.
	 */
	if(gwin->ui_nami) {
		ULONG bm_n, bm_g;
		const char *hint;
		long disabled;

		if(stop == FALSE) {
			bm_n = GID_STOP_BM;
			bm_g = GID_STOP_BM_G;
			hint = gwin->helphints[GID_STOP];
			disabled = FALSE;
		} else {
			bm_n = GID_RELOAD_BM;
			bm_g = GID_RELOAD_BM_G;
			hint = gwin->helphints[GID_RELOAD];
			disabled = reload;
		}
		if(gwin->objects[GID_RELOAD] != NULL) {
			RefreshSetGadgetAttrs((struct Gadget *)gwin->objects[GID_RELOAD],
				gwin->win, NULL,
				AMI_GA_HELP, hint,
				TAG_DONE);
			ami_gui_tb_set_ghosted(gwin, GID_RELOAD, bm_n, bm_g, disabled);
		}
	} else {
		GetAttr(GA_ReadOnly, gwin->objects[GID_RELOAD], (uint32 *)&s_reload);
		GetAttr(GA_ReadOnly, gwin->objects[GID_STOP], (uint32 *)&s_stop);

		if(BOOL_MISMATCH(s_reload, reload))
			ami_gui_tb_set_ghosted(gwin, GID_RELOAD, GID_RELOAD_BM,
					GID_RELOAD_BM_G, reload);

		if(BOOL_MISMATCH(s_stop, stop))
			ami_gui_tb_set_ghosted(gwin, GID_STOP, GID_STOP_BM,
					GID_STOP_BM_G, stop);
	}

	/* Standalone close button is only used when clicktab has no embedded close */
	if((ami_clicktab_has_close() == FALSE) &&
	   (gwin->objects[GID_CLOSETAB] != NULL)) {
		if(gwin->tabs <= 1) tabclose = TRUE;

		GetAttr(GA_ReadOnly, gwin->objects[GID_CLOSETAB], (uint32 *)&s_tabclose);

		if(BOOL_MISMATCH(s_tabclose, tabclose))
			ami_gui_tb_set_ghosted(gwin, GID_CLOSETAB, GID_CLOSETAB_BM,
					GID_CLOSETAB_BM_G, tabclose);
	}

	/* Update the back/forward buttons history context menu */
	ami_ctxmenu_history_create(AMI_CTXMENU_HISTORY_BACK, gwin);
	ami_ctxmenu_history_create(AMI_CTXMENU_HISTORY_FORWARD, gwin);

	/* Stop/reload swap refreshes the toolbar and paints over the glyphs. */
	ami_gui_nami_refresh_corner_gadgets(gwin);
}

void ami_gui_history(struct gui_window_2 *gwin, bool back)
{
	if(back == true)
	{
		if(browser_window_back_available(gwin->gw->bw))
			browser_window_history_back(gwin->gw->bw, false);
	}
	else
	{
		if(browser_window_forward_available(gwin->gw->bw))
			browser_window_history_forward(gwin->gw->bw, false);
	}

	ami_update_buttons(gwin);
}

int ami_key_to_nskey(ULONG keycode, struct InputEvent *ie)
{
	int nskey = 0, chars;
	char buffer[20];
	char *utf8 = NULL;

	if(keycode >= IECODE_UP_PREFIX) return 0;

	switch(keycode)
	{
		case RAWKEY_CRSRUP:
			if(ie->ie_Qualifier & NSA_QUAL_SHIFT) {
				nskey = NS_KEY_PAGE_UP;
			} else if(ie->ie_Qualifier & NSA_QUAL_ALT) {
				nskey = NS_KEY_TEXT_START;
			}
			else nskey = NS_KEY_UP;
		break;
		case RAWKEY_CRSRDOWN:
			if(ie->ie_Qualifier & NSA_QUAL_SHIFT) {
				nskey = NS_KEY_PAGE_DOWN;
			} else if(ie->ie_Qualifier & NSA_QUAL_ALT) {
				nskey = NS_KEY_TEXT_END;
			}
			else nskey = NS_KEY_DOWN;
		break;
		case RAWKEY_CRSRLEFT:
			if(ie->ie_Qualifier & NSA_QUAL_SHIFT) {
				nskey = NS_KEY_LINE_START;
			}else if(ie->ie_Qualifier & NSA_QUAL_ALT) {
				nskey = NS_KEY_WORD_LEFT;
			}
			else nskey = NS_KEY_LEFT;
		break;
		case RAWKEY_CRSRRIGHT:
			if(ie->ie_Qualifier & NSA_QUAL_SHIFT) {
				nskey = NS_KEY_LINE_END;
			}else if(ie->ie_Qualifier & NSA_QUAL_ALT) {
				nskey = NS_KEY_WORD_RIGHT;
			}
			else nskey = NS_KEY_RIGHT;
		break;
		case RAWKEY_ESC:
			nskey = NS_KEY_ESCAPE;
		break;
		case RAWKEY_PAGEUP:
			nskey = NS_KEY_PAGE_UP;
		break;
		case RAWKEY_PAGEDOWN:
			nskey = NS_KEY_PAGE_DOWN;
		break;
		case RAWKEY_HOME:
			nskey = NS_KEY_TEXT_START;
		break;
		case RAWKEY_END:
			nskey = NS_KEY_TEXT_END;
		break;
		case RAWKEY_BACKSPACE:
			if(ie->ie_Qualifier & NSA_QUAL_SHIFT) {
				nskey = NS_KEY_DELETE_LINE_START;
			} else {
				nskey = NS_KEY_DELETE_LEFT;
			}
		break;
		case RAWKEY_DEL:
			if(ie->ie_Qualifier & NSA_QUAL_SHIFT) {
				nskey = NS_KEY_DELETE_LINE_END;
			} else {
				nskey = NS_KEY_DELETE_RIGHT;
			}
		break;
		case RAWKEY_TAB:
			if(ie->ie_Qualifier & NSA_QUAL_SHIFT) {
				nskey = NS_KEY_SHIFT_TAB;
			} else {
				nskey = NS_KEY_TAB;
			}
		break;
		case RAWKEY_F5:
		case RAWKEY_F8:
		case RAWKEY_F9:
		case RAWKEY_F10:
		case RAWKEY_F12:
		case RAWKEY_HELP:
			// don't translate
			nskey = keycode;
		break;
		default:
			if((chars = MapRawKey(ie,buffer,20,NULL)) > 0) {
				if(utf8_from_local_encoding(buffer, chars, &utf8) != NSERROR_OK) return 0;
				nskey = utf8_to_ucs4(utf8, utf8_char_byte_length(utf8));
				free(utf8);

				if(ie->ie_Qualifier & IEQUALIFIER_RCOMMAND) {
					switch(nskey) {
						case 'a':
							nskey = NS_KEY_SELECT_ALL;
						break;
						case 'c':
							nskey = NS_KEY_COPY_SELECTION;
						break;
						case 'v':
							nskey = NS_KEY_PASTE;
						break;
						case 'x':
							nskey = NS_KEY_CUT_SELECTION;
						break;
						case 'y':
							nskey = NS_KEY_REDO;
						break;
						case 'z':
							nskey = NS_KEY_UNDO;
						break;
					}
				}
			}
		break;
	}

	return nskey;
}

int ami_gui_get_quals(Object *win_obj)
{
	uint32 quals = 0;
	int key_state = 0;

	/* WINDOW_Qualifier is available on OS3.2 window.class V47+ (and OS4) */
	GetAttr(WINDOW_Qualifier, win_obj, (uint32 *)&quals);

	if(quals & NSA_QUAL_SHIFT) {
		key_state |= BROWSER_MOUSE_MOD_1;
	}

	if(quals & IEQUALIFIER_CONTROL) {
		key_state |= BROWSER_MOUSE_MOD_2;
	}

	if(quals & NSA_QUAL_ALT) {
		key_state |= BROWSER_MOUSE_MOD_3;
	}

	return key_state;
}

static void ami_update_quals(struct gui_window_2 *gwin)
{
	gwin->key_state = ami_gui_get_quals(gwin->objects[OID_MAIN]);
}

/* exported interface documented in amiga/gui.h */
nserror ami_gui_get_space_box(Object *obj, struct IBox **bbox)
{
#ifdef __amigaos4__
	struct IBox *ib = AllocVec(sizeof(struct IBox), MEMF_PRIVATE);
#else
	struct IBox *ib = ami_memory_allocvec(sizeof(struct IBox), 0);
#endif
	if(ib == NULL) return NSERROR_NOMEM;
#ifdef __amigaos4__
	if(LIB_IS_AT_LEAST((struct Library *)SpaceBase, 53, 6)) {
		GetAttr(SPACE_RenderBox, obj, (ULONG *)ib);
	} else
#endif
	{
		struct IBox *t_ib;
		GetAttr(SPACE_AreaBox, obj, (ULONG *)&t_ib);
		if(t_ib == NULL) {
			FreeVec(ib);
			return NSERROR_NOMEM;
		} else {
			/* Create a copy so this works the same as the newer SPACE_RenderBox */
			CopyMem(t_ib, ib, sizeof(struct IBox));
		}
	}

	*bbox = ib;
	return NSERROR_OK;
}

/* exported interface documented in amiga/gui.h */
void ami_gui_free_space_box(struct IBox *bbox)
{
	if(bbox != NULL) FreeVec(bbox);
}

static bool ami_spacebox_to_ns_coords(struct gui_window_2 *gwin,
		int *restrict x, int *restrict y, int space_x, int space_y)
{
	int ns_x = space_x;
	int ns_y = space_y;

	ns_x += gwin->gw->scrollx;
	ns_y += gwin->gw->scrolly;

	*x = ns_x;
	*y = ns_y;

	return true;	
}

bool ami_mouse_to_ns_coords(struct gui_window_2 *gwin, int *restrict x, int *restrict y,
	int mouse_x, int mouse_y)
{
	int ns_x, ns_y;
	struct IBox *bbox;

	if(mouse_x == -1) mouse_x = gwin->win->MouseX;
	if(mouse_y == -1) mouse_y = gwin->win->MouseY;

	if(ami_gui_get_space_box((Object *)gwin->objects[GID_BROWSER], &bbox) == NSERROR_OK) {
		ns_x = (ULONG)(mouse_x - bbox->Left);
		ns_y = (ULONG)(mouse_y - bbox->Top);

		if((ns_x < 0) || (ns_x > bbox->Width) || (ns_y < 0) || (ns_y > bbox->Height)) {
			ami_gui_free_space_box(bbox);
			return false;
		}

		ami_gui_free_space_box(bbox);
	} else {
		amiga_warn_user("NoMemory", "");
		return false;
	}

	return ami_spacebox_to_ns_coords(gwin, x, y, ns_x, ns_y);
}

static void ami_gui_scroll_internal(struct gui_window_2 *gwin, int xs, int ys)
{
	struct IBox *bbox;
	int x, y;
	struct rect rect;

	if(ami_mouse_to_ns_coords(gwin, &x, &y, -1, -1) == true)
	{
		if(browser_window_scroll_at_point(gwin->gw->bw, x, y, xs, ys) == false)
		{
			ami_gui_scroll_viewport(gwin, xs, ys);
		}
	}
}

/**
 * Scroll the HTML viewport by deltas (or page/top/bottom sentinels).
 * Used for border arrows where the pointer is outside the browser SpaceObj.
 */
static void ami_gui_scroll_viewport(struct gui_window_2 *gwin, int xs, int ys)
{
	struct IBox *bbox;
	struct rect rect;
	int width, height;

	if(gwin == NULL || gwin->gw == NULL || gwin->gw->bw == NULL)
		return;

	gui_window_get_scroll(gwin->gw,
		&gwin->gw->scrollx,
		&gwin->gw->scrolly);

	if(ami_gui_get_space_box((Object *)gwin->objects[GID_BROWSER], &bbox) != NSERROR_OK) {
		amiga_warn_user("NoMemory", "");
		return;
	}

	browser_window_get_extents(gwin->gw->bw, false, &width, &height);

	switch(xs)
	{
		case SCROLL_PAGE_UP:
			xs = gwin->gw->scrollx - bbox->Width;
		break;

		case SCROLL_PAGE_DOWN:
			xs = gwin->gw->scrollx + bbox->Width;
		break;

		case SCROLL_TOP:
			xs = 0;
		break;

		case SCROLL_BOTTOM:
			xs = width;
		break;

		default:
			xs += gwin->gw->scrollx;
		break;
	}

	switch(ys)
	{
		case SCROLL_PAGE_UP:
			ys = gwin->gw->scrolly - bbox->Height;
		break;

		case SCROLL_PAGE_DOWN:
			ys = gwin->gw->scrolly + bbox->Height;
		break;

		case SCROLL_TOP:
			ys = 0;
		break;

		case SCROLL_BOTTOM:
			ys = height;
		break;

		default:
			ys += gwin->gw->scrolly;
		break;
	}

	ami_gui_free_space_box(bbox);
	rect.x0 = rect.x1 = xs;
	rect.y0 = rect.y1 = ys;
	gui_window_set_scroll(gwin->gw, &rect);
}

static struct IBox *ami_ns_rect_to_ibox(struct gui_window_2 *gwin, const struct rect *rect)
{
	struct IBox *bbox, *ibox;

	ibox = malloc(sizeof(struct IBox));
	if(ibox == NULL) return NULL;

	if(ami_gui_get_space_box((Object *)gwin->objects[GID_BROWSER], &bbox) != NSERROR_OK) {
		free(ibox);
		amiga_warn_user("NoMemory", "");
		return NULL;
	}

	ibox->Left = gwin->win->MouseX + (rect->x0);
	ibox->Top = gwin->win->MouseY + (rect->y0);

	ibox->Width = (rect->x1 - rect->x0);
	ibox->Height = (rect->y1 - rect->y0);

	if(ibox->Left < bbox->Left) ibox->Left = bbox->Left;
	if(ibox->Top < bbox->Top) ibox->Top = bbox->Top;

	if((ibox->Left > (bbox->Left + bbox->Width)) ||
		(ibox->Top > (bbox->Top + bbox->Height)) ||
		(ibox->Width < 0) || (ibox->Height < 0))
	{
		free(ibox);
		ami_gui_free_space_box(bbox);
		return NULL;
	}

	ami_gui_free_space_box(bbox);
	return ibox;
}

static void ami_gui_trap_mouse(struct gui_window_2 *gwin)
{
#ifdef __amigaos4__
	switch(gwin->drag_op)
	{
		case GDRAGGING_NONE:
		case GDRAGGING_SCROLLBAR:
		case GDRAGGING_OTHER:
		break;

		default:
			if(gwin->ptr_lock)
			{
				SetWindowAttrs(gwin->win, WA_GrabFocus, 10,
					WA_MouseLimits, gwin->ptr_lock, TAG_DONE);
			}
		break;
	}
#endif
}

static void ami_gui_menu_update_all(void)
{
	struct nsObject *node;
	struct nsObject *nnode;
	struct gui_window_2 *gwin;

	if(IsMinListEmpty(window_list))	return;

	node = (struct nsObject *)GetHead((struct List *)window_list);

	do {
		nnode=(struct nsObject *)GetSucc((struct Node *)node);
		gwin = node->objstruct;

		if(node->Type == AMINS_WINDOW)
		{
			ami_gui_menu_update_checked(gwin);
		}
	} while((node = nnode));
}

/**
 * Find the current dimensions of a amiga browser window content area.
 *
 * \param gw The gui window to measure content area of.
 * \param width receives width of window
 * \param height receives height of window
 * \return NSERROR_OK on sucess and width and height updated
 *          else error code.
 */
static nserror
gui_window_get_dimensions(struct gui_window *gw,
			  int *restrict width,
			  int *restrict height)
{
	struct IBox *bbox;
	nserror res;

	res = ami_gui_get_space_box((Object *)gw->shared->objects[GID_BROWSER],
				    &bbox);
	if (res != NSERROR_OK) {
		amiga_warn_user("NoMemory", "");
		return res;
	}

	*width = bbox->Width;
	*height = bbox->Height;

	ami_gui_free_space_box(bbox);

	return NSERROR_OK;
}

/* Add a horizontal scroller, if not already present
 * Returns true if changed, false otherwise */
static bool ami_gui_hscroll_add(struct gui_window_2 *gwin)
{
	struct TagItem attrs[2];

	if(gwin->objects[GID_HSCROLL] != NULL) return false;

	/* Nami border scrollers are created once; do not use layout children */
	if(gwin->ui_nami)
		return false;

	attrs[0].ti_Tag = CHILD_MinWidth;
	attrs[0].ti_Data = 0;
	attrs[1].ti_Tag = TAG_DONE;
	attrs[1].ti_Data = 0;

	gwin->objects[GID_HSCROLL] = ScrollerObj,
					GA_ID, GID_HSCROLL,
					GA_RelVerify, TRUE,
					SCROLLER_Orientation, SORIENT_HORIZ,
					ICA_TARGET, ICTARGET_IDCMP,
				ScrollerEnd;
#ifdef __amigaos4__
	IDoMethod(gwin->objects[GID_HSCROLLLAYOUT], LM_ADDCHILD,
			gwin->win, gwin->objects[GID_HSCROLL], attrs);
#else
	SetGadgetAttrs((struct Gadget *)gwin->objects[GID_HSCROLLLAYOUT],
			gwin->win, NULL,
			LAYOUT_AddChild, gwin->objects[GID_HSCROLL], TAG_MORE, &attrs);
#endif
	return true;
}

/* Remove the horizontal scroller, if present */
static bool ami_gui_hscroll_remove(struct gui_window_2 *gwin)
{
	if(gwin->objects[GID_HSCROLL] == NULL) return false;

	/* Nami: keep border scrollers for the life of the window */
	if(gwin->ui_nami)
		return false;

#ifdef __amigaos4__
	IDoMethod(gwin->objects[GID_HSCROLLLAYOUT], LM_REMOVECHILD,
			gwin->win, gwin->objects[GID_HSCROLL]);
#else
	SetGadgetAttrs((struct Gadget *)gwin->objects[GID_HSCROLLLAYOUT],
			gwin->win, NULL,
			LAYOUT_RemoveChild, gwin->objects[GID_HSCROLL], TAG_DONE);
#endif

	gwin->objects[GID_HSCROLL] = NULL;

	return true;
}

/* Add a vertical scroller, if not already present
 * Returns true if changed, false otherwise */
static bool ami_gui_vscroll_add(struct gui_window_2 *gwin)
{
	struct TagItem attrs[2];

	if(gwin->objects[GID_VSCROLL] != NULL) return false;

	if(gwin->ui_nami)
		return false;

	attrs[0].ti_Tag = CHILD_MinWidth;
	attrs[0].ti_Data = 0;
	attrs[1].ti_Tag = TAG_DONE;
	attrs[1].ti_Data = 0;

	gwin->objects[GID_VSCROLL] = ScrollerObj,
					GA_ID, GID_VSCROLL,
					GA_RelVerify, TRUE,
					ICA_TARGET, ICTARGET_IDCMP,
				ScrollerEnd;
#ifdef __amigaos4__
	IDoMethod(gwin->objects[GID_VSCROLLLAYOUT], LM_ADDCHILD,
			gwin->win, gwin->objects[GID_VSCROLL], attrs);
#else
	SetGadgetAttrs((struct Gadget *)gwin->objects[GID_VSCROLLLAYOUT],
			gwin->win, NULL,
			LAYOUT_AddChild, gwin->objects[GID_VSCROLL], TAG_MORE, &attrs);
#endif
	return true;
}

/* Remove the vertical scroller, if present */
static bool ami_gui_vscroll_remove(struct gui_window_2 *gwin)
{
	if(gwin->objects[GID_VSCROLL] == NULL) return false;

	if(gwin->ui_nami)
		return false;

#ifdef __amigaos4__
	IDoMethod(gwin->objects[GID_VSCROLLLAYOUT], LM_REMOVECHILD,
			gwin->win, gwin->objects[GID_VSCROLL]);
#else
	SetGadgetAttrs((struct Gadget *)gwin->objects[GID_VSCROLLLAYOUT],
			gwin->win, NULL,
			LAYOUT_RemoveChild, gwin->objects[GID_VSCROLL], TAG_DONE);
#endif

	gwin->objects[GID_VSCROLL] = NULL;

	return true;
}

/**
 * Nami: border scrollers — sysiclass arrows + borderless propgclass
 * knobs drawn in the size borders (not scroller.gadget bevel chrome).
 * Drive HTML viewport via PGA_Top; stay for the window lifetime.
 * Zoom and depth sit on the window's top-right corner, not in the
 * chrome layout row.
 */
static void ami_gui_nami_create_border_scrollers(struct gui_window_2 *gwin)
{
	struct Window *win;
	struct DrawInfo *dri;
	struct Gadget *cgad;
	struct Image *cim;
	struct Image *zim;
	Object *prev;
	WORD br, bb, bt, bl;
	WORD dh, uh, lw, rw;
	WORD vtop;
	WORD chtop, chh;
	WORD dw, dhh;
	WORD zw, zhh;

	if(gwin == NULL || gwin->ui_nami == false)
		return;
	win = gwin->win;
	if(win == NULL)
		return;
	if(gwin->objects[GID_VSCROLL] != NULL)
		return;

	br = win->BorderRight;
	bb = win->BorderBottom;
	bt = win->BorderTop;
	bl = win->BorderLeft;
	if(br < 12)
		br = 16;
	if(bb < 10)
		bb = 14;

	/* Align with in-layout chrome row (close/tabs/drag) */
	chtop = bt;
	chh = 16;
	if(gwin->objects[GID_CHROMELAYOUT] != NULL) {
		cgad = (struct Gadget *)gwin->objects[GID_CHROMELAYOUT];
		if(cgad->Height > 0) {
			chtop = cgad->TopEdge;
			chh = cgad->Height;
		}
	}

	cim = (struct Image *)gwin->objects[GID_WIN_DEPTH_BM];
	zim = (struct Image *)gwin->objects[GID_WIN_ZOOM_BM];
	dw = (cim != NULL && cim->Width > 0) ? cim->Width : 16;
	dhh = (cim != NULL && cim->Height > 0) ? cim->Height : 16;
	zw = (zim != NULL && zim->Width > 0) ? zim->Width : 16;
	zhh = (zim != NULL && zim->Height > 0) ? zim->Height : 16;
	/* Natural image size.  Do not stretch to the chrome row — that
	 * drops the glyphs down into the window contents. */

	/* Keep the chrome row clear of the corner glyphs (they hang
	 * left from the window's right edge, partly over the client). */
	if(gwin->objects[GID_CHROME_RPAD] != NULL &&
	   gwin->objects[GID_CHROMELAYOUT] != NULL) {
		WORD pad;
		WORD corner;

		pad = 0;
		corner = (WORD)(zw + dw);
		if(corner > br)
			pad = (WORD)(corner - br);
		SetGadgetAttrs((struct Gadget *)gwin->objects[GID_CHROMELAYOUT],
				win, NULL,
				LAYOUT_ModifyChild, gwin->objects[GID_CHROME_RPAD],
				CHILD_MinWidth, (ULONG)pad,
				CHILD_MaxWidth, (ULONG)pad,
				TAG_DONE);
		FlushLayoutDomainCache((struct Gadget *)gwin->objects[GID_MAIN]);
		RethinkLayout((struct Gadget *)gwin->objects[GID_MAIN],
				win, NULL, TRUE);
		/* Chrome height may have settled after rethink */
		if(gwin->objects[GID_CHROMELAYOUT] != NULL) {
			cgad = (struct Gadget *)gwin->objects[GID_CHROMELAYOUT];
			if(cgad->Height > 0) {
				chtop = cgad->TopEdge;
				chh = cgad->Height;
			}
		}
	}

	/* Start under chrome, not at BorderTop */
	vtop = (WORD)(chtop + chh + 2);
	if(vtop < (WORD)(bt + 2))
		vtop = (WORD)(bt + 2);

	dri = GetScreenDrawInfo(scrn);
	if(dri == NULL)
		return;

	gwin->objects[GID_SCROLL_DOWN_BM] = NewObject(NULL, "sysiclass",
			SYSIA_DrawInfo, dri,
			SYSIA_Which, DOWNIMAGE,
			TAG_DONE);
	gwin->objects[GID_SCROLL_UP_BM] = NewObject(NULL, "sysiclass",
			SYSIA_DrawInfo, dri,
			SYSIA_Which, UPIMAGE,
			TAG_DONE);
	gwin->objects[GID_SCROLL_RIGHT_BM] = NewObject(NULL, "sysiclass",
			SYSIA_DrawInfo, dri,
			SYSIA_Which, RIGHTIMAGE,
			TAG_DONE);
	gwin->objects[GID_SCROLL_LEFT_BM] = NewObject(NULL, "sysiclass",
			SYSIA_DrawInfo, dri,
			SYSIA_Which, LEFTIMAGE,
			TAG_DONE);
	FreeScreenDrawInfo(scrn, dri);

	if(gwin->objects[GID_SCROLL_DOWN_BM] == NULL ||
	   gwin->objects[GID_SCROLL_UP_BM] == NULL ||
	   gwin->objects[GID_SCROLL_RIGHT_BM] == NULL ||
	   gwin->objects[GID_SCROLL_LEFT_BM] == NULL) {
		ami_gui_nami_destroy_border_scrollers(gwin);
		return;
	}

	/*
	 * Zoom then depth, flush to the window's top-right corner (y = 0,
	 * right edge).  Not in the chrome layout — that offsets them down
	 * into the client area.  Images are the ones built at window create;
	 * detach GA_Image before dispose so they are freed once.
	 */
	if(gwin->objects[GID_WIN_ZOOM] == NULL &&
	   gwin->objects[GID_WIN_ZOOM_BM] != NULL) {
		gwin->objects[GID_WIN_ZOOM] = NewObject(NULL, "buttongclass",
				GA_RelRight, (LONG)(-(LONG)(zw + dw) + 1),
				GA_Top, 0,
				GA_Width, (ULONG)zw,
				GA_Height, (ULONG)zhh,
				GA_Image, gwin->objects[GID_WIN_ZOOM_BM],
				GA_ID, GID_WIN_ZOOM,
				GA_Immediate, TRUE,
				GA_RelVerify, TRUE,
				TAG_DONE);
		if(gwin->objects[GID_WIN_ZOOM] != NULL) {
			((struct Gadget *)gwin->objects[GID_WIN_ZOOM])->Flags |=
					GFLG_RELRIGHT;
			gwin->nami_border_depth = true;
		}
	}
	if(gwin->objects[GID_WIN_DEPTH] == NULL &&
	   gwin->objects[GID_WIN_DEPTH_BM] != NULL) {
		gwin->objects[GID_WIN_DEPTH] = NewObject(NULL, "buttongclass",
				GA_RelRight, (LONG)(-(LONG)dw + 1),
				GA_Top, 0,
				GA_Width, (ULONG)dw,
				GA_Height, (ULONG)dhh,
				GA_Image, gwin->objects[GID_WIN_DEPTH_BM],
				GA_ID, GID_WIN_DEPTH,
				GA_Immediate, TRUE,
				GA_RelVerify, TRUE,
				TAG_DONE);
		if(gwin->objects[GID_WIN_DEPTH] != NULL) {
			((struct Gadget *)gwin->objects[GID_WIN_DEPTH])->Flags |=
					GFLG_RELRIGHT;
			gwin->nami_border_depth = true;
		}
	}

	dh = ((struct Image *)gwin->objects[GID_SCROLL_DOWN_BM])->Height;
	uh = ((struct Image *)gwin->objects[GID_SCROLL_UP_BM])->Height;
	rw = ((struct Image *)gwin->objects[GID_SCROLL_RIGHT_BM])->Width;
	lw = ((struct Image *)gwin->objects[GID_SCROLL_LEFT_BM])->Width;

	/* Arrow buttons then prop knobs in the borders */
	gwin->objects[GID_SCROLL_DOWN] = NewObject(NULL, "buttongclass",
			GA_RelRight, (LONG)(-(br) + 1),
			GA_RelBottom, (LONG)(-(bb) - dh + 1),
			GA_Image, gwin->objects[GID_SCROLL_DOWN_BM],
			GA_ID, GID_SCROLL_DOWN,
			GA_Immediate, TRUE,
			GA_RelVerify, TRUE,
			ICA_TARGET, ICTARGET_IDCMP,
			TAG_DONE);
	prev = gwin->objects[GID_SCROLL_DOWN];

	gwin->objects[GID_SCROLL_UP] = NewObject(NULL, "buttongclass",
			GA_RelRight, (LONG)(-(br) + 1),
			GA_RelBottom, (LONG)(-(bb) - dh - uh + 1),
			GA_Image, gwin->objects[GID_SCROLL_UP_BM],
			GA_ID, GID_SCROLL_UP,
			GA_Immediate, TRUE,
			GA_RelVerify, TRUE,
			ICA_TARGET, ICTARGET_IDCMP,
			GA_Previous, prev,
			TAG_DONE);
	prev = gwin->objects[GID_SCROLL_UP];

	gwin->objects[GID_VSCROLL] = NewObject(NULL, "propgclass",
			GA_RelRight, (LONG)(-(br) + 5),
			GA_Width, (ULONG)(br - 8),
			GA_Top, (ULONG)vtop,
			GA_RelHeight, (LONG)(-(vtop + bb + dh + uh + 2)),
			GA_ID, GID_VSCROLL,
			PGA_Freedom, FREEVERT,
			PGA_Total, 1,
			PGA_Visible, 1,
			PGA_NewLook, TRUE,
			PGA_Borderless, TRUE,
			ICA_TARGET, ICTARGET_IDCMP,
			GA_Previous, prev,
			TAG_DONE);
	prev = gwin->objects[GID_VSCROLL];

	gwin->objects[GID_SCROLL_RIGHT] = NewObject(NULL, "buttongclass",
			GA_RelRight, (LONG)(-(br) - rw + 1),
			GA_RelBottom, (LONG)(-(bb) + 1),
			GA_Image, gwin->objects[GID_SCROLL_RIGHT_BM],
			GA_ID, GID_SCROLL_RIGHT,
			GA_Immediate, TRUE,
			GA_RelVerify, TRUE,
			ICA_TARGET, ICTARGET_IDCMP,
			GA_Previous, prev,
			TAG_DONE);
	prev = gwin->objects[GID_SCROLL_RIGHT];

	gwin->objects[GID_SCROLL_LEFT] = NewObject(NULL, "buttongclass",
			GA_RelRight, (LONG)(-(br) - rw - lw + 1),
			GA_RelBottom, (LONG)(-(bb) + 1),
			GA_Image, gwin->objects[GID_SCROLL_LEFT_BM],
			GA_ID, GID_SCROLL_LEFT,
			GA_Immediate, TRUE,
			GA_RelVerify, TRUE,
			ICA_TARGET, ICTARGET_IDCMP,
			GA_Previous, prev,
			TAG_DONE);
	prev = gwin->objects[GID_SCROLL_LEFT];

	gwin->objects[GID_HSCROLL] = NewObject(NULL, "propgclass",
			GA_Left, (ULONG)bl,
			GA_RelBottom, (LONG)(-(bb) + 3),
			GA_RelWidth, (LONG)(-(bl + br + rw + lw + 2)),
			GA_Height, (ULONG)(bb - 4),
			GA_ID, GID_HSCROLL,
			PGA_Freedom, FREEHORIZ,
			PGA_Total, 1,
			PGA_Visible, 1,
			PGA_NewLook, TRUE,
			PGA_Borderless, TRUE,
			ICA_TARGET, ICTARGET_IDCMP,
			GA_Previous, prev,
			TAG_DONE);

	if(gwin->objects[GID_SCROLL_DOWN] == NULL ||
	   gwin->objects[GID_SCROLL_UP] == NULL ||
	   gwin->objects[GID_VSCROLL] == NULL ||
	   gwin->objects[GID_SCROLL_RIGHT] == NULL ||
	   gwin->objects[GID_SCROLL_LEFT] == NULL ||
	   gwin->objects[GID_HSCROLL] == NULL) {
		ami_gui_nami_destroy_border_scrollers(gwin);
		return;
	}

	gwin->nami_vprop_shift = 0;
	gwin->nami_hprop_shift = 0;

	/* Zoom/Depth first so they sit above the SizeBRight fill. */
	if(gwin->nami_border_depth && gwin->objects[GID_WIN_ZOOM] != NULL) {
		AddGList(win, (struct Gadget *)gwin->objects[GID_WIN_ZOOM],
				(UWORD)~0, 1, NULL);
	}
	if(gwin->nami_border_depth && gwin->objects[GID_WIN_DEPTH] != NULL) {
		AddGList(win, (struct Gadget *)gwin->objects[GID_WIN_DEPTH],
				(UWORD)~0, 1, NULL);
	}

	/* One AddGList of the GA_Previous scroller chain */
	AddGList(win, (struct Gadget *)gwin->objects[GID_SCROLL_DOWN],
			(UWORD)~0, -1, NULL);
	RefreshGList((struct Gadget *)gwin->objects[GID_SCROLL_DOWN],
			win, NULL, -1);
	if(gwin->nami_border_depth && gwin->objects[GID_WIN_ZOOM] != NULL)
		RefreshGList((struct Gadget *)gwin->objects[GID_WIN_ZOOM],
				win, NULL, 1);
	if(gwin->nami_border_depth && gwin->objects[GID_WIN_DEPTH] != NULL)
		RefreshGList((struct Gadget *)gwin->objects[GID_WIN_DEPTH],
				win, NULL, 1);
	ami_gui_nami_paint_chrome_rborder(gwin);
}

static void ami_gui_nami_destroy_border_scrollers(struct gui_window_2 *gwin)
{
	struct Gadget *gad;
	struct Gadget *scan;
	BOOL inwin;
	Object *depth_img;
	Object *zoom_img;
	Object *down_img;
	Object *up_img;
	Object *right_img;
	Object *left_img;
	ULONG img_ptr;

	if(gwin == NULL || gwin->ui_nami == false)
		return;

	/*
	 * buttongclass may free GA_Image on DisposeObject.  Snapshot each
	 * image, detach (GA_Image NULL), dispose gadgets, then dispose each
	 * image exactly once.  Double DisposeObject of sysi images was a
	 * MemList corruption source that showed up on the next launch.
	 */
	depth_img = NULL;
	zoom_img = NULL;
	down_img = NULL;
	up_img = NULL;
	right_img = NULL;
	left_img = NULL;

	inwin = FALSE;
	if(gwin->nami_border_depth &&
	   gwin->win != NULL && gwin->objects[GID_WIN_DEPTH] != NULL) {
		gad = (struct Gadget *)gwin->objects[GID_WIN_DEPTH];
		for(scan = gwin->win->FirstGadget; scan != NULL; scan = scan->NextGadget) {
			if(scan == gad) {
				inwin = TRUE;
				break;
			}
		}
		if(inwin)
			RemoveGList(gwin->win, gad, 1);
	}
	if(gwin->nami_border_depth && gwin->objects[GID_WIN_DEPTH] != NULL) {
		img_ptr = 0;
		GetAttr(GA_Image, gwin->objects[GID_WIN_DEPTH], &img_ptr);
		depth_img = (Object *)img_ptr;
		SetAttrs(gwin->objects[GID_WIN_DEPTH], GA_Image, NULL, TAG_DONE);
		((struct Gadget *)gwin->objects[GID_WIN_DEPTH])->NextGadget = NULL;
		DisposeObject(gwin->objects[GID_WIN_DEPTH]);
		gwin->objects[GID_WIN_DEPTH] = NULL;
	}
	inwin = FALSE;
	if(gwin->nami_border_depth &&
	   gwin->win != NULL && gwin->objects[GID_WIN_ZOOM] != NULL) {
		gad = (struct Gadget *)gwin->objects[GID_WIN_ZOOM];
		for(scan = gwin->win->FirstGadget; scan != NULL; scan = scan->NextGadget) {
			if(scan == gad) {
				inwin = TRUE;
				break;
			}
		}
		if(inwin)
			RemoveGList(gwin->win, gad, 1);
	}
	if(gwin->nami_border_depth && gwin->objects[GID_WIN_ZOOM] != NULL) {
		img_ptr = 0;
		GetAttr(GA_Image, gwin->objects[GID_WIN_ZOOM], &img_ptr);
		zoom_img = (Object *)img_ptr;
		SetAttrs(gwin->objects[GID_WIN_ZOOM], GA_Image, NULL, TAG_DONE);
		((struct Gadget *)gwin->objects[GID_WIN_ZOOM])->NextGadget = NULL;
		DisposeObject(gwin->objects[GID_WIN_ZOOM]);
		gwin->objects[GID_WIN_ZOOM] = NULL;
	}
	gwin->nami_border_depth = false;

	inwin = FALSE;
	if(gwin->win != NULL && gwin->objects[GID_SCROLL_DOWN] != NULL) {
		gad = (struct Gadget *)gwin->objects[GID_SCROLL_DOWN];
		for(scan = gwin->win->FirstGadget; scan != NULL; scan = scan->NextGadget) {
			if(scan == gad) {
				inwin = TRUE;
				break;
			}
		}
		if(inwin)
			RemoveGList(gwin->win, gad, -1);
	}

	/* Snapshot + detach images before disposing buttongclass */
	if(gwin->objects[GID_SCROLL_DOWN] != NULL) {
		img_ptr = 0;
		GetAttr(GA_Image, gwin->objects[GID_SCROLL_DOWN], &img_ptr);
		down_img = (Object *)img_ptr;
		SetAttrs(gwin->objects[GID_SCROLL_DOWN], GA_Image, NULL, TAG_DONE);
		((struct Gadget *)gwin->objects[GID_SCROLL_DOWN])->NextGadget = NULL;
	}
	if(gwin->objects[GID_SCROLL_UP] != NULL) {
		img_ptr = 0;
		GetAttr(GA_Image, gwin->objects[GID_SCROLL_UP], &img_ptr);
		up_img = (Object *)img_ptr;
		SetAttrs(gwin->objects[GID_SCROLL_UP], GA_Image, NULL, TAG_DONE);
		((struct Gadget *)gwin->objects[GID_SCROLL_UP])->NextGadget = NULL;
	}
	if(gwin->objects[GID_VSCROLL] != NULL)
		((struct Gadget *)gwin->objects[GID_VSCROLL])->NextGadget = NULL;
	if(gwin->objects[GID_SCROLL_RIGHT] != NULL) {
		img_ptr = 0;
		GetAttr(GA_Image, gwin->objects[GID_SCROLL_RIGHT], &img_ptr);
		right_img = (Object *)img_ptr;
		SetAttrs(gwin->objects[GID_SCROLL_RIGHT], GA_Image, NULL, TAG_DONE);
		((struct Gadget *)gwin->objects[GID_SCROLL_RIGHT])->NextGadget = NULL;
	}
	if(gwin->objects[GID_SCROLL_LEFT] != NULL) {
		img_ptr = 0;
		GetAttr(GA_Image, gwin->objects[GID_SCROLL_LEFT], &img_ptr);
		left_img = (Object *)img_ptr;
		SetAttrs(gwin->objects[GID_SCROLL_LEFT], GA_Image, NULL, TAG_DONE);
		((struct Gadget *)gwin->objects[GID_SCROLL_LEFT])->NextGadget = NULL;
	}
	if(gwin->objects[GID_HSCROLL] != NULL)
		((struct Gadget *)gwin->objects[GID_HSCROLL])->NextGadget = NULL;

	if(gwin->objects[GID_HSCROLL] != NULL) {
		DisposeObject(gwin->objects[GID_HSCROLL]);
		gwin->objects[GID_HSCROLL] = NULL;
	}
	if(gwin->objects[GID_SCROLL_LEFT] != NULL) {
		DisposeObject(gwin->objects[GID_SCROLL_LEFT]);
		gwin->objects[GID_SCROLL_LEFT] = NULL;
	}
	if(gwin->objects[GID_SCROLL_RIGHT] != NULL) {
		DisposeObject(gwin->objects[GID_SCROLL_RIGHT]);
		gwin->objects[GID_SCROLL_RIGHT] = NULL;
	}
	if(gwin->objects[GID_VSCROLL] != NULL) {
		DisposeObject(gwin->objects[GID_VSCROLL]);
		gwin->objects[GID_VSCROLL] = NULL;
	}
	if(gwin->objects[GID_SCROLL_UP] != NULL) {
		DisposeObject(gwin->objects[GID_SCROLL_UP]);
		gwin->objects[GID_SCROLL_UP] = NULL;
	}
	if(gwin->objects[GID_SCROLL_DOWN] != NULL) {
		DisposeObject(gwin->objects[GID_SCROLL_DOWN]);
		gwin->objects[GID_SCROLL_DOWN] = NULL;
	}

	/* Dispose each image once; clear BM slots (may alias the snapshots) */
	if(down_img != NULL)
		DisposeObject(down_img);
	else if(gwin->objects[GID_SCROLL_DOWN_BM] != NULL)
		DisposeObject(gwin->objects[GID_SCROLL_DOWN_BM]);
	gwin->objects[GID_SCROLL_DOWN_BM] = NULL;

	if(up_img != NULL)
		DisposeObject(up_img);
	else if(gwin->objects[GID_SCROLL_UP_BM] != NULL)
		DisposeObject(gwin->objects[GID_SCROLL_UP_BM]);
	gwin->objects[GID_SCROLL_UP_BM] = NULL;

	if(right_img != NULL)
		DisposeObject(right_img);
	else if(gwin->objects[GID_SCROLL_RIGHT_BM] != NULL)
		DisposeObject(gwin->objects[GID_SCROLL_RIGHT_BM]);
	gwin->objects[GID_SCROLL_RIGHT_BM] = NULL;

	if(left_img != NULL)
		DisposeObject(left_img);
	else if(gwin->objects[GID_SCROLL_LEFT_BM] != NULL)
		DisposeObject(gwin->objects[GID_SCROLL_LEFT_BM]);
	gwin->objects[GID_SCROLL_LEFT_BM] = NULL;

	if(depth_img != NULL &&
	   depth_img != gwin->objects[GID_WIN_DEPTH_BM]) {
		DisposeObject(depth_img);
	}
	if(zoom_img != NULL &&
	   zoom_img != gwin->objects[GID_WIN_ZOOM_BM]) {
		DisposeObject(zoom_img);
	}
	/*
	 * Chrome still uses GID_WIN_DEPTH_BM until OID_MAIN is disposed —
	 * leave it for gui_window_destroy.
	 */
}

/**
 * Apply PGA_Total/Visible/Top with optional >64k shift.
 */
static void ami_gui_nami_set_prop(struct gui_window_2 *gwin, Object *gad,
		UBYTE *shift_io, ULONG total, ULONG vis, ULONG top, BOOL set_top)
{
	ULONG stotal, svis, stop;
	UBYTE shift;

	if(gad == NULL || gwin->win == NULL)
		return;

	stotal = total;
	svis = vis;
	shift = 0;
	while(stotal > 0xffffUL) {
		stotal >>= 1;
		shift++;
	}
	svis >>= shift;
	if(svis < 1)
		svis = 1;
	*shift_io = shift;

	if(set_top) {
		stop = top >> shift;
		RefreshSetGadgetAttrs((struct Gadget *)gad, gwin->win, NULL,
				PGA_Total, stotal,
				PGA_Visible, svis,
				PGA_Top, stop,
				TAG_DONE);
	} else {
		RefreshSetGadgetAttrs((struct Gadget *)gad, gwin->win, NULL,
				PGA_Total, stotal,
				PGA_Visible, svis,
				TAG_DONE);
	}
}

/**
 * Check the scroll bar requirements for a browser window, and add/remove
 * the vertical scroller as appropriate.  This should be the main entry
 * point used to perform this task.
 *
 * \param  gwin      "Shared" GUI window to check the state of
 */
static void ami_gui_scroller_update(struct gui_window_2 *gwin)
{
	int h = 1, w = 1, wh = 0, ww = 0;
	bool rethinkv = false;
	bool rethinkh = false;
	browser_scrolling hscroll = BW_SCROLLING_YES;
	browser_scrolling vscroll = BW_SCROLLING_YES;

	/* Nami border scrollers are permanent; extent attrs update separately */
	if(gwin->ui_nami) {
		ami_gui_nami_create_border_scrollers(gwin);
		return;
	}

	browser_window_get_scrollbar_type(gwin->gw->bw, &hscroll, &vscroll);

	if(browser_window_is_frameset(gwin->gw->bw) == true) {
		rethinkv = ami_gui_vscroll_remove(gwin);
		rethinkh = ami_gui_hscroll_remove(gwin);
	} else {
		if((browser_window_get_extents(gwin->gw->bw, false, &w, &h) == NSERROR_OK)) {
			gui_window_get_dimensions(gwin->gw, &ww, &wh);
		}

		if(vscroll == BW_SCROLLING_NO) {
			rethinkv = ami_gui_vscroll_remove(gwin);
		} else {
			if (h > wh) rethinkv = ami_gui_vscroll_add(gwin);
				else rethinkv = ami_gui_vscroll_remove(gwin);
		}

		if(hscroll == BW_SCROLLING_NO) {
			rethinkh = ami_gui_hscroll_remove(gwin);
		} else {
			if (w > ww) rethinkh = ami_gui_hscroll_add(gwin);
				else rethinkh = ami_gui_hscroll_remove(gwin);
		}
	}

	if(rethinkv || rethinkh) {
		FlushLayoutDomainCache((struct Gadget *)gwin->objects[GID_MAIN]);
		RethinkLayout((struct Gadget *)gwin->objects[GID_MAIN],
				gwin->win, NULL, TRUE);
		browser_window_schedule_reformat(gwin->gw->bw);
	}
}

/* For future use
static void ami_gui_console_log_clear(struct gui_window *g)
{
	if(g->shared->objects[GID_LOG] != NULL) {
		SetGadgetAttrs((struct Gadget *)g->shared->objects[GID_LOG], g->shared->win, NULL,
						LISTBROWSER_Labels, NULL,
						TAG_DONE);
	}
	
	FreeListBrowserList(&g->loglist);

	NewList(&g->loglist);

	if(g->shared->objects[GID_LOG] != NULL) {
		SetGadgetAttrs((struct Gadget *)g->shared->objects[GID_LOG], g->shared->win, NULL,
						LISTBROWSER_Labels, &g->loglist,
						TAG_DONE);
	}
}
*/

static void ami_gui_console_log_add(struct gui_window *g)
{
	struct TagItem attrs[2];

	if(g->shared->objects[GID_LOG] != NULL) return;

	attrs[0].ti_Tag = CHILD_MinHeight;
	attrs[0].ti_Data = 50;
	attrs[1].ti_Tag = TAG_DONE;
	attrs[1].ti_Data = 0;

	g->shared->objects[GID_LOG] = ListBrowserObj,
					GA_ID, GID_LOG,
					LISTBROWSER_ColumnInfo, g->logcolumns,
					LISTBROWSER_ColumnTitles, TRUE,
					LISTBROWSER_Labels, &g->loglist,
					LISTBROWSER_Striping, LBS_ROWS,
				ListBrowserEnd;

#ifdef __amigaos4__
	IDoMethod(g->shared->objects[GID_LOGLAYOUT], LM_ADDCHILD,
			g->shared->win, g->shared->objects[GID_LOG], NULL);
#else
	SetGadgetAttrs((struct Gadget *)g->shared->objects[GID_LOGLAYOUT],
		g->shared->win, NULL,
		LAYOUT_AddChild, g->shared->objects[GID_LOG], TAG_MORE, &attrs);
#endif

	FlushLayoutDomainCache((struct Gadget *)g->shared->objects[GID_MAIN]);

	RethinkLayout((struct Gadget *)g->shared->objects[GID_MAIN],
			g->shared->win, NULL, TRUE);
		
	ami_schedule_redraw(g->shared, true);
}

static void ami_gui_console_log_remove(struct gui_window *g)
{
	if(g->shared->objects[GID_LOG] == NULL) return;

#ifdef __amigaos4__
	IDoMethod(g->shared->objects[GID_LOGLAYOUT], LM_REMOVECHILD,
			g->shared->win, g->shared->objects[GID_LOG]);
#else
	SetGadgetAttrs((struct Gadget *)g->shared->objects[GID_LOGLAYOUT],
		g->shared->win, NULL,
		LAYOUT_RemoveChild, g->shared->objects[GID_LOG], TAG_DONE);
#endif

	g->shared->objects[GID_LOG] = NULL;

	FlushLayoutDomainCache((struct Gadget *)g->shared->objects[GID_MAIN]);

	RethinkLayout((struct Gadget *)g->shared->objects[GID_MAIN],
			g->shared->win, NULL, TRUE);

	ami_schedule_redraw(g->shared, true);
}

static bool ami_gui_console_log_toggle(struct gui_window *g)
{
	if(g->shared->objects[GID_LOG] == NULL) {
		ami_gui_console_log_add(g);
		return true;
	} else {
		ami_gui_console_log_remove(g);
		return false;
	}
}

static void ami_gui_console_log_switch(struct gui_window *g)
{
	if(g->shared->objects[GID_LOG] == NULL) return;

	RefreshSetGadgetAttrs((struct Gadget *)g->shared->objects[GID_LOG], g->shared->win, NULL,
					LISTBROWSER_ColumnInfo, g->logcolumns,
					LISTBROWSER_Labels, &g->loglist,
					TAG_DONE);
}

static void
gui_window_console_log(struct gui_window *g,
		       browser_window_console_source src,
		       const char *msg,
		       size_t msglen,
		       browser_window_console_flags flags)
{
	bool foldable = !!(flags & BW_CS_FLAG_FOLDABLE);
	const char *src_text;
	const char *level_text;
	struct Node *node;
	ULONG style = 0;
	ULONG fgpen = TEXTPEN;
	ULONG lbflags = LBFLG_READONLY;
	char timestamp[256];
	time_t now = time(NULL);
	struct tm *timedata = localtime(&now);

	strftime(timestamp, 256, "%c", timedata);

	if(foldable) lbflags |= LBFLG_HASCHILDREN;

	switch (src) {
	case BW_CS_INPUT:
		src_text = "client-input";
		break;
	case BW_CS_SCRIPT_ERROR:
		src_text = "scripting-error";
		break;
	case BW_CS_SCRIPT_CONSOLE:
		src_text = "scripting-console";
		break;
	default:
		assert(0 && "Unknown scripting source");
		src_text = "unknown";
		break;
	}

	switch (flags & BW_CS_FLAG_LEVEL_MASK) {
	case BW_CS_FLAG_LEVEL_DEBUG:
		level_text = "DEBUG";
		fgpen = DISABLEDTEXTPEN;
		lbflags |= LBFLG_CUSTOMPENS;
		break;
	case BW_CS_FLAG_LEVEL_LOG:
		level_text = "LOG";
		fgpen = DISABLEDTEXTPEN;
		lbflags |= LBFLG_CUSTOMPENS;
		break;
	case BW_CS_FLAG_LEVEL_INFO:
		level_text = "INFO";
		break;
	case BW_CS_FLAG_LEVEL_WARN:
		level_text = "WARN";
		break;
	case BW_CS_FLAG_LEVEL_ERROR:
		level_text = "ERROR";
		style = FSF_BOLD;
		break;
	default:
		assert(0 && "Unknown console logging level");
		level_text = "unknown";
		break;
	}

	if(g->shared->objects[GID_LOG] != NULL) {
		SetGadgetAttrs((struct Gadget *)g->shared->objects[GID_LOG], g->shared->win, NULL,
						LISTBROWSER_Labels, NULL,
						TAG_DONE);
	}

	/* Add log entry to list irrespective of whether the log is open. */
	if((node = AllocListBrowserNode(4,
				LBNA_Flags, lbflags,
				LBNA_Column, 0,
					LBNCA_SoftStyle, style,
					LBNCA_FGPen, fgpen,
					LBNCA_CopyText, TRUE,
					LBNCA_Text, timestamp,
				LBNA_Column, 1,
					LBNCA_SoftStyle, style,
					LBNCA_FGPen, fgpen,
					LBNCA_CopyText, TRUE,
					LBNCA_Text, src_text,
				LBNA_Column, 2,
					LBNCA_SoftStyle, style,
					LBNCA_FGPen, fgpen,
					LBNCA_CopyText, TRUE,
					LBNCA_Text, level_text,
				LBNA_Column, 3,
					LBNCA_SoftStyle, style,
					LBNCA_FGPen, fgpen,
					LBNCA_CopyText, TRUE,
					LBNCA_Text, msg,
				TAG_DONE))) {
		AddTail(&g->loglist, node);
	}

	if(g->shared->objects[GID_LOG] != NULL) {
		RefreshSetGadgetAttrs((struct Gadget *)g->shared->objects[GID_LOG], g->shared->win, NULL,
						LISTBROWSER_Labels, &g->loglist,
						TAG_DONE);
	}

#ifdef __amigaos4__
	DebugPrintF("NETSURF: CONSOLE_LOG SOURCE %s %sFOLDABLE %s %.*s\n",
	      src_text, foldable ? "" : "NOT-", level_text,
	      (int)msglen, msg);
#endif
}


/**
 * function to add retrieved favicon to gui
 *
 * clicktab TNA_Image is declared in gadgets/clicktab.h but unimplemented
 * (shows nothing on OS3/OS4).  Draw beside the URL bar instead.  Close
 * gadgets use CLICKTAB_CloseImage from sysiclass and are unrelated.
 *
 * Nami: this slot is also the hotlist control — with no page favicon the
 * AISS +hotlist (or remove) glyph is drawn; click toggles the hotlist.
 */
static void gui_window_set_icon(struct gui_window *g, struct hlcache_handle *icon)
{
	struct BitMap *bm = NULL;
	struct IBox *bbox;
	struct bitmap *icon_bitmap = NULL;
	Object *def_img;
	char *urlstr;
	nsurl *page_url;
	nsurl *hot_url;
	BOOL in_hotlist;
	struct DrawInfo *dri;
	char *iconname;
	char menu_icon[1024];
	Object *new_icon;

	if(nsoption_bool(kiosk_mode) == true) return;
	if(!g) return;

	g->favicon = icon;
	def_img = NULL;
	urlstr = NULL;
	page_url = NULL;
	hot_url = NULL;
	in_hotlist = FALSE;
	dri = NULL;
	iconname = NULL;
	new_icon = NULL;

	if ((icon != NULL) && ((icon_bitmap = content_get_bitmap(icon)) != NULL))
	{
		bm = ami_bitmap_get_native(icon_bitmap, 16, 16,
					ami_plot_screen_is_palettemapped(),
					g->shared->win->RPort->BitMap,
					nsoption_colour(sys_colour_ButtonFace));
	}

	/* Nami: refresh this tab's sidebar button favicon from the cache */
	if(g->shared->ui_nami && g->bw != NULL) {
		if(browser_window_get_url(g->bw, false, &page_url) == NSERROR_OK &&
		   page_url != NULL) {
			iconname = ami_gui_get_cache_favicon_name(page_url, true);
			if(iconname != NULL) {
				ami_locate_resource(menu_icon, iconname);
				new_icon = BitMapObj,
						BITMAP_SourceFile, menu_icon,
						BITMAP_Screen, scrn,
						BITMAP_Masking, TRUE,
						BITMAP_Width, 16,
						BITMAP_Height, 16,
					BitMapEnd;
			}
		}
		{
			Object *old_icon;

			old_icon = g->sidebar_icon;
			g->sidebar_icon = new_icon;
			if(g->sidebar_tab_slot >= 0)
				ami_gui_nami_sidebar_apply_label(g);
			if(old_icon != NULL)
				DisposeObject(old_icon);
		}
	}

	if((g != g->shared->gw) ||
	   (g->shared->objects[GID_ICON] == NULL))
		return;

	if(ami_gui_get_space_box((Object *)g->shared->objects[GID_ICON],
				&bbox) != NSERROR_OK) {
		amiga_warn_user("NoMemory", "");
		return;
	}

	EraseRect(g->shared->win->RPort, bbox->Left, bbox->Top,
				bbox->Left + 16, bbox->Top + 16);

	if(bm != NULL) {
		ULONG tag_data;
		ULONG minterm;

		tag_data = (ULONG)ami_bitmap_get_mask(icon_bitmap, 16, 16, bm);
		minterm = MINTERM_SRCMASK;

		if((icon_bitmap != NULL) &&
		   (amiga_bitmap_get_opaque(icon_bitmap) == false) &&
		   (tag_data != 0)) {
			BltMaskBitMapRastPort(bm, 0, 0, g->shared->win->RPort,
						bbox->Left, bbox->Top, 16, 16,
						minterm, (PLANEPTR)tag_data);
		} else {
			BltBitMapRastPort(bm, 0, 0, g->shared->win->RPort,
						bbox->Left, bbox->Top, 16, 16,
						0xc0);
		}
	} else if(g->shared->ui_nami) {
		/* No page favicon — show +hotlist / remove-hotlist glyph */
		GetAttr(STRINGA_TextVal,
			(Object *)g->shared->objects[GID_URL],
			(ULONG *)&urlstr);
		if(urlstr != NULL && nsurl_create(urlstr, &hot_url) == NSERROR_OK) {
			in_hotlist = hotlist_has_url(hot_url) ? TRUE : FALSE;
			nsurl_unref(hot_url);
		}
		def_img = in_hotlist
			? g->shared->objects[GID_FAVE_RMV]
			: g->shared->objects[GID_FAVE_ADD];
		if(def_img != NULL) {
			dri = GetScreenDrawInfo(scrn);
			if(dri != NULL) {
				DrawImageState(g->shared->win->RPort,
						(struct Image *)def_img,
						bbox->Left, bbox->Top,
						IDS_NORMAL, dri);
				FreeScreenDrawInfo(scrn, dri);
			}
		}
	}

	ami_gui_free_space_box(bbox);
}

static void ami_gui_refresh_favicon(void *p)
{
	struct gui_window_2 *gwin = (struct gui_window_2 *)p;
	gui_window_set_icon(gwin->gw, gwin->gw->favicon);
}

/* Gets the size that border gadget 1 (status) needs to be.
 * Returns the width of the size gadget as a convenience.
 */
#ifdef __amigaos4__
static ULONG ami_get_border_gadget_size(struct gui_window_2 *gwin,
		ULONG *restrict width, ULONG *restrict height)
{
	static ULONG sz_gad_width = 0;
	static ULONG sz_gad_height = 0;
	ULONG available_width;

	if((sz_gad_width == 0) || (sz_gad_height == 0)) {
		struct DrawInfo *dri = GetScreenDrawInfo(scrn);
		GetGUIAttrs(NULL, dri,
			GUIA_SizeGadgetWidth, &sz_gad_width,
			GUIA_SizeGadgetHeight, &sz_gad_height,
		TAG_DONE);
		FreeScreenDrawInfo(scrn, dri);
	}
	available_width = gwin->win->Width - scrn->WBorLeft - sz_gad_width;

	*width = available_width;
	*height = sz_gad_height;

	return sz_gad_width;
}
#endif

static void ami_set_border_gadget_size(struct gui_window_2 *gwin)
{
#ifdef __amigaos4__
	/* Reset gadget widths according to new calculation */
	ULONG size1, size2;

	if(gwin->objects[GID_STATUS] == NULL)
		return;

	ami_get_border_gadget_size(gwin, &size1, &size2);

	RefreshSetGadgetAttrs((struct Gadget *)(APTR)gwin->objects[GID_STATUS],
			gwin->win, NULL,
			GA_Width, size1,
			TAG_DONE);

	RefreshWindowFrame(gwin->win);
#endif
}

static BOOL ami_handle_msg(void)
{
	struct ami_generic_window *w = NULL;
	struct nsObject *node;
	struct nsObject *nnode;
	BOOL win_closed = FALSE;

	if(IsMinListEmpty(window_list)) {
		/* no windows in list, so NetSurf should not be running */
		ami_try_quit();
		return FALSE;
	}

	node = (struct nsObject *)GetHead((struct List *)window_list);

	do {
		nnode=(struct nsObject *)GetSucc((struct Node *)node);

		w = node->objstruct;
		if(w == NULL) continue;

		if(w->tbl->event != NULL) {
			if((win_closed = w->tbl->event(w))) {
				if((node->Type != AMINS_GUIOPTSWINDOW) ||
					((node->Type == AMINS_GUIOPTSWINDOW) && (scrn != NULL))) {
					ami_try_quit();
					break;
				}
			} else {
				node = nnode;
				continue;
			}
		}
	} while((node = nnode));

	if(ami_gui_menu_quit_selected() == true) {
		ami_quit_netsurf();
	}
	
	if(ami_gui_menu_get_check_toggled() == true) {
		ami_gui_menu_update_all();
	}

	return win_closed;
}

static BOOL ami_gui_event(void *w)
{
	struct gui_window_2 *gwin = (struct gui_window_2 *)w;
	ULONG result, storage = 0, x, y, xs, ys, width = 800, height = 600;
	uint16 code;
	struct IBox *bbox;
	struct InputEvent *ie;
	struct Node *tabnode;
	int nskey;
	struct timeval curtime;
	static int drag_x_move = 0, drag_y_move = 0;
	char *utf8 = NULL;
	nsurl *url;
	BOOL win_closed = FALSE;

	ami_gtdrag_poll_drops(gwin);

	while((result = RA_HandleInput(gwin->objects[OID_MAIN], &code)) != WMHI_LASTMSG) {
        switch(result & WMHI_CLASSMASK) // class
	   	{
			case WMHI_MOUSEMOVE:
				ami_gui_trap_mouse(gwin); /* re-assert mouse area */

				if(ami_gtdrag_armed()) {
					ami_gui_nami_gtdrag_apply_abs(gwin);
					ami_gtdrag_mouse_move(gwin->win);
				}

				if(gwin->chrome_dragging) {
					MoveWindow(gwin->win,
						gwin->win->MouseX - gwin->chrome_drag_offx,
						gwin->win->MouseY - gwin->chrome_drag_offy);
					if(gwin->objects[GID_CHROMELAYOUT] != NULL)
						RefreshGList((struct Gadget *)gwin->objects[GID_CHROMELAYOUT],
								gwin->win, NULL, 1);
					/* Border zoom/depth are outside the layout. */
					ami_gui_nami_refresh_corner_gadgets(gwin);
					ami_throbber_redraw_schedule(0, gwin->gw);
					break;
				}

				if(gwin->ui_nami)
					ami_gui_tb_set_hover(gwin, ami_gui_tb_hit(gwin));

				drag_x_move = 0;
				drag_y_move = 0;

				if(ami_gui_get_space_box((Object *)gwin->objects[GID_BROWSER], &bbox) != NSERROR_OK) {
					amiga_warn_user("NoMemory", "");
					break;
				}

				x = (ULONG)((gwin->win->MouseX - bbox->Left));
				y = (ULONG)((gwin->win->MouseY - bbox->Top));

				ami_get_hscroll_pos(gwin, (ULONG *)&xs);
				ami_get_vscroll_pos(gwin, (ULONG *)&ys);

				x += xs;
				y += ys;

				width=bbox->Width;
				height=bbox->Height;

				if(gwin->mouse_state & BROWSER_MOUSE_DRAG_ON)
				{
					if(ami_drag_icon_move() == TRUE) {
						if((gwin->win->MouseX < bbox->Left) &&
							((gwin->win->MouseX - bbox->Left) > -AMI_DRAG_THRESHOLD))
							drag_x_move = gwin->win->MouseX - bbox->Left;
						if((gwin->win->MouseX > (bbox->Left + bbox->Width)) &&
							((gwin->win->MouseX - (bbox->Left + bbox->Width)) < AMI_DRAG_THRESHOLD))
							drag_x_move = gwin->win->MouseX - (bbox->Left + bbox->Width);
						if((gwin->win->MouseY < bbox->Top) &&
							((gwin->win->MouseY - bbox->Top) > -AMI_DRAG_THRESHOLD))
							drag_y_move = gwin->win->MouseY - bbox->Top;
						if((gwin->win->MouseY > (bbox->Top + bbox->Height)) &&
							((gwin->win->MouseY - (bbox->Top + bbox->Height)) < AMI_DRAG_THRESHOLD))
							drag_y_move = gwin->win->MouseY - (bbox->Top + bbox->Height);
					}
				}

				ami_gui_free_space_box(bbox);

				if((x>=xs) && (y>=ys) && (x<width+xs) && (y<height+ys))
				{
					ami_update_quals(gwin);

					if(gwin->mouse_state & BROWSER_MOUSE_PRESS_1)
					{
						browser_window_mouse_track(gwin->gw->bw,BROWSER_MOUSE_DRAG_1 | gwin->key_state,x,y);
						gwin->mouse_state = BROWSER_MOUSE_HOLDING_1 | BROWSER_MOUSE_DRAG_ON;
					}
					else if(gwin->mouse_state & BROWSER_MOUSE_PRESS_2)
					{
						browser_window_mouse_track(gwin->gw->bw,BROWSER_MOUSE_DRAG_2 | gwin->key_state,x,y);
						gwin->mouse_state = BROWSER_MOUSE_HOLDING_2 | BROWSER_MOUSE_DRAG_ON;
					}
					else
					{
						browser_window_mouse_track(gwin->gw->bw,gwin->mouse_state | gwin->key_state,x,y);
					}
				} else {
					if(!gwin->mouse_state) ami_set_pointer(gwin, GUI_POINTER_DEFAULT, true);
				}
			break;

			case WMHI_MOUSEBUTTONS:
				if(gwin->ui_nami) {
					if(code == SELECTDOWN) {
						/* Fresh press — clear FakeInputEvent re-arm block */
						ami_gtdrag_note_lmb_up();
						/* Prefer tab-drag arm before chrome drag strip */
						if(ami_gui_nami_gtdrag_try_arm_ex(gwin, true))
							break;
						if(ami_gui_nami_chrome_down(gwin))
							break;
					} else if(code == SELECTUP) {
						/*
						 * Unlock if still armed (Fake twin may hit WMHI
						 * before the hook).  Always unlock the armed
						 * window — drop may be over another.  Never
						 * poll here; IDCMP path finishes the drop.
						 */
						if(ami_gtdrag_armed()) {
							struct Window *arm_win;
							struct gui_window_2 *arm_gwin;

							arm_win = ami_gtdrag_arm_window();
							arm_gwin = (arm_win != NULL) ?
									ami_find_gwin_by_id(arm_win,
										AMINS_WINDOW) : NULL;
							if(arm_gwin != NULL)
								ami_gui_nami_gtdrag_apply_abs(arm_gwin);
							(void)ami_gtdrag_select_up(arm_win);
							ami_gui_nami_gtdrag_restore_applied();
							ami_gtdrag_discard_drops();
							ami_gui_nami_sidebar_drop_wells_refresh_all();
						} else {
							Object *side_obj;
							struct Gadget *side_gad;
							int mx;

							ami_gtdrag_note_lmb_up();
							ami_gui_nami_gtdrag_restore_applied();
							/*
							 * Weight-bar grab ends with SELECTUP near the
							 * panel's right edge — sync sidebar layout.
							 */
							side_obj = gwin->objects[GID_SIDELAYOUT];
							side_gad = (struct Gadget *)side_obj;
							if(side_gad != NULL && gwin->win != NULL) {
								mx = gwin->win->MouseX;
								if(mx >= (side_gad->LeftEdge +
										side_gad->Width - 6) &&
								   mx <= (side_gad->LeftEdge +
										side_gad->Width + 20))
									ami_gui_nami_sidebar_tab_rethink(gwin);
							}
						}
						if(ami_gui_nami_chrome_up(gwin, &win_closed))
							break;
					}
				}

				if(ami_gui_get_space_box((Object *)gwin->objects[GID_BROWSER], &bbox) != NSERROR_OK) {
					amiga_warn_user("NoMemory", "");
					return FALSE;
				}

				x = (ULONG)(gwin->win->MouseX - bbox->Left);
				y = (ULONG)(gwin->win->MouseY - bbox->Top);

				ami_get_hscroll_pos(gwin, (ULONG *)&xs);
				ami_get_vscroll_pos(gwin, (ULONG *)&ys);

				x += xs;
				y += ys;

				width=bbox->Width;
				height=bbox->Height;

				ami_gui_free_space_box(bbox);

				ami_update_quals(gwin);

				if((x>=xs) && (y>=ys) && (x<width+xs) && (y<height+ys))
				{
					//code = code>>16;
					switch(code)
					{
						case SELECTDOWN:
							browser_window_mouse_click(gwin->gw->bw,BROWSER_MOUSE_PRESS_1 | gwin->key_state,x,y);
							gwin->mouse_state=BROWSER_MOUSE_PRESS_1;
						break;
						case MIDDLEDOWN:
							browser_window_mouse_click(gwin->gw->bw,BROWSER_MOUSE_PRESS_2 | gwin->key_state,x,y);
							gwin->mouse_state=BROWSER_MOUSE_PRESS_2;
						break;
					}
				}

				if(x<xs) x=xs;
				if(y<ys) y=ys;
				if(x>=width+xs) x=width+xs-1;
				if(y>=height+ys) y=height+ys-1;

				switch(code)
				{
					case SELECTUP:
						if(gwin->mouse_state & BROWSER_MOUSE_PRESS_1)
						{
							CurrentTime((ULONG *)&curtime.tv_sec, (ULONG *)&curtime.tv_usec);

							gwin->mouse_state = BROWSER_MOUSE_CLICK_1;

							if(gwin->lastclick.tv_sec)
							{
								if(DoubleClick(gwin->lastclick.tv_sec,
											gwin->lastclick.tv_usec,
											curtime.tv_sec, curtime.tv_usec)) {
									if(gwin->prev_mouse_state & BROWSER_MOUSE_DOUBLE_CLICK) {
										gwin->mouse_state |= BROWSER_MOUSE_TRIPLE_CLICK;
									} else {
										gwin->mouse_state |= BROWSER_MOUSE_DOUBLE_CLICK;
									}
								}
							}

							browser_window_mouse_click(gwin->gw->bw,
								gwin->mouse_state | gwin->key_state,x,y);

							if(gwin->mouse_state & BROWSER_MOUSE_TRIPLE_CLICK)
							{
								gwin->lastclick.tv_sec = 0;
								gwin->lastclick.tv_usec = 0;
							}
							else
							{
								gwin->lastclick.tv_sec = curtime.tv_sec;
								gwin->lastclick.tv_usec = curtime.tv_usec;
							}
						}
						else
						{
							browser_window_mouse_track(gwin->gw->bw, 0, x, y);
						}
						gwin->prev_mouse_state = gwin->mouse_state;
						gwin->mouse_state=0;
					break;

					case MIDDLEUP:
						if(gwin->mouse_state & BROWSER_MOUSE_PRESS_2)
						{
							CurrentTime((ULONG *)&curtime.tv_sec, (ULONG *)&curtime.tv_usec);

							gwin->mouse_state = BROWSER_MOUSE_CLICK_2;

							if(gwin->lastclick.tv_sec)
							{
								if(DoubleClick(gwin->lastclick.tv_sec,
											gwin->lastclick.tv_usec,
											curtime.tv_sec, curtime.tv_usec)) {
									if(gwin->prev_mouse_state & BROWSER_MOUSE_DOUBLE_CLICK) {
										gwin->mouse_state |= BROWSER_MOUSE_TRIPLE_CLICK;
									} else {
										gwin->mouse_state |= BROWSER_MOUSE_DOUBLE_CLICK;
									}
								}
							}

							browser_window_mouse_click(gwin->gw->bw,
								gwin->mouse_state | gwin->key_state,x,y);

							if(gwin->mouse_state & BROWSER_MOUSE_TRIPLE_CLICK)
							{
								gwin->lastclick.tv_sec = 0;
								gwin->lastclick.tv_usec = 0;
							}
							else
							{
								gwin->lastclick.tv_sec = curtime.tv_sec;
								gwin->lastclick.tv_usec = curtime.tv_usec;
							}
						}
						else
						{
							browser_window_mouse_track(gwin->gw->bw, 0, x, y);
						}
						gwin->prev_mouse_state = gwin->mouse_state;
						gwin->mouse_state=0;
					break;
#ifdef __amigaos4__
					case SIDEUP:
						ami_gui_history(gwin, true);
					break;

					case EXTRAUP:
						ami_gui_history(gwin, false);
					break;
#endif
				}

				if(ami_drag_has_data() && !gwin->mouse_state)
					ami_drag_save(gwin->win);
			break;

			case WMHI_GADGETDOWN:
				/* Nami border arrows — mouse is outside the HTML view */
				switch(result & WMHI_GADGETMASK)
				{
					case GID_SCROLL_UP:
						ami_gui_scroll_viewport(gwin, 0, -NSA_WHEEL_SCROLL_PX);
					break;
					case GID_SCROLL_DOWN:
						ami_gui_scroll_viewport(gwin, 0, +NSA_WHEEL_SCROLL_PX);
					break;
					case GID_SCROLL_LEFT:
						ami_gui_scroll_viewport(gwin, -NSA_WHEEL_SCROLL_PX, 0);
					break;
					case GID_SCROLL_RIGHT:
						ami_gui_scroll_viewport(gwin, +NSA_WHEEL_SCROLL_PX, 0);
					break;
					case GID_BACK:
					case GID_FORWARD:
					case GID_STOP:
					case GID_RELOAD:
					case GID_PAGEINFO:
					case GID_ICON:
						if(gwin->ui_nami)
							ami_gui_tb_set_armed(gwin,
									(WORD)(result & WMHI_GADGETMASK));
					break;
					default:
					{
						ULONG gid_down;

						gid_down = result & WMHI_GADGETMASK;
						if(gwin->ui_nami &&
						   ami_gtdrag_available() &&
						   ((gid_down >= GID_SIDE_TAB_BASE &&
						     gid_down < (ULONG)(GID_SIDE_TAB_BASE +
								AMI_SIDE_TAB_MAX)) ||
						    (gid_down >= GID_SIDE_TAB_ICON_BASE &&
						     gid_down < (ULONG)(GID_SIDE_TAB_ICON_BASE +
								AMI_SIDE_TAB_MAX)))) {
							int ti;
							Object *btn;

							if(gid_down >= GID_SIDE_TAB_ICON_BASE)
								ti = (int)(gid_down - GID_SIDE_TAB_ICON_BASE);
							else
								ti = (int)(gid_down - GID_SIDE_TAB_BASE);
							btn = gwin->side_tab_btn[ti];
							if(btn != NULL &&
							   gwin->side_tab_gw[ti] != NULL) {
								ami_gui_nami_gtdrag_sync_bounds(gwin);
								ami_gtdrag_arm(
										(struct Gadget *)btn,
										gwin->win);
							}
						}
					}
					break;
				}
			break;

			case WMHI_GADGETUP:
				if(gwin->ui_nami && gwin->tb_armed_gid != 0)
					ami_gui_tb_set_armed(gwin, 0);

				switch(result & WMHI_GADGETMASK)
				{
					case GID_TABS:
						if(gwin->objects[GID_TABS] == NULL) break;
						tabnode = NULL;
						if(ami_clicktab_has_close()) {
							GetAttr(CLICKTAB_NodeClosed,
								(Object *)gwin->objects[GID_TABS],
								(ULONG *)&tabnode);
						}

						if(tabnode) {
							ami_gui_close_clicktab_node(gwin, tabnode);
						} else {
							GetAttr(CLICKTAB_CurrentNode, (Object *)gwin->objects[GID_TABS], (ULONG *)&tabnode);
							if(tabnode != gwin->new_tab_tab) {
								ami_switch_tab(gwin, true);
							} else {
								ami_gui_new_blank_tab(gwin);
							}
						}
					break;

					case GID_SIDE_NEWTAB:
						ami_schedule(0, ami_gui_nami_new_tab_cb, gwin);
					break;

					case GID_SIDE_CLOSETAB:
						ami_schedule(0, ami_gui_nami_close_tab_cb, gwin);
					break;

					case GID_SIDE_TOGGLE:
						/* Defer layout rethink out of the gadget-up path */
						ami_schedule(0, ami_gui_nami_sidebar_toggle_cb, gwin);
					break;

					case GID_SIDE_STRIP:
						/* Legacy strip — unused; toggle opens sidebar */
					break;

					case GID_WIN_CLOSE:
						ami_gui_nami_chrome_activate(gwin, GID_WIN_CLOSE, &win_closed);
						if(win_closed)
							return TRUE;
					break;

					case GID_WIN_ZOOM:
						ami_gui_nami_chrome_activate(gwin, GID_WIN_ZOOM, NULL);
					break;

					case GID_WIN_DEPTH:
						ami_gui_nami_chrome_activate(gwin, GID_WIN_DEPTH, NULL);
					break;

					case GID_CLOSETAB:
						browser_window_destroy(gwin->gw->bw);
					break;

					case GID_URL:
					{
						nserror ret;
						nsurl *url;
						GetAttr(STRINGA_TextVal,
							(Object *)gwin->objects[GID_URL],
							(ULONG *)&storage);
						utf8 = ami_to_utf8_easy((const char *)storage);

						ret = search_web_omni(utf8, SEARCH_WEB_OMNI_NONE, &url);
						ami_utf8_free(utf8);
						if (ret == NSERROR_OK) {
								browser_window_navigate(gwin->gw->bw,
										url,
										NULL,
										BW_NAVIGATE_HISTORY,
										NULL,
										NULL,
										NULL);
								nsurl_unref(url);
						}
						if (ret != NSERROR_OK) {
							amiga_warn_user(messages_get_errorcode(ret), 0);
						}
					}
					break;

					case GID_TOOLBARLAYOUT:
						/* Layout group does not send GA_RelVerify */
					break;

					case GID_SEARCH_ICON:
					{
						char *prov = NULL;
						struct Node *chooser_node = NULL;
						struct List *chooser_labels = NULL;
						ULONG selected = 0;
						ULONG i;

						if(gwin->objects[GID_SEARCH_ICON] == NULL)
							break;

#ifdef __amigaos4__
						GetAttr(CHOOSER_SelectedNode, gwin->objects[GID_SEARCH_ICON],
							(ULONG *)&chooser_node);
#else
						/* CHOOSER_SelectedNode is OS4-only; resolve via index */
						GetAttr(CHOOSER_Selected, gwin->objects[GID_SEARCH_ICON],
							&selected);
						GetAttr(CHOOSER_Labels, gwin->objects[GID_SEARCH_ICON],
							(ULONG *)&chooser_labels);
						if(chooser_labels != NULL) {
							chooser_node = GetHead(chooser_labels);
							for(i = 0; (chooser_node != NULL) && (i < selected); i++) {
								chooser_node = GetSucc(chooser_node);
							}
						}
#endif
						if(chooser_node != NULL) {
							GetChooserNodeAttrs(chooser_node, CNA_Text,
								(ULONG *)&prov, TAG_DONE);
							if(prov != NULL) {
								nsoption_set_charp(search_web_provider,
									(char *)strdup(prov));
							}
						}
						search_web_select_provider(nsoption_charp(search_web_provider));
					}
					break;

					case GID_SEARCHSTRING:
					{
						nserror ret;
						nsurl *url;

						if(gwin->objects[GID_SEARCHSTRING] == NULL)
							break;

						GetAttr(STRINGA_TextVal,
							(Object *)gwin->objects[GID_SEARCHSTRING],
							(ULONG *)&storage);

						utf8 = ami_to_utf8_easy((const char *)storage);

						ret = search_web_omni(utf8, SEARCH_WEB_OMNI_SEARCHONLY, &url);
						ami_utf8_free(utf8);
						if (ret == NSERROR_OK) {
								browser_window_navigate(gwin->gw->bw,
										url,
										NULL,
										BW_NAVIGATE_HISTORY,
										NULL,
										NULL,
										NULL);
							nsurl_unref(url);
						}
						if (ret != NSERROR_OK) {
							amiga_warn_user(messages_get_errorcode(ret), 0);
						}

					}
					break;

					case GID_HOME:
						{
							if (nsurl_create(nsoption_charp(homepage_url), &url) != NSERROR_OK) {
								amiga_warn_user("NoMemory", 0);
							} else {
								browser_window_navigate(gwin->gw->bw,
										url,
										NULL,
										BW_NAVIGATE_HISTORY,
										NULL,
										NULL,
										NULL);
								nsurl_unref(url);
							}
						}
					break;

					case GID_STOP:
						if(browser_window_stop_available(gwin->gw->bw))
							browser_window_stop(gwin->gw->bw);
					break;

					case GID_RELOAD:
						/* Nami combined Stop/Reload: stop while loading */
						if(gwin->ui_nami &&
						   browser_window_stop_available(gwin->gw->bw)) {
							browser_window_stop(gwin->gw->bw);
							break;
						}
						ami_update_quals(gwin);

						if(browser_window_reload_available(gwin->gw->bw))
						{
							if(gwin->key_state & BROWSER_MOUSE_MOD_1)
							{
								browser_window_reload(gwin->gw->bw, true);
							}
							else
							{
								browser_window_reload(gwin->gw->bw, false);
							}
						}
					break;

					case GID_BACK:
						ami_gui_history(gwin, true);
					break;

					case GID_FORWARD:
						ami_gui_history(gwin, false);
					break;

					case GID_PAGEINFO:
						{
							ULONG w_top, w_left;
							ULONG g_top, g_left, g_height;

							GetAttr(WA_Top, gwin->objects[OID_MAIN], &w_top);
							GetAttr(WA_Left, gwin->objects[OID_MAIN], &w_left);
							GetAttr(GA_Top, gwin->objects[GID_PAGEINFO], &g_top);
							GetAttr(GA_Left, gwin->objects[GID_PAGEINFO], &g_left);
							GetAttr(GA_Height, gwin->objects[GID_PAGEINFO], &g_height);
														
							if(ami_pageinfo_open(gwin->gw->bw,
											w_left + g_left,
											w_top + g_top + g_height) != NSERROR_OK) {
								NSLOG(netsurf, INFO, "Unable to open page info window");
							}
						}
					break;

					case GID_ICON:
						if(gwin->ui_nami)
							ami_gui_hotlist_toggle_from_url(gwin);
					break;

					case GID_FAVE:
						ami_gui_hotlist_toggle_from_url(gwin);
					break;

					case GID_HOTLIST:
					default:
					{
						ULONG gid_hit;

						gid_hit = result & WMHI_GADGETMASK;
						if(gwin->ui_nami &&
						   gid_hit >= GID_SIDE_HOT_BASE &&
						   gid_hit < (ULONG)(GID_SIDE_HOT_BASE + AMI_SIDE_HOTLIST_MAX)) {
							int hot_i;
							nsurl *hot_url;

							hot_i = (int)(gid_hit - GID_SIDE_HOT_BASE);
							hot_url = gwin->side_hot_url[hot_i];
							if(hot_url != NULL)
								ami_gui_nami_hotlist_open(gwin, hot_url);
						} else if(gwin->ui_nami &&
						   ((gid_hit >= GID_SIDE_TAB_BASE &&
						     gid_hit < (ULONG)(GID_SIDE_TAB_BASE +
								AMI_SIDE_TAB_MAX)) ||
						    (gid_hit >= GID_SIDE_TAB_ICON_BASE &&
						     gid_hit < (ULONG)(GID_SIDE_TAB_ICON_BASE +
								AMI_SIDE_TAB_MAX)))) {
							int tab_i;
							struct gui_window *tab_gw;
							bool was_drag;

							was_drag = false;
							if(ami_gtdrag_armed()) {
								ami_gui_nami_gtdrag_apply_abs(gwin);
								was_drag = ami_gtdrag_select_up(gwin->win);
								ami_gui_nami_gtdrag_restore_applied();
								/* Drop poll waits for real LMB-up in IDCMP hook */
							}
							/* Drag stole the click — do not also switch tabs */
							if(was_drag)
								break;

							if(gid_hit >= GID_SIDE_TAB_ICON_BASE)
								tab_i = (int)(gid_hit - GID_SIDE_TAB_ICON_BASE);
							else
								tab_i = (int)(gid_hit - GID_SIDE_TAB_BASE);
							tab_gw = gwin->side_tab_gw[tab_i];
							if(tab_gw != NULL)
								ami_switch_tab_to(gwin, tab_gw, true);
						}
					}
					break;
				}
			break;

			case WMHI_RAWKEY:
				ami_update_quals(gwin);
			
				storage = result & WMHI_GADGETMASK;
				if(storage >= IECODE_UP_PREFIX) break;

				GetAttr(WINDOW_InputEvent,gwin->objects[OID_MAIN],(ULONG *)&ie);

				nskey = ami_key_to_nskey(storage, ie);

				if((ie->ie_Qualifier & IEQUALIFIER_RCOMMAND) &&
					((31 < nskey) && (nskey < 127))) {
				/* NB: Some keypresses are converted to generic keypresses above
				 * rather than being "menu-emulated" here. */
					switch(nskey)
					{
						/* The following aren't available from the menu at the moment */

						case 'r': // reload
							if(browser_window_reload_available(gwin->gw->bw))
								browser_window_reload(gwin->gw->bw, false);
						break;

						case 'u': // open url
							if((nsoption_bool(kiosk_mode) == false))
								ActivateLayoutGadget((struct Gadget *)gwin->objects[GID_MAIN],
									gwin->win, NULL, (uint32)gwin->objects[GID_URL]);
						break;
					}
				}
				else
				{
					if(!browser_window_key_press(gwin->gw->bw, nskey))
					{
						switch(nskey)
						{
							case NS_KEY_UP:
								ami_gui_scroll_internal(gwin, 0, -NSA_KBD_SCROLL_PX);
							break;

							case NS_KEY_DOWN:
								ami_gui_scroll_internal(gwin, 0, +NSA_KBD_SCROLL_PX);
							break;

							case NS_KEY_LEFT:
								ami_gui_scroll_internal(gwin, -NSA_KBD_SCROLL_PX, 0);
							break;

							case NS_KEY_RIGHT:
								ami_gui_scroll_internal(gwin, +NSA_KBD_SCROLL_PX, 0);
							break;

							case NS_KEY_PAGE_UP:
								ami_gui_scroll_internal(gwin, 0, SCROLL_PAGE_UP);
							break;

							case NS_KEY_PAGE_DOWN:
							case ' ':
								ami_gui_scroll_internal(gwin, 0, SCROLL_PAGE_DOWN);
							break;

							case NS_KEY_LINE_START: // page left
								ami_gui_scroll_internal(gwin, SCROLL_PAGE_UP, 0);
							break;

							case NS_KEY_LINE_END: // page right
								ami_gui_scroll_internal(gwin, SCROLL_PAGE_DOWN, 0);
							break;

							case NS_KEY_TEXT_START: // home
								ami_gui_scroll_internal(gwin, SCROLL_TOP, SCROLL_TOP);
							break;

							case NS_KEY_TEXT_END: // end
								ami_gui_scroll_internal(gwin, SCROLL_BOTTOM, SCROLL_BOTTOM);
							break;

							case NS_KEY_WORD_RIGHT: // alt+right
								ami_change_tab(gwin, 1);
							break;

							case NS_KEY_WORD_LEFT: // alt+left
								ami_change_tab(gwin, -1);
							break;

							case NS_KEY_DELETE_LEFT: // backspace
								ami_gui_history(gwin, true);
							break;

							/* RawKeys. NB: These are passthrus in ami_key_to_nskey() */
							case RAWKEY_F5: // reload
								if(browser_window_reload_available(gwin->gw->bw))
									browser_window_reload(gwin->gw->bw,false);
							break;

							case RAWKEY_F8: // scale 100%
								ami_gui_set_scale(gwin->gw, 1.0);
							break;

							case RAWKEY_F9: // decrease scale
								ami_gui_adjust_scale(gwin->gw, -0.1);
							break;

							case RAWKEY_F10: // increase scale
								ami_gui_adjust_scale(gwin->gw, +0.1);
							break;
							
							case RAWKEY_F12: // console log
								ami_gui_console_log_toggle(gwin->gw);
							break;

							case RAWKEY_HELP: // help
								ami_help_open(AMI_HELP_GUI, scrn);
							break;
						}
					} else if(nskey == NS_KEY_COPY_SELECTION) {
						/* if we've copied a selection we need to clear it - style guide rules */
						browser_window_key_press(gwin->gw->bw, NS_KEY_CLEAR_SELECTION);
					}
				}
			break;

			case WMHI_NEWSIZE:
				ami_set_border_gadget_size(gwin);
				ami_gui_relabel_all_tabs(gwin);
				ami_throbber_redraw_schedule(0, gwin->gw);
				ami_schedule(0, ami_gui_refresh_favicon, gwin);
				browser_window_schedule_reformat(gwin->gw->bw);
				/* Do not wait for content settle — size chrome now */
				ami_schedule_redraw_interactive(gwin, true);
				if(gwin->ui_nami) {
					if(gwin->objects[GID_CHROMELAYOUT] != NULL) {
						FlushLayoutDomainCache(
							(struct Gadget *)gwin->objects[GID_MAIN]);
						RethinkLayout(
							(struct Gadget *)gwin->objects[GID_MAIN],
							gwin->win, NULL, TRUE);
						RefreshGList((struct Gadget *)gwin->objects[GID_CHROMELAYOUT],
								gwin->win, NULL, 1);
					}
					/* Only rethink when the panel is actually in the tree */
					if(gwin->sidebar_expanded)
						ami_gui_nami_sidebar_tab_rethink(gwin);
					ami_gui_nami_paint_chrome_rborder(gwin);
					ami_throbber_redraw_schedule(0, gwin->gw);
				}
			break;

			case WMHI_CLOSEWINDOW:
				/* Destroy after paint strips finish — gwin is mid-use */
				if(ami_gui_paint_depth > 0) {
					gwin->closed = true;
					win_closed = TRUE;
					break;
				}
				ami_gui_close_window(gwin);
				win_closed = TRUE;
	        break;
#ifdef __amigaos4__
			case WMHI_ICONIFY:
			{
				struct bitmap *bm = NULL;
				browser_window_history_get_thumbnail(gwin->gw->bw,
								     &bm);
				gwin->dobj = amiga_icon_from_bitmap(bm);
				amiga_icon_superimpose_favicon_internal(gwin->gw->favicon,
					gwin->dobj);
				HideWindow(gwin->win);
				if(strlen(gwin->wintitle) > 23) {
					strncpy(gwin->icontitle, gwin->wintitle, 20);
					gwin->icontitle[20] = '.';
					gwin->icontitle[21] = '.';
					gwin->icontitle[22] = '.';
					gwin->icontitle[23] = '\0';
				} else {
					strlcpy(gwin->icontitle, gwin->wintitle, 23);
				}
				gwin->appicon = AddAppIcon((ULONG)gwin->objects[OID_MAIN],
									(ULONG)gwin, gwin->icontitle, appport,
									0, gwin->dobj, NULL);

				cur_gw = NULL;
			}
			break;
#endif
			case WMHI_INACTIVE:
				gwin->gw->c_h_temp = gwin->gw->c_h;
				gui_window_remove_caret(gwin->gw);
				if(gwin->ui_nami) {
					ami_gui_tb_set_hover(gwin, 0);
					ami_gui_tb_set_armed(gwin, 0);
				}
				if(gwin->ui_nami && gwin->objects[GID_CHROMELAYOUT] != NULL)
					RefreshGList((struct Gadget *)gwin->objects[GID_CHROMELAYOUT],
						gwin->win, NULL, 1);
				if(gwin->ui_nami)
					ami_gui_nami_paint_chrome_rborder(gwin);
			break;

			case WMHI_ACTIVE:
				if(gwin->gw->bw) cur_gw = gwin->gw;
				if(gwin->gw->c_h_temp)
					gwin->gw->c_h = gwin->gw->c_h_temp;
				/* Front again — next depth click should send to back */
				gwin->chrome_depth_back = false;
				if(gwin->ui_nami && gwin->objects[GID_CHROMELAYOUT] != NULL)
					RefreshGList((struct Gadget *)gwin->objects[GID_CHROMELAYOUT],
						gwin->win, NULL, 1);
				if(gwin->ui_nami)
					ami_gui_nami_paint_chrome_rborder(gwin);
				if(gwin->ui_nami)
					ami_gui_update_screentitle(gwin);
			break;

			case WMHI_INTUITICK:
				/* Nami: refresh screen-title telemetry ~1 Hz while active */
				if(gwin->ui_nami && gwin->win != NULL &&
				   (gwin->win->Flags & WFLG_WINDOWACTIVE))
					ami_gui_update_screentitle(gwin);
			break;

   	     	default:
				//printf("class: %ld\n",(result & WMHI_CLASSMASK));
       		break;
		}

		if(win_destroyed)
		{
				/* we can't be sure what state our window_list is in, so let's
				jump out of the function and start again */

			win_destroyed = false;
			return TRUE;
		}

		if(drag_x_move || drag_y_move)
		{
			struct rect rect;

			gui_window_get_scroll(gwin->gw,
				&gwin->gw->scrollx, &gwin->gw->scrolly);

			rect.x0 = rect.x1 = gwin->gw->scrollx + drag_x_move;
			rect.y0 = rect.y1 = gwin->gw->scrolly + drag_y_move;

			gui_window_set_scroll(gwin->gw, &rect);
		}

//	ReplyMsg((struct Message *)message);
	}

	if(gwin->closed == true) {
		win_closed = TRUE;
		/* Mid-paint close: destroy when strips finish (caller checks closed) */
		if(ami_gui_paint_depth == 0)
			ami_gui_close_window(gwin);
	}

	return win_closed;
}

static void ami_gui_appicon_remove(struct gui_window_2 *gwin)
{
	if(gwin->appicon)
	{
		RemoveAppIcon(gwin->appicon);
		amiga_icon_free(gwin->dobj);
		gwin->appicon = NULL;
	}
}

static nserror gui_page_info_change(struct gui_window *gw)
{
	int bm_idx;
	browser_window_page_info_state pistate;
	struct gui_window_2 *gwin = ami_gui_get_gui_window_2(gw);
	struct browser_window *bw = ami_gui_get_browser_window(gw);

	/* if this isn't the visible tab, don't do anything */
	if((gwin == NULL) || (gwin->gw != gw)) return NSERROR_OK;

	pistate = browser_window_get_page_info_state(bw);

	switch(pistate) {
		case PAGE_STATE_INTERNAL:
			bm_idx = GID_PAGEINFO_INTERNAL_BM;
		break;

		case PAGE_STATE_LOCAL:
			bm_idx = GID_PAGEINFO_LOCAL_BM;
		break;

		case PAGE_STATE_INSECURE:
			bm_idx = GID_PAGEINFO_INSECURE_BM;
		break;

		case PAGE_STATE_SECURE_OVERRIDE:
			bm_idx = GID_PAGEINFO_WARNING_BM;
		break;

		case PAGE_STATE_SECURE_ISSUES:
			bm_idx = GID_PAGEINFO_WARNING_BM;
		break;

		case PAGE_STATE_SECURE:
			bm_idx = GID_PAGEINFO_SECURE_BM;
		break;

		default:
			bm_idx = GID_PAGEINFO_INTERNAL_BM;
		break;
	}

	RefreshSetGadgetAttrs((struct Gadget *)gwin->objects[GID_PAGEINFO], gwin->win, NULL,
				BUTTON_RenderImage, gwin->objects[bm_idx],
				AMI_GA_HELP, gwin->helphints[bm_idx],
				TAG_DONE);

	return NSERROR_OK;
}

static void ami_handle_appmsg(void)
{
	struct AppMessage *appmsg;
	struct gui_window_2 *gwin;
	int x, y;
	struct WBArg *appwinargs;
	STRPTR filename;
	int i = 0;

	while((appmsg = (struct AppMessage *)GetMsg(appport)))
	{
		gwin = (struct gui_window_2 *)appmsg->am_UserData;

		if(appmsg->am_Type == AMTYPE_APPICON)
		{
			ami_gui_appicon_remove(gwin);
			ShowWindow(gwin->win, WINDOW_FRONTMOST);
			ActivateWindow(gwin->win);
		}
		else if(appmsg->am_Type == AMTYPE_APPWINDOW)
		{
			for(i = 0; i < appmsg->am_NumArgs; ++i)
			{
				if((appwinargs = &appmsg->am_ArgList[i]))
				{
					if((filename = malloc(1024)))
					{
						if(appwinargs->wa_Lock)
						{
							NameFromLock(appwinargs->wa_Lock, filename, 1024);
						}

						AddPart(filename, appwinargs->wa_Name, 1024);

						if(ami_mouse_to_ns_coords(gwin, &x, &y,
							appmsg->am_MouseX, appmsg->am_MouseY) == false)
						{
							nsurl *url;

							if (netsurf_path_to_nsurl(filename, &url) != NSERROR_OK) {
								amiga_warn_user("NoMemory", 0);
							}
							else
							{
								if(i == 0)
								{
									browser_window_navigate(gwin->gw->bw,
										url,
										NULL,
										BW_NAVIGATE_HISTORY,
										NULL,
										NULL,
										NULL);

									ActivateWindow(gwin->win);
								}
								else
								{
									browser_window_create(BW_CREATE_CLONE | BW_CREATE_HISTORY |
											      BW_CREATE_TAB,
											      url,
											      NULL,
											      gwin->gw->bw,
											      NULL);
								}
								nsurl_unref(url);
							}
						}
						else
						{
							if(browser_window_drop_file_at_point(gwin->gw->bw, x, y, filename) == false)
							{
								nsurl *url;

								if (netsurf_path_to_nsurl(filename, &url) != NSERROR_OK) {
									amiga_warn_user("NoMemory", 0);
								}
								else
								{

									if(i == 0)
									{
										browser_window_navigate(gwin->gw->bw,
											url,
											NULL,
											BW_NAVIGATE_HISTORY,
											NULL,
											NULL,
											NULL);

										ActivateWindow(gwin->win);
									}
									else
									{
										browser_window_create(BW_CREATE_CLONE | BW_CREATE_HISTORY |
												      BW_CREATE_TAB,
												      url,
												      NULL,
												      gwin->gw->bw,
												      NULL);
										
									}
									nsurl_unref(url);
								}
							}
						}
						free(filename);
					}
				}
			}
		}
		ReplyMsg((struct Message *)appmsg);
	}
}

static void ami_handle_applib(void)
{
#ifdef __amigaos4__
	struct ApplicationMsg *applibmsg;
	struct browser_window *bw;
	nsurl *url;
	nserror error;

	if(!applibport) return;

	while((applibmsg=(struct ApplicationMsg *)GetMsg(applibport)))
	{
		switch (applibmsg->type)
		{
			case APPLIBMT_NewBlankDoc:
			{

				error = nsurl_create(nsoption_charp(homepage_url), &url);
				if (error == NSERROR_OK) {
					error = browser_window_create(BW_CREATE_HISTORY,
								      url,
								      NULL,
								      NULL,
								      &bw);
					nsurl_unref(url);
				}
				if (error != NSERROR_OK) {
					amiga_warn_user(messages_get_errorcode(error), 0);
				}
			}
			break;

			case APPLIBMT_OpenDoc:
			{
				struct ApplicationOpenPrintDocMsg *applibopdmsg =
					(struct ApplicationOpenPrintDocMsg *)applibmsg;

				error = netsurf_path_to_nsurl(applibopdmsg->fileName, &url);
				if (error == NSERROR_OK) {
					error = browser_window_create(BW_CREATE_HISTORY,
								      url,
								      NULL,
								      NULL,
								      &bw);
					nsurl_unref(url);
				}
				if (error != NSERROR_OK) {
					amiga_warn_user(messages_get_errorcode(error), 0);
				}
			}
			break;

			case APPLIBMT_ToFront:
				if(cur_gw)
				{
					ScreenToFront(scrn);
					WindowToFront(cur_gw->shared->win);
					ActivateWindow(cur_gw->shared->win);
				}
			break;

			case APPLIBMT_OpenPrefs:
				ScreenToFront(scrn);
				ami_gui_opts_open();
			break;

			case APPLIBMT_Quit:
			case APPLIBMT_ForceQuit:
				ami_quit_netsurf();
			break;

			case APPLIBMT_CustomMsg:
			{
				struct ApplicationCustomMsg *applibcustmsg =
					(struct ApplicationCustomMsg *)applibmsg;
				NSLOG(netsurf, INFO,
				      "Ringhio BackMsg received: %s",
				      applibcustmsg->customMsg);

				ami_download_parse_backmsg(applibcustmsg->customMsg);
			}
			break;
		}
		ReplyMsg((struct Message *)applibmsg);
	}
#endif
}

void ami_get_msg(void)
{
	ULONG winsignal = 1L << sport->mp_SigBit;
	ULONG appsig = 1L << appport->mp_SigBit;
	ULONG schedulesig = 1L << schedulermsgport->mp_SigBit;
	ULONG ctrlcsig = SIGBREAKF_CTRL_C;
	uint32 signal = 0;
	fd_set read_fd_set, write_fd_set, except_fd_set;
	int max_fd = -1;
	struct MsgPort *printmsgport = ami_print_get_msgport();
	ULONG printsig = 0;
	ULONG helpsignal = ami_help_signal();
	if(printmsgport) printsig = 1L << printmsgport->mp_SigBit;
	uint32 signalmask = winsignal | appsig | schedulesig | rxsig |
				printsig | applibsig | helpsignal;
#ifdef WITH_AMIHTTP
	{
		BYTE amihttp_sig;

		amihttp_sig = fetch_amihttp_signal();
		if (amihttp_sig != -1) {
			signalmask |= (1UL << amihttp_sig);
		}
	}
#endif

#ifndef __amigaos4__
	ami_memory_poll();
#endif

	if ((fetch_fdset(&read_fd_set, &write_fd_set, &except_fd_set, &max_fd) == NSERROR_OK) &&
			(max_fd != -1)) {
		/* max_fd is the highest fd in use, but waitselect() needs to know how many
		 * are in use, so we add 1. */

		if (waitselect(max_fd + 1, &read_fd_set, &write_fd_set, &except_fd_set,
				NULL, (ULONG *)&signalmask) != -1) {
			signal = signalmask;
		} else {
			NSLOG(netsurf, INFO, "waitselect() returned error");
			/* \todo Fix Ctrl-C handling.
			 * WaitSelect() from bsdsocket.library returns -1 if the task was
			 * signalled with a Ctrl-C.  waitselect() from newlib.library does not.
			 * Adding the Ctrl-C signal to our user signal mask causes a Ctrl-C to
			 * occur sporadically.  Otherwise we never get a -1 except on error.
			 * NetSurf still terminates at the Wait() when network activity is over.
			 */
		}
	} else {
		/* If fetcher_fdset fails or no network activity, do it the old fashioned way. */
		signalmask |= ctrlcsig;
		signal = Wait(signalmask);
	}

	if(signal & winsignal)
		while(ami_handle_msg());

	if(signal & appsig)
		ami_handle_appmsg();

	if(signal & rxsig)
		ami_arexx_handle();

	if(signal & applibsig)
		ami_handle_applib();

	if(signal & printsig) {
		while(GetMsg(printmsgport));  //ReplyMsg
		ami_print_cont();
	}

	if(signal & schedulesig) {
		ami_schedule_handle(schedulermsgport);
	}

	if(signal & helpsignal)
		ami_help_process();

	if(signal & ctrlcsig)
		ami_quit_netsurf_delayed();
}

static void ami_change_tab(struct gui_window_2 *gwin, int direction)
{
	struct Node *tab_node = gwin->gw->tab_node;
	struct Node *ptab = NULL;
	struct gui_window *gw;

	if(gwin->tabs <= 1) return;

	if(direction > 0) {
		ptab = GetSucc(tab_node);
	} else {
		ptab = GetPred(tab_node);
	}

	if(!ptab) return;
	if(ptab == gwin->new_tab_tab) return;

	if(gwin->ui_nami) {
		gw = NULL;
		GetClickTabNodeAttrs(ptab, TNA_UserData, &gw, TAG_DONE);
		if(gw != NULL)
			ami_switch_tab_to(gwin, gw, true);
		return;
	}

	RefreshSetGadgetAttrs((struct Gadget *)gwin->objects[GID_TABS], gwin->win, NULL,
						CLICKTAB_CurrentNode, ptab,
						TAG_DONE);

	ami_switch_tab(gwin, true);
}


static void gui_window_set_title(struct gui_window *g, const char *restrict title)
{
	char *restrict utf8title;
	size_t max_chars;

	if(!g) return;
	if(!title) return;

	utf8title = ami_utf8_easy((char *)title);

	if(g->tab_node) {
		if((g->tabtitle == NULL) || (strcmp(utf8title, g->tabtitle)))
		{
			if(g->tabtitle) free(g->tabtitle);
			g->tabtitle = strdup(utf8title);

			max_chars = ami_gui_tab_label_max_chars(g->shared);
			if(g->shared->ui_nami)
				max_chars = ami_gui_nami_tab_label_max_chars(g->shared);

			if(g->shared->ui_nami) {
				ami_gui_apply_tab_label(g, max_chars);
			} else {
				SetGadgetAttrs((struct Gadget *)g->shared->objects[GID_TABS],
								g->shared->win, NULL,
								CLICKTAB_Labels, ~0,
								TAG_DONE);

				ami_gui_apply_tab_label(g, max_chars);

				/* Title text only — MinorLabelChange avoids page/layout flicker. */
				RefreshSetGadgetAttrs((struct Gadget *)g->shared->objects[GID_TABS],
									g->shared->win, NULL,
									CLICKTAB_Labels, &g->shared->tab_list,
									CLICKTAB_MinorLabelChange, TRUE,
									TAG_DONE);
			}
		}
	}

	if(g == g->shared->gw) {
		if((g->shared->wintitle == NULL) || (strcmp(utf8title, g->shared->wintitle)))
		{
			if(g->shared->wintitle) free(g->shared->wintitle);
			g->shared->wintitle = strdup(utf8title);
			/* Nami has no title bar; session info lives in the screen title */
			if(g->shared->ui_nami)
				ami_gui_update_screentitle(g->shared);
			else
				SetWindowTitles(g->shared->win, g->shared->wintitle,
						ami_gui_get_screen_title());
		}
	}

	ami_utf8_free(utf8title);
}

static void gui_window_update_extent(struct gui_window *g)
{
	struct IBox *bbox;

	if(!g || !g->bw) return;
	if(browser_window_has_content(g->bw) == false) return;

	if(g == g->shared->gw) {
		int width, height;
		if(ami_gui_get_space_box((Object *)g->shared->objects[GID_BROWSER], &bbox) != NSERROR_OK) {
			amiga_warn_user("NoMemory", "");
			return;
		}

		if(g->shared->objects[GID_VSCROLL]) {
			browser_window_get_extents(g->bw, true, &width, &height);
			if(g->shared->ui_nami) {
				ami_gui_nami_set_prop(g->shared, g->shared->objects[GID_VSCROLL],
						&g->shared->nami_vprop_shift,
						(ULONG)height, (ULONG)bbox->Height, 0, FALSE);
			} else {
				RefreshSetGadgetAttrs((struct Gadget *)(APTR)g->shared->objects[GID_VSCROLL],g->shared->win,NULL,
					SCROLLER_Total, (ULONG)(height),
					SCROLLER_Visible, bbox->Height,
				TAG_DONE);
			}
		}

		if(g->shared->objects[GID_HSCROLL])
		{
			browser_window_get_extents(g->bw, true, &width, &height);
			if(g->shared->ui_nami) {
				ami_gui_nami_set_prop(g->shared, g->shared->objects[GID_HSCROLL],
						&g->shared->nami_hprop_shift,
						(ULONG)width, (ULONG)bbox->Width, 0, FALSE);
			} else {
				RefreshSetGadgetAttrs((struct Gadget *)(APTR)g->shared->objects[GID_HSCROLL],
					g->shared->win, NULL,
					SCROLLER_Total, (ULONG)(width),
					SCROLLER_Visible, bbox->Width,
					TAG_DONE);
			}
		}

		ami_gui_free_space_box(bbox);
	}

	ami_gui_scroller_update(g->shared);
	g->shared->new_content = true;
}


/**
 * Invalidates an area of an amiga browser window
 *
 * \param g gui_window
 * \param rect area to redraw or NULL for the entire window area
 * \return NSERROR_OK on success or appropriate error code
 */
static nserror amiga_window_invalidate_area(struct gui_window *g,
					    const struct rect *restrict rect)
{
	struct nsObject *nsobj;
	struct rect *restrict deferred_rect;

	if(!g) return NSERROR_BAD_PARAMETER;

	if (rect == NULL) {
		if (g != g->shared->gw) {
			return NSERROR_OK;
		}
		/* Full-window invalidate from core — paint as one redraw */
		ami_schedule_redraw(g->shared, true);
		return NSERROR_OK;
	}

	if (ami_gui_window_update_box_deferred_check(g->deferred_rects, rect,
						    g->deferred_rects_pool)) {
		deferred_rect = ami_memory_itempool_alloc(g->deferred_rects_pool,
							  sizeof(struct rect));
		CopyMem(rect, deferred_rect, sizeof(struct rect));
		nsobj = AddObject(g->deferred_rects, AMINS_RECT);
		nsobj->objstruct = deferred_rect;
	} else {
		NSLOG(netsurf, INFO,
		      "Ignoring duplicate or subset of queued box redraw");
	}
	/* Scroll strip invalidates must stay on the interactive budget */
	if(g->shared->redraw_scroll || g->shared->redraw_interactive)
		ami_schedule_redraw_interactive(g->shared, false);
	else
		ami_schedule_redraw(g->shared, false);

	return NSERROR_OK;
}


static void ami_switch_tab_to(struct gui_window_2 *gwin, struct gui_window *new_gw,
		bool redraw)
{
	struct IBox *bbox;

	/* Clear the last new tab list */
	gwin->last_new_tab = NULL;

	if(gwin->tabs == 0) return;
	if(new_gw == NULL)
		return;

	/* Already on this tab — do not clear/redraw the browser view */
	if(new_gw == gwin->gw) {
		if(gwin->ui_nami)
			ami_gui_nami_sidebar_sync_selection(gwin);
		return;
	}

	gui_window_get_scroll(gwin->gw,
		&gwin->gw->scrollx, &gwin->gw->scrolly);

	gwin->gw = new_gw;
	cur_gw = gwin->gw;

	ami_gui_console_log_switch(gwin->gw);

	if(gwin->ui_nami)
		ami_gui_nami_sidebar_sync_selection(gwin);

	if(ami_gui_get_space_box((Object *)gwin->objects[GID_BROWSER], &bbox) != NSERROR_OK) {
		amiga_warn_user("NoMemory", "");
		return;
	}

	if((gwin->gw->bw == NULL) || (browser_window_has_content(gwin->gw->bw)) == false) {
		RefreshSetGadgetAttrs((struct Gadget *)gwin->objects[GID_URL],
			gwin->win, NULL, STRINGA_TextVal, "", TAG_DONE);

		ami_plot_clear_bbox(gwin->win->RPort, bbox);
		ami_gui_free_space_box(bbox);
		if(gwin->ui_nami)
			ami_gui_update_screentitle(gwin);
		return;
	}

	ami_plot_release_pens(gwin->shared_pens);
	ami_update_buttons(gwin);
	ami_gui_menu_update_disabled(gwin->gw, browser_window_get_content(gwin->gw->bw));

	if(redraw)
	{
		struct rect rect;

		ami_plot_clear_bbox(gwin->win->RPort, bbox);
		gui_window_set_title(gwin->gw,
				     browser_window_get_title(gwin->gw->bw));
		gui_window_update_extent(gwin->gw);
		amiga_window_invalidate_area(gwin->gw, NULL);

		rect.x0 = rect.x1 = gwin->gw->scrollx;
		rect.y0 = rect.y1 = gwin->gw->scrolly;

		gui_window_set_scroll(gwin->gw, &rect);
		gwin->redraw_scroll = false;

		browser_window_refresh_url_bar(gwin->gw->bw);
		ami_gui_update_hotlist_button(gwin);
		ami_gui_scroller_update(gwin);
		ami_throbber_redraw_schedule(0, gwin->gw);

		gui_window_set_icon(gwin->gw, gwin->gw->favicon);
		gui_page_info_change(gwin->gw);
	}

	ami_gui_free_space_box(bbox);
	if(gwin->ui_nami)
		ami_gui_update_screentitle(gwin);
}

static void ami_switch_tab(struct gui_window_2 *gwin, bool redraw)
{
	struct Node *tabnode;
	struct gui_window *new_gw;

	if(gwin->tabs == 0) return;

	/* Nami uses tab ButtonObj clicks → ami_switch_tab_to directly */
	if(gwin->ui_nami)
		return;

	if(gwin->objects[GID_TABS] == NULL) return;
	GetAttr(CLICKTAB_CurrentNode, (Object *)gwin->objects[GID_TABS],
				(ULONG *)&tabnode);
	if((tabnode == NULL) || (tabnode == gwin->new_tab_tab))
		return;
	GetClickTabNodeAttrs(tabnode,
				TNA_UserData, &new_gw,
				TAG_DONE);
	ami_switch_tab_to(gwin, new_gw, redraw);
}

void ami_quit_netsurf(void)
{
	struct nsObject *node;
	struct nsObject *nnode;
	struct ami_generic_window *w;

	NSLOG(netsurf, INFO, "ami_quit_netsurf");

	/* Disable the multiple tabs open warning */
	nsoption_set_bool(tab_close_warn, false);

	if(!IsMinListEmpty(window_list)) {
		node = (struct nsObject *)GetHead((struct List *)window_list);

		do {
			nnode=(struct nsObject *)GetSucc((struct Node *)node);
			w = node->objstruct;

			if(w->tbl->close != NULL) {
				if(node->Type == AMINS_WINDOW) {
					struct gui_window_2 *gwin = (struct gui_window_2 *)w;
					ShowWindow(gwin->win, WINDOW_BACKMOST); // do we need this??
				}
				w->tbl->close(w);
			}
		} while((node = nnode));

		win_destroyed = true;
	}

	if(IsMinListEmpty(window_list)) {
		/* last window closed, so exit */
		ami_quit = true;
	}
}

static void ami_quit_netsurf_delayed(void)
{
	int res = -1;
#ifdef __amigaos4__
	char *utf8text;
	char *utf8gadgets;
	char *title;

	utf8text = ami_utf8_easy(messages_get("TCPIPShutdown"));
	utf8gadgets = ami_utf8_easy(messages_get("AbortShutdown"));
	title = ami_utf8_easy(messages_get("NetSurf"));

	DisplayBeep(NULL);

	res = ami_misc_requester_ex(NULL,
			title != NULL ? title : messages_get("NetSurf"),
			utf8text != NULL ? utf8text : messages_get("TCPIPShutdown"),
			utf8gadgets != NULL ? utf8gadgets : messages_get("AbortShutdown"),
			AMI_REQ_IMAGE_INFO, 5, TRUE);

	if(utf8text != NULL)
		free(utf8text);
	if(utf8gadgets != NULL)
		free(utf8gadgets);
	if(title != NULL)
		free(title);
#endif
	if(res == -1) { /* Requester timed out / unavailable on OS3 */
		ami_quit_netsurf();
	}
}

static void ami_gui_close_screen(struct Screen *scrn, BOOL locked_screen, BOOL donotwait)
{
	if(scrn == NULL) return;

	if(locked_screen) {
		UnlockPubScreen(NULL,scrn);
		locked_screen = FALSE;
	}

	/* If this is our own screen, wait for visitor windows to close */
	if(screen_signal == -1) return;

	if(CloseScreen(scrn) == TRUE) {
		if(screen_signal != -1) {
			FreeSignal(screen_signal);
			screen_signal = -1;
			scrn = NULL;
		}
		return;
	}
	if(donotwait == TRUE) return;

	ULONG scrnsig = 1 << screen_signal;
	NSLOG(netsurf, INFO,
	      "Waiting for visitor windows to close... (signal)");
	Wait(scrnsig);

	while (CloseScreen(scrn) == FALSE) {
		NSLOG(netsurf, INFO,
		      "Waiting for visitor windows to close... (polling)");
		Delay(50);
	}

	FreeSignal(screen_signal);
	screen_signal = -1;
	scrn = NULL;
}

void ami_try_quit(void)
{
	if(!IsMinListEmpty(window_list)) return;

	if(nsoption_bool(close_no_quit) == false)
	{
		ami_quit = true;
		return;
	}
	else
	{
		ami_gui_close_screen(scrn, locked_screen, TRUE);
	}
}

static void gui_quit(void)
{
	ami_theme_throbber_free();

	urldb_save(nsoption_charp(url_file));
	urldb_save_cookies(nsoption_charp(cookie_file));
	hotlist_fini();
#ifdef __amigaos4__
	if(IApplication && ami_appid)
		UnregisterApplication(ami_appid, NULL);
#endif
	ami_arexx_cleanup();

	ami_plot_ra_free(browserglob);

	ami_font_fini();
	ami_help_free();
	
	NSLOG(netsurf, INFO, "Freeing menu items");
	ami_ctxmenu_free();
	ami_menu_free_glyphs();

	NSLOG(netsurf, INFO, "Freeing mouse pointers");
	ami_mouse_pointers_free();

	ami_file_req_free();
	ami_openurl_close();
#ifdef __amigaos4__
	FreeStringClass(urlStringClass);
#endif

	FreeObjList(window_list);

	ami_clipboard_free();
	ami_gui_resources_free();

	NSLOG(netsurf, INFO, "Closing screen");
	ami_gui_close_screen(scrn, locked_screen, FALSE);
	if(nsscreentitle) FreeVec(nsscreentitle);
}

char *ami_gui_get_cache_favicon_name(nsurl *url, bool only_if_avail)
{
	STRPTR filename = NULL;

	if ((filename = ASPrintf("%s/%x", current_user_faviconcache, nsurl_hash(url)))) {
		NSLOG(netsurf, INFO, "favicon cache location: %s", filename);

		if (only_if_avail == true) {
			BPTR lock = 0;
			if((lock = Lock(filename, ACCESS_READ))) {
				UnLock(lock);
				return filename;
			}
		} else {
			return filename;
		}
	}
	return NULL;
}

static void ami_gui_cache_favicon(nsurl *url, struct bitmap *favicon)
{
	STRPTR filename = NULL;

	if ((filename = ami_gui_get_cache_favicon_name(url, false))) {
		if(favicon) amiga_bitmap_save(favicon, filename, AMI_BITMAP_SCALE_ICON);
		FreeVec(filename);
	}
}

static void ami_gui_hotlist_toggle_from_url(struct gui_window_2 *gwin)
{
	char *storage;
	nsurl *url;

	if(gwin == NULL || gwin->objects[GID_URL] == NULL)
		return;

	GetAttr(STRINGA_TextVal,
		(Object *)gwin->objects[GID_URL],
		(ULONG *)&storage);
	if(nsurl_create((const char *)storage, &url) == NSERROR_OK) {
		if(hotlist_has_url(url)) {
			hotlist_remove_url(url);
		} else {
			hotlist_add_url(url);
		}
		nsurl_unref(url);
	}
	ami_gui_update_hotlist_button(gwin);
}

void ami_gui_update_hotlist_button(struct gui_window_2 *gwin)
{
	char *url;
	nsurl *nsurl;
	BOOL in_hotlist;
	const char *hint;

	if(gwin == NULL)
		return;

	GetAttr(STRINGA_TextVal,
		(Object *)gwin->objects[GID_URL],
		(ULONG *)&url);

	if(nsurl_create(url, &nsurl) != NSERROR_OK)
		return;

	in_hotlist = hotlist_has_url(nsurl) ? TRUE : FALSE;

	if(gwin->objects[GID_FAVE] != NULL) {
		/* NetSurf: dedicated star button swaps add/remove glyphs */
		if(in_hotlist) {
			RefreshSetGadgetAttrs((struct Gadget *)gwin->objects[GID_FAVE], gwin->win, NULL,
				BUTTON_RenderImage, gwin->objects[GID_FAVE_RMV],
				AMI_GA_HELP, gwin->helphints[GID_FAVE_RMV],
				TAG_DONE);

			if (gwin->gw->favicon)
				ami_gui_cache_favicon(nsurl, content_get_bitmap(gwin->gw->favicon));
		} else {
			RefreshSetGadgetAttrs((struct Gadget *)gwin->objects[GID_FAVE], gwin->win, NULL,
				BUTTON_RenderImage, gwin->objects[GID_FAVE_ADD],
				AMI_GA_HELP, gwin->helphints[GID_FAVE_ADD],
				TAG_DONE);
		}
	} else if(gwin->ui_nami && gwin->objects[GID_ICON] != NULL) {
		/* Nami: favicon slot is the hotlist control */
		hint = in_hotlist
			? gwin->helphints[GID_FAVE_RMV]
			: gwin->helphints[GID_FAVE_ADD];
		SetGadgetAttrs((struct Gadget *)gwin->objects[GID_ICON],
				gwin->win, NULL,
				AMI_GA_HELP, hint,
				TAG_DONE);
		if(in_hotlist && gwin->gw->favicon)
			ami_gui_cache_favicon(nsurl, content_get_bitmap(gwin->gw->favicon));
		gui_window_set_icon(gwin->gw, gwin->gw->favicon);
	}

	nsurl_unref(nsurl);
}

static bool ami_gui_hotlist_add(void *userdata, int level, int item,
		const char *title, nsurl *url, bool is_folder)
{
	struct ami_gui_tb_userdata *tb_userdata = (struct ami_gui_tb_userdata *)userdata;
	struct Node *speed_button_node;
	char menu_icon[1024];
	char *utf8title = NULL;

	if(level != 1) return false;
	if(item > AMI_GUI_TOOLBAR_MAX) return false;
	if(is_folder == true) return false;

	if(utf8_to_local_encoding(title,
		(strlen(title) < NSA_MAX_HOTLIST_BUTTON_LEN) ? strlen(title) : NSA_MAX_HOTLIST_BUTTON_LEN,
		&utf8title) != NSERROR_OK)
		return false;

	char *iconname = ami_gui_get_cache_favicon_name(url, true);
	if (iconname == NULL) iconname = ASPrintf("icons/content.png");
	ami_locate_resource(menu_icon, iconname);

	tb_userdata->gw->hotlist_toolbar_lab[item] = BitMapObj,
						IA_Scalable, TRUE,
						BITMAP_Screen, scrn,
						BITMAP_SourceFile, menu_icon,
						BITMAP_Masking, TRUE,
					BitMapEnd;

	/* \todo make this scale the bitmap to these dimensions */
	SetAttrs(tb_userdata->gw->hotlist_toolbar_lab[item],
				BITMAP_Width, 16,
				BITMAP_Height, 16,
			TAG_DONE);

	Object *lab_item = LabelObj,
				//		LABEL_DrawInfo, dri,
						LABEL_DisposeImage, TRUE,
						LABEL_Image, tb_userdata->gw->hotlist_toolbar_lab[item],
						LABEL_Text, " ",
						LABEL_Text, utf8title,
					LabelEnd;

	free(utf8title);

	speed_button_node = AllocSpeedButtonNode(item,
					SBNA_Image, lab_item,
					SBNA_HintInfo, nsurl_access(url),
					SBNA_UserData, (void *)url,
					TAG_DONE);
			
	AddTail(tb_userdata->sblist, speed_button_node);

	tb_userdata->items++;
	return true;
}

static int ami_gui_hotlist_scan(struct List *speed_button_list, struct gui_window_2 *gwin)
{
	struct ami_gui_tb_userdata userdata;
	userdata.gw = gwin;
	userdata.sblist = speed_button_list;
	userdata.items = 0;

	ami_hotlist_scan((void *)&userdata, 0, messages_get("HotlistToolbar"), ami_gui_hotlist_add);
	return userdata.items;
}

static void ami_gui_hotlist_toolbar_add(struct gui_window_2 *gwin)
{
	struct TagItem attrs[2];

	attrs[0].ti_Tag = CHILD_MinWidth;
	attrs[0].ti_Data = 0;
	attrs[1].ti_Tag = TAG_DONE;
	attrs[1].ti_Data = 0;

	NewList(&gwin->hotlist_toolbar_list);

	if(ami_gui_hotlist_scan(&gwin->hotlist_toolbar_list, gwin) > 0) {
		gwin->objects[GID_HOTLIST] =
				SpeedBarObj,
					GA_ID, GID_HOTLIST,
					GA_RelVerify, TRUE,
					ICA_TARGET, ICTARGET_IDCMP,
					SPEEDBAR_BevelStyle, BVS_NONE,
					SPEEDBAR_Buttons, &gwin->hotlist_toolbar_list,
				SpeedBarEnd;
				
		gwin->objects[GID_HOTLISTSEPBAR] =
				BevelObj,
					BEVEL_Style, BVS_SBAR_VERT,
				BevelEnd;
#ifdef __amigaos4__
		IDoMethod(gwin->objects[GID_HOTLISTLAYOUT], LM_ADDCHILD,
				gwin->win, gwin->objects[GID_HOTLIST], attrs);

		IDoMethod(gwin->objects[GID_HOTLISTLAYOUT], LM_ADDIMAGE,
				gwin->win, gwin->objects[GID_HOTLISTSEPBAR], NULL);

#else
		SetGadgetAttrs((struct Gadget *)gwin->objects[GID_HOTLISTLAYOUT],
			gwin->win, NULL,
			LAYOUT_AddChild, gwin->objects[GID_HOTLIST], TAG_MORE, &attrs);
		SetGadgetAttrs((struct Gadget *)gwin->objects[GID_HOTLISTLAYOUT],
			gwin->win, NULL,
			LAYOUT_AddChild, gwin->objects[GID_HOTLISTSEPBAR], TAG_DONE);
#endif

		FlushLayoutDomainCache((struct Gadget *)gwin->objects[GID_MAIN]);

		RethinkLayout((struct Gadget *)gwin->objects[GID_MAIN],
				gwin->win, NULL, TRUE);
		
		ami_schedule_redraw(gwin, true);
	}
}

static void ami_gui_hotlist_toolbar_free(struct gui_window_2 *gwin, struct List *speed_button_list)
{
	int i;
	struct Node *node;
	struct Node *nnode;

	if(nsoption_bool(kiosk_mode) == true) return;

	if(IsListEmpty(speed_button_list)) return;
	node = GetHead(speed_button_list);

	do {
		nnode = GetSucc(node);
		Remove(node);
		FreeSpeedButtonNode(node);
	} while((node = nnode));

	for(i = 0; i < AMI_GUI_TOOLBAR_MAX; i++) {
		if(gwin->hotlist_toolbar_lab[i]) {
			DisposeObject(gwin->hotlist_toolbar_lab[i]);
			gwin->hotlist_toolbar_lab[i] = NULL;
		}
	}
}

static void ami_gui_hotlist_toolbar_remove(struct gui_window_2 *gwin)
{
#ifdef __amigaos4__
	IDoMethod(gwin->objects[GID_HOTLISTLAYOUT], LM_REMOVECHILD,
			gwin->win, gwin->objects[GID_HOTLIST]);

	IDoMethod(gwin->objects[GID_HOTLISTLAYOUT], LM_REMOVECHILD,
			gwin->win, gwin->objects[GID_HOTLISTSEPBAR]);
#else
	SetGadgetAttrs((struct Gadget *)gwin->objects[GID_HOTLISTLAYOUT],
		gwin->win, NULL,
		LAYOUT_RemoveChild, gwin->objects[GID_HOTLIST], TAG_DONE);
	SetGadgetAttrs((struct Gadget *)gwin->objects[GID_HOTLISTLAYOUT],
		gwin->win, NULL,
		LAYOUT_RemoveChild, gwin->objects[GID_HOTLISTSEPBAR], TAG_DONE);
#endif
	FlushLayoutDomainCache((struct Gadget *)gwin->objects[GID_MAIN]);

	RethinkLayout((struct Gadget *)gwin->objects[GID_MAIN],
			gwin->win, NULL, TRUE);

	ami_schedule_redraw(gwin, true);
}

static void ami_gui_hotlist_toolbar_update(struct gui_window_2 *gwin)
{
	if(IsListEmpty(&gwin->hotlist_toolbar_list)) {
		ami_gui_hotlist_toolbar_add(gwin);
		return;
	}

	/* Below should be SetAttr according to Autodocs */
	SetGadgetAttrs((struct Gadget *)gwin->objects[GID_HOTLIST],
						gwin->win, NULL,
						SPEEDBAR_Buttons, ~0,
						TAG_DONE);

	ami_gui_hotlist_toolbar_free(gwin, &gwin->hotlist_toolbar_list);

	if(ami_gui_hotlist_scan(&gwin->hotlist_toolbar_list, gwin) > 0) {
		SetGadgetAttrs((struct Gadget *)gwin->objects[GID_HOTLIST],
						gwin->win, NULL,
						SPEEDBAR_Buttons, &gwin->hotlist_toolbar_list,
						TAG_DONE);
	} else {
		ami_gui_hotlist_toolbar_remove(gwin);
	}
}

/**
 * Update hotlist toolbar and recreate the menu for all windows
 */
void ami_gui_hotlist_update_all(void)
{
	struct nsObject *node;
	struct nsObject *nnode;
	struct gui_window_2 *gwin;

	if(IsMinListEmpty(window_list))	return;

	ami_gui_menu_refresh_hotlist();

	node = (struct nsObject *)GetHead((struct List *)window_list);

	do {
		nnode=(struct nsObject *)GetSucc((struct Node *)node);
		gwin = node->objstruct;

		if(node->Type == AMINS_WINDOW) {
			if(gwin->ui_nami)
				ami_gui_nami_sidebar_refresh_hotlist(gwin);
			else
				ami_gui_hotlist_toolbar_update(gwin);
		}
	} while((node = nnode));
}

static void ami_toggletabbar(struct gui_window_2 *gwin, bool show)
{
	/* Nami chrome always hosts the clicktab; never tear it out. */
	if(gwin->ui_nami)
		return;

	if(ClickTabBase->lib_Version < 53) return;

	if(show) {
		struct TagItem attrs[3];

		attrs[0].ti_Tag = CHILD_WeightedWidth;
		attrs[0].ti_Data = 0;
		attrs[1].ti_Tag = CHILD_WeightedHeight;
		attrs[1].ti_Data = 0;
		attrs[2].ti_Tag = TAG_DONE;
		attrs[2].ti_Data = 0;

		gwin->objects[GID_TABS] = ClickTabObj,
					GA_ID, GID_TABS,
					GA_RelVerify, TRUE,
					GA_Underscore, 13, // disable kb shortcuts
					GA_ContextMenu, ami_ctxmenu_clicktab_create(gwin, &gwin->clicktab_ctxmenu),
					ICA_TARGET, ICTARGET_IDCMP,
					CLICKTAB_Labels, &gwin->tab_list,
					CLICKTAB_LabelTruncate, TRUE,
					CLICKTAB_AutoFit, TRUE,
					CLICKTAB_CloseImage, gwin->objects[GID_CLOSETAB_BM],
					CLICKTAB_ClosePlacement, PLACECLOSE_RIGHT,
					CLICKTAB_FlagImage, gwin->objects[GID_TABS_FLAG],
#ifdef __amigaos4__
					CLICKTAB_EvenSize, FALSE,
#endif
					ClickTabEnd;

#ifdef __amigaos4__
		IDoMethod(gwin->objects[GID_TABLAYOUT], LM_ADDCHILD,
				gwin->win, gwin->objects[GID_TABS], NULL);
#else
		SetGadgetAttrs((struct Gadget *)gwin->objects[GID_TABLAYOUT],
				gwin->win, NULL,
				LAYOUT_AddChild, gwin->objects[GID_TABS], TAG_DONE);
#endif
	} else {
#ifdef __amigaos4__
		IDoMethod(gwin->objects[GID_TABLAYOUT], LM_REMOVECHILD,
				gwin->win, gwin->objects[GID_TABS]);
#else
		SetGadgetAttrs((struct Gadget *)gwin->objects[GID_TABLAYOUT],
				gwin->win, NULL,
				LAYOUT_RemoveChild, gwin->objects[GID_TABS], TAG_DONE);
#endif

		gwin->objects[GID_TABS] = NULL;
	}

	FlushLayoutDomainCache((struct Gadget *)gwin->objects[GID_MAIN]);

	RethinkLayout((struct Gadget *)gwin->objects[GID_MAIN],
			gwin->win, NULL, TRUE);

	if (gwin->gw && gwin->gw->bw) {
		gui_window_set_title(gwin->gw,
				     browser_window_get_title(gwin->gw->bw));
		gui_window_update_extent(gwin->gw);
		amiga_window_invalidate_area(gwin->gw, NULL);
	}
}

void ami_gui_tabs_toggle_all(void)
{
	struct nsObject *node;
	struct nsObject *nnode;
	struct gui_window_2 *gwin;

	if(IsMinListEmpty(window_list))	return;

	node = (struct nsObject *)GetHead((struct List *)window_list);

	do {
		nnode=(struct nsObject *)GetSucc((struct Node *)node);
		gwin = node->objstruct;

		if(node->Type == AMINS_WINDOW)
		{
			if(gwin->tabs == 1) {
				if(nsoption_bool(tab_always_show) == true) {
					ami_toggletabbar(gwin, true);
				} else {
					ami_toggletabbar(gwin, false);
				}
			}
		}
	} while((node = nnode));
}


/**
 * Count windows, and optionally tabs.
 *
 * \param  window    window to count tabs of
 * \param  tabs      if window > 0, will be updated to contain the number of tabs
 *                   in that window, unchanged otherwise
 * \return number of windows currently open
 */
int ami_gui_count_windows(int window, int *tabs)
{
	int windows = 0;
	struct nsObject *node, *nnode;
	struct gui_window_2 *gwin;

	if(!IsMinListEmpty(window_list)) {
		node = (struct nsObject *)GetHead((struct List *)window_list);
		do {
			nnode=(struct nsObject *)GetSucc((struct Node *)node);

			gwin = node->objstruct;

			if(node->Type == AMINS_WINDOW) {
				windows++;
				if(window == windows) *tabs = gwin->tabs;
			}
		} while((node = nnode));
	}
	return windows;
}

/**
 * Set the scale of a gui window
 *
 * \param gw	gui_window to set scale for
 * \param scale	scale to set
 */
void ami_gui_set_scale(struct gui_window *gw, float scale)
{
	browser_window_set_scale(gw->bw, scale, true);
	ami_schedule_redraw(gw->shared, true);
}

void ami_gui_adjust_scale(struct gui_window *gw, float adjustment)
{
	browser_window_set_scale(gw->bw, adjustment, false);
	ami_schedule_redraw(gw->shared, true);
}

nserror ami_gui_new_blank_tab(struct gui_window_2 *gwin)
{
	nsurl *url;
	nserror error;
	struct browser_window *bw = NULL;
	const char *addr;

	/* Nami new tabs use the built-in home (history / hotlist / search). */
	addr = "about:home";
	if(gwin == NULL || gwin->ui_nami == false) {
		addr = nsoption_charp(homepage_url);
		if(addr == NULL)
			addr = NETSURF_HOMEPAGE;
	}

	error = nsurl_create(addr, &url);
	if (error == NSERROR_OK) {
		error = browser_window_create(BW_CREATE_HISTORY |
					      BW_CREATE_TAB | BW_CREATE_FOREGROUND,
					      url,
					      NULL,
					      gwin->gw->bw,
					      &bw);
		nsurl_unref(url);
	}
	if (error != NSERROR_OK) {
		amiga_warn_user(messages_get_errorcode(error), 0);
		return error;
	}

	return NSERROR_OK;
}

static void ami_do_redraw_tiled(struct gui_window_2 *gwin, bool busy,
	int left, int top, int width, int height,
	int sx, int sy, struct IBox *bbox, struct redraw_context *ctx)
{
	struct gui_globals *glob = (struct gui_globals *)ctx->priv;
	int x, y;
	struct rect clip;
	int tile_size_x;
	int tile_size_y;
	int step_y;

	ami_plot_ra_get_size(glob, &tile_size_x, &tile_size_y);
	ami_plot_ra_set_pen_list(glob, gwin->shared_pens);
	
	if(top < 0) {
		height += top;
		top = 0;
	}

	if(left < 0) {
		width += left;
		left = 0;
	}

	if(top < sy) {
		height += (top - sy);
		top = sy;
	}
	if(left < sx) {
		width += (left - sx);
		left = sx;
	}

	if(((top - sy) + height) > bbox->Height)
		height = bbox->Height - (top - sy);

	if(((left - sx) + width) > bbox->Width)
		width = bbox->Width - (left - sx);

	if(width <= 0) return;
	if(height <= 0) return;

	/*
	 * Even with a screen-sized plot buffer (OS3 tile size 0), paint in
	 * horizontal strips so we can drain IDCMP between bands. Full-height
	 * single-pass paint held the UI task for seconds on image-heavy pages.
	 */
	step_y = tile_size_y;
	if(step_y <= 0 || step_y > AMI_PAINT_YIELD_STRIP)
		step_y = AMI_PAINT_YIELD_STRIP;

	if(busy) ami_set_pointer(gwin, GUI_POINTER_WAIT, false);

	ami_gui_paint_depth++;

	for(y = top; y < (top + height); y += step_y) {
		clip.y0 = 0;
		clip.y1 = step_y;
		if(clip.y1 > tile_size_y)
			clip.y1 = tile_size_y;
		if(clip.y1 > ((top + height) - y)) {
			clip.y1 = (top + height) - y;
		}
		if(((y - sy) + clip.y1) > bbox->Height)
			clip.y1 = bbox->Height - (y - sy);
		if(clip.y1 <= 0) {
			break;
		}

		for(x = left; x < (left + width); x += tile_size_x) {
			clip.x0 = 0;
			clip.x1 = tile_size_x;
			if(clip.x1 > ((left + width) - x)) {
				clip.x1 = (left + width) - x;
			}
			if(((x - sx) + clip.x1) > bbox->Width)
				clip.x1 = bbox->Width - (x - sx);
			if(clip.x1 <= 0) {
				break;
			}

			if(browser_window_redraw(gwin->gw->bw,
				clip.x0 - (int)x,
				clip.y0 - (int)y,
				&clip, ctx))
			{
				ami_clearclipreg(glob);
#ifdef __amigaos4__
				BltBitMapTags(BLITA_SrcType, BLITT_BITMAP, 
					BLITA_Source, ami_plot_ra_get_bitmap(glob),
					BLITA_SrcX, 0,
					BLITA_SrcY, 0,
					BLITA_DestType, BLITT_RASTPORT, 
					BLITA_Dest, gwin->win->RPort,
					BLITA_DestX, bbox->Left + (int)(x - sx),
					BLITA_DestY, bbox->Top + (int)(y - sy),
					BLITA_Width, (int)(clip.x1),
					BLITA_Height, (int)(clip.y1),
					TAG_DONE);
#else
				BltBitMapRastPort(ami_plot_ra_get_bitmap(glob), 0, 0, gwin->win->RPort,
					bbox->Left + (int)(x - sx),
					bbox->Top + (int)(y - sy),
					(int)(clip.x1), (int)(clip.y1), 0xC0);
#endif
			}
		}

		/* Let resize / scroll / close land between strips */
		ami_gui_yield_input();
		if(gwin->closed || gwin->win == NULL || win_destroyed)
			break;
	}

	ami_gui_paint_depth--;

	if(gwin->closed && gwin->win != NULL)
		ami_gui_close_window(gwin);
	
	if(busy) ami_reset_pointer(gwin);
}


/**
 * Redraw an area of the browser window - Amiga-specific function
 *
 * \param  g   a struct gui_window 
 * \param  bw  a struct browser_window
 * \param  busy  busy flag passed to tiled redraw.
 * \param  x0  top-left co-ordinate (in document co-ordinates)
 * \param  y0  top-left co-ordinate (in document co-ordinates)
 * \param  x1  bottom-right co-ordinate (in document co-ordinates)
 * \param  y1  bottom-right co-ordinate (in document co-ordinates)
 */

static void ami_do_redraw_limits(struct gui_window *g, struct browser_window *bw, bool busy,
		int x0, int y0, int x1, int y1)
{
	struct IBox *bbox;
	ULONG sx, sy;

	struct redraw_context ctx = {
		.interactive = true,
		.background_images = true,
		.plot = &amiplot,
		.priv = browserglob
	};

	if(!g) return;
	if(browser_window_redraw_ready(bw) == false) return;

	sx = g->scrollx;
	sy = g->scrolly;

	if(g != g->shared->gw) return;

	if(ami_gui_get_space_box((Object *)g->shared->objects[GID_BROWSER], &bbox) != NSERROR_OK) {
		amiga_warn_user("NoMemory", "");
		return;
	}

	ami_do_redraw_tiled(g->shared, busy, x0, y0,
		x1 - x0, y1 - y0, sx, sy, bbox, &ctx);

	ami_gui_free_space_box(bbox);

	return;
}


static void ami_refresh_window(struct gui_window_2 *gwin)
{
	/* simplerefresh only */

	struct IBox *bbox;
	int sx, sy;
	struct RegionRectangle *regrect;
	struct rect r;

	sx = gwin->gw->scrollx;
	sy = gwin->gw->scrolly;

	ami_set_pointer(gwin, GUI_POINTER_WAIT, false);

	if(ami_gui_get_space_box((Object *)gwin->objects[GID_BROWSER], &bbox) != NSERROR_OK) {
		amiga_warn_user("NoMemory", "");
		return;
	}
	
	BeginRefresh(gwin->win);

	r.x0 = (gwin->win->RPort->Layer->DamageList->bounds.MinX - bbox->Left) + sx - 1;
	r.x1 = (gwin->win->RPort->Layer->DamageList->bounds.MaxX - bbox->Left) + sx + 2;
	r.y0 = (gwin->win->RPort->Layer->DamageList->bounds.MinY - bbox->Top) + sy - 1;
	r.y1 = (gwin->win->RPort->Layer->DamageList->bounds.MaxY - bbox->Top) + sy + 2;

	regrect = gwin->win->RPort->Layer->DamageList->RegionRectangle;

	amiga_window_invalidate_area(gwin->gw, &r);

	while(regrect)
	{
		r.x0 = (regrect->bounds.MinX - bbox->Left) + sx - 1;
		r.x1 = (regrect->bounds.MaxX - bbox->Left) + sx + 2;
		r.y0 = (regrect->bounds.MinY - bbox->Top) + sy - 1;
		r.y1 = (regrect->bounds.MaxY - bbox->Top) + sy + 2;

		regrect = regrect->Next;

		amiga_window_invalidate_area(gwin->gw, &r);
	}

	EndRefresh(gwin->win, TRUE);

	ami_gui_free_space_box(bbox);
	if(gwin->ui_nami) {
		if(gwin->objects[GID_CHROMELAYOUT] != NULL)
			RefreshGList((struct Gadget *)gwin->objects[GID_CHROMELAYOUT],
					gwin->win, NULL, 1);
		if(gwin->sidebar_expanded &&
		   gwin->objects[GID_SIDELAYOUT] != NULL)
			RefreshGList((struct Gadget *)gwin->objects[GID_SIDELAYOUT],
					gwin->win, NULL, 1);
		ami_gui_nami_paint_chrome_rborder(gwin);
		ami_throbber_redraw_schedule(0, gwin->gw);
	}
	ami_reset_pointer(gwin);
}

HOOKF(void, ami_scroller_hook, Object *, object, struct IntuiMessage *)
{
	ULONG gid;
	struct gui_window_2 *gwin = hook->h_Data;
	struct IntuiWheelData *wheel;
	struct Node *node = NULL;
	nsurl *url;

	/*
	 * RelVerify sidebar tabs often never deliver GADGETDOWN / SELECTDOWN
	 * to RA_HandleInput.  Arm on LMB+MOUSEMOVE while over a tab, then let
	 * FilterIMsg see the synthesised GADGETDOWN + this MOUSEMOVE.
	 * CreateDragObj's FakeInputEvent LMB-up must not re-arm (can_arm).
	 */
	if(gwin->ui_nami && ami_gtdrag_available() && gwin->gtdrag_registered) {
		if(msg->Class == IDCMP_MOUSEMOVE && ami_gtdrag_armed())
			ami_gui_nami_gtdrag_apply_abs(gwin);
		else if(msg->Class == IDCMP_MOUSEBUTTONS &&
			msg->Code == SELECTUP &&
			ami_gtdrag_armed())
			ami_gui_nami_gtdrag_apply_abs(gwin);
		else if(msg->Class == IDCMP_MOUSEMOVE &&
			  (msg->Qualifier & IEQUALIFIER_LEFTBUTTON) &&
			  !ami_gtdrag_armed() &&
			  ami_gtdrag_can_arm(msg->Qualifier))
			(void)ami_gui_nami_gtdrag_try_arm_ex(gwin, false);
		else if(msg->Class == IDCMP_GADGETDOWN &&
			msg->IAddress != NULL) {
			ULONG tab_gid;

			tab_gid = ((struct Gadget *)msg->IAddress)->GadgetID;
			if(((tab_gid >= (ULONG)GID_SIDE_TAB_BASE &&
			     tab_gid < (ULONG)(GID_SIDE_TAB_BASE + AMI_SIDE_TAB_MAX)) ||
			    (tab_gid >= (ULONG)GID_SIDE_TAB_ICON_BASE &&
			     tab_gid < (ULONG)(GID_SIDE_TAB_ICON_BASE + AMI_SIDE_TAB_MAX))) &&
			   !ami_gtdrag_armed() &&
			   ami_gtdrag_can_arm(msg->Qualifier))
				(void)ami_gui_nami_gtdrag_try_arm_ex(gwin, true);
		}
	}

	ami_gtdrag_filter_imsg(msg);

	/*
	 * CreateDragObj holds LockLayers until FreeDragObj.  Feed SELECTUP while
	 * armed so UnlockLayers runs (otherwise the UI freezes).  Cross-window
	 * drops often deliver SELECTUP/ticks to the destination window — always
	 * unlock the armed window.  After FakeInputEvent, Intuition may omit
	 * SELECTUP entirely; unlock when PeekQualifier says LMB is up.
	 */
	if(gwin->ui_nami && ami_gtdrag_available() && ami_gtdrag_armed() &&
	   ((msg->Class == IDCMP_MOUSEBUTTONS && msg->Code == SELECTUP) ||
	    ((msg->Class == IDCMP_MOUSEMOVE ||
	      msg->Class == IDCMP_INTUITICKS) &&
	     !ami_gtdrag_lmb_physically_down()))) {
		struct Window *arm_win;
		struct gui_window_2 *arm_gwin;

		arm_win = ami_gtdrag_arm_window();
		arm_gwin = (arm_win != NULL) ?
				ami_find_gwin_by_id(arm_win, AMINS_WINDOW) : NULL;
		if(arm_gwin != NULL)
			ami_gui_nami_gtdrag_apply_abs(arm_gwin);
		(void)ami_gtdrag_select_up(arm_win);
		ami_gui_nami_gtdrag_restore_applied();
		ami_gtdrag_discard_drops();
		ami_gui_nami_sidebar_drop_wells_refresh_all();
		NSLOG(netsurf, INFO,
				"gtdrag: end-drag UnlockLayers; await real release");
	} else if(gwin->ui_nami && ami_gtdrag_available() &&
		  msg->Class == IDCMP_MOUSEBUTTONS && msg->Code == SELECTUP) {
		ami_gtdrag_note_lmb_up();
		ami_gui_nami_gtdrag_restore_applied();
		if(ami_gtdrag_should_poll_drop(msg->Qualifier)) {
			ami_gui_nami_gtdrag_finish_drop();
			ami_gtdrag_drop_polled();
		}
	} else if(gwin->ui_nami && ami_gtdrag_available() &&
		  (msg->Class == IDCMP_MOUSEMOVE ||
		   msg->Class == IDCMP_INTUITICKS) &&
		  ami_gtdrag_should_poll_drop(msg->Qualifier)) {
		/* Physical LMB up after UnlockLayers — Intuition may omit SELECTUP. */
		ami_gui_nami_gtdrag_restore_applied();
		ami_gui_nami_gtdrag_finish_drop();
		ami_gtdrag_drop_polled();
	} else if(gwin->ui_nami && !ami_gtdrag_armed()) {
		ami_gui_nami_gtdrag_restore_applied();
	}

	switch(msg->Class)
	{
		case IDCMP_IDCMPUPDATE:
			gid = GetTagData( GA_ID, 0, msg->IAddress );

			switch( gid ) 
			{
				case GID_HSCROLL:
 				case GID_VSCROLL:
					/* Always ScrollWindowRaster + strip; short coalesce */
					gwin->redraw_scroll = true;
					ami_schedule_redraw_interactive(gwin, true);
 				break;

				case GID_TABS:
					/* V47 clicktab sends OM_NOTIFY for CLICKTAB_NodeClosed */
					if(ami_clicktab_has_close()) {
						node = (struct Node *)GetTagData(
								CLICKTAB_NodeClosed, 0, msg->IAddress);
						if(node != NULL)
							ami_gui_close_clicktab_node(gwin, node);
					}
				break;
				
				case GID_HOTLIST:
					if((node = (struct Node *)GetTagData(SPEEDBAR_SelectedNode, 0, msg->IAddress))) {
						GetSpeedButtonNodeAttrs(node, SBNA_UserData, (ULONG *)&url, TAG_DONE);

						if(gwin->key_state & BROWSER_MOUSE_MOD_2) {
							browser_window_create(BW_CREATE_TAB,
										      url,
										      NULL,
										      gwin->gw->bw,
										      NULL);
						} else {
							browser_window_navigate(gwin->gw->bw,
									url,
									NULL,
									BW_NAVIGATE_HISTORY,
									NULL,
									NULL,
									NULL);

						}
					}
				break;

			} 
		break;
		case IDCMP_EXTENDEDMOUSE:
			/* OS3.2 / V47 Intuition wheel (NDK3.2 IDCMP_EXTENDEDMOUSE) */
			if (msg->Code == IMSGCODE_INTUIWHEELDATA) {
				wheel = (struct IntuiWheelData *)msg->IAddress;
				if (wheel != NULL) {
					ami_gui_scroll_internal(gwin,
							wheel->WheelX * NSA_WHEEL_SCROLL_PX,
							wheel->WheelY * NSA_WHEEL_SCROLL_PX);
				}
			}
		break;
		case IDCMP_SIZEVERIFY:
			/* No longer requested on browser windows — Intuition
			 * completed the size without waiting on us. */
		break;

		case IDCMP_REFRESHWINDOW:
			ami_refresh_window(gwin);
		break;

		case IDCMP_MOUSEMOVE:
		case IDCMP_MOUSEBUTTONS:
		case IDCMP_GADGETDOWN:
		case IDCMP_INTUITICKS:
		case IDCMP_OBJECTDROP:
			/* Handled above for gtdrag; RA_HandleInput also sees these. */
		break;

		default:
			NSLOG(netsurf, INFO,
			      "IDCMP hook unhandled event: %ld", msg->Class);
		break;
	}
//	ReplyMsg((struct Message *)msg);
} 

/* exported function documented in gui.h */
nserror ami_gui_win_list_add(void *win, int type, const struct ami_win_event_table *table)
{
	struct nsObject *node = AddObject(window_list, type);
	if(node == NULL) return NSERROR_NOMEM;
	node->objstruct = win;

	struct ami_generic_window *w = (struct ami_generic_window *)win;
	w->tbl = table;
	w->node = node;

	return NSERROR_OK;
}

/* exported function documented in gui.h */
void ami_gui_win_list_remove(void *win)
{
	struct ami_generic_window *w = (struct ami_generic_window *)win;

	if(w->node->Type == AMINS_TVWINDOW) {
		DelObjectNoFree(w->node);
	} else {
		DelObject(w->node);
	}
}

static const struct ami_win_event_table ami_gui_table = {
	ami_gui_event,
	ami_gui_close_window,
};

static struct gui_window *
gui_window_create(struct browser_window *bw,
		struct gui_window *existing,
		gui_window_create_flags flags)
{
	struct gui_window *g = NULL;
	struct Window *ref = NULL;
	char nav_west[100],nav_west_s[100],nav_west_g[100];
	char nav_east[100],nav_east_s[100],nav_east_g[100];
	char stop[100],stop_s[100],stop_g[100];
	char reload[100],reload_s[100],reload_g[100];
	char home[100],home_s[100],home_g[100];
	char closetab[100],closetab_s[100],closetab_g[100];
	char addtab[100],addtab_s[100],addtab_g[100];
	char fave[100], unfave[100];
	char pi_insecure[100], pi_internal[100], pi_local[100], pi_secure[100], pi_warning[100];
	char tabthrobber[100];
	ULONG refresh_mode = WA_SmartRefresh;
	ULONG defer_layout = TRUE;
	/* Do not take IDCMP_SIZEVERIFY: Intuition waits for us to reply before
	 * completing a size change, which freezes resize while paint/layout runs.
	 */
	ULONG idcmp_sizeverify = 0;
	ULONG gtdrag_idcmp = ami_gtdrag_idcmp_bits();

	NSLOG(netsurf, INFO, "Creating window");

	if (!scrn) ami_openscreenfirst();

	if (nsoption_bool(kiosk_mode)) flags &= ~GW_CREATE_TAB;

	if(existing) {
		ref = existing->shared->win;
	}

	g = calloc(1, sizeof(struct gui_window));

	if(!g)
	{
		amiga_warn_user("NoMemory","");
		return NULL;
	}

	g->sidebar_tab_slot = -1;

	NewList(&g->dllist);
	g->deferred_rects = NewObjList();
	g->deferred_rects_pool = ami_memory_itempool_create(sizeof(struct rect));
	g->bw = bw;

	NewList(&g->loglist);
	g->logcolumns = NULL;
	/* AllocLBColumnInfo needs listbrowser.gadget V45+ (present on OS3.2) */
	if(ListBrowserBase->lib_Version >= 45) {
		g->logcolumns = AllocLBColumnInfo(4,
			LBCIA_Column, 0,
				LBCIA_Title, "time",
				LBCIA_Weight, 10,
				LBCIA_DraggableSeparator, TRUE,
				LBCIA_Separator, TRUE,
			LBCIA_Column, 1,
				LBCIA_Title, "source",
				LBCIA_Weight, 10,
				LBCIA_DraggableSeparator, TRUE,
				LBCIA_Separator, TRUE,
			LBCIA_Column, 2,
				LBCIA_Title, "level",
				LBCIA_Weight, 5,
				LBCIA_DraggableSeparator, TRUE,
				LBCIA_Separator, TRUE,
			LBCIA_Column, 3,
				LBCIA_Title, "message",
				LBCIA_Weight, 75,
				LBCIA_DraggableSeparator, TRUE,
				LBCIA_Separator, TRUE,
			TAG_DONE);
	}

	if((flags & GW_CREATE_TAB) && existing)
	{
		g->shared = existing->shared;
		g->tab = g->shared->next_tab;
		g->shared->tabs++; /* do this early so functions know to update the tabs */

		if((g->shared->tabs == 2) && (nsoption_bool(tab_always_show) == false)) {
			ami_toggletabbar(g->shared, true);
		}

		if(g->shared->ui_nami) {
			g->tab_node = AllocClickTabNode(TNA_Text, messages_get("NetSurf"),
									TNA_Number, g->tab,
									TNA_UserData, g,
									TNA_CloseGadget, TRUE,
									TAG_DONE);

			{
				struct Node *insert_after = existing->tab_node;

				if(g->shared->last_new_tab)
					insert_after = g->shared->last_new_tab;
				Insert(&g->shared->tab_list, g->tab_node, insert_after);
				g->shared->last_new_tab = g->tab_node;
			}

			NSLOG(netsurf, INFO, "Nami: adding sidebar tab button");
			ami_gui_nami_sidebar_create_tab_btn(g);
			NSLOG(netsurf, INFO, "Nami: sidebar tab button done");
		} else {
			SetGadgetAttrs((struct Gadget *)g->shared->objects[GID_TABS],
							g->shared->win, NULL,
							CLICKTAB_Labels, ~0,
							TAG_DONE);

			g->tab_node = AllocClickTabNode(TNA_Text, messages_get("NetSurf"),
									TNA_Number, g->tab,
									TNA_UserData, g,
									TNA_CloseGadget, TRUE,
									TAG_DONE);

			{
				struct Node *insert_after = existing->tab_node;

				if(g->shared->last_new_tab)
					insert_after = g->shared->last_new_tab;
				Insert(&g->shared->tab_list, g->tab_node, insert_after);
			}

			g->shared->last_new_tab = g->tab_node;

			RefreshSetGadgetAttrs((struct Gadget *)g->shared->objects[GID_TABS],
								g->shared->win, NULL,
								CLICKTAB_Labels, &g->shared->tab_list,
								TAG_DONE);

			if(flags & GW_CREATE_FOREGROUND) {
				RefreshSetGadgetAttrs((struct Gadget *)g->shared->objects[GID_TABS],
								g->shared->win, NULL,
								CLICKTAB_Current, g->tab,
								TAG_DONE);
			}

			if(ClickTabBase->lib_Version < 53) {
				RethinkLayout((struct Gadget *)g->shared->objects[GID_TABLAYOUT],
					g->shared->win, NULL, TRUE);
			}
		}

		if(g->shared->ui_nami == false)
			ami_gui_relabel_all_tabs(g->shared);

		g->shared->next_tab++;

		if(flags & GW_CREATE_FOREGROUND) {
			if(g->shared->ui_nami)
				ami_switch_tab_to(g->shared, g, true);
			else
				ami_switch_tab(g->shared, false);
		}

		ami_update_buttons(g->shared);
		ami_schedule(0, ami_gui_refresh_favicon, g->shared);

		return g;
	}

	g->shared = calloc(1, sizeof(struct gui_window_2));

	if(!g->shared)
	{
		amiga_warn_user("NoMemory","");
		return NULL;
	}

	/* Freeze chrome style before theme/image setup */
	g->shared->ui_nami = (nsoption_int(ui_style) == 1) ? true : false;

	g->shared->shared_pens = ami_AllocMinList();

	g->shared->scrollerhook.h_Entry = (void *)ami_scroller_hook;
	g->shared->scrollerhook.h_Data = g->shared;

	g->shared->favicon_hook.h_Entry = (void *)ami_set_favicon_render_hook;
	g->shared->favicon_hook.h_Data = g->shared;

	g->shared->throbber_hook.h_Entry = (void *)ami_set_throbber_render_hook;
	g->shared->throbber_hook.h_Data = g->shared;

	g->shared->browser_hook.h_Entry = (void *)ami_gui_browser_render_hook;
	g->shared->browser_hook.h_Data = g->shared;

	/* Nami chrome: sysiclass zoom/depth drawn via SpaceObj render hooks */
	g->shared->chrome_zoom_hook.h_Entry = (void *)ami_gui_chrome_sysi_render_hook;
	g->shared->chrome_zoom_hook.h_Data = g->shared;
	g->shared->chrome_depth_hook.h_Entry = (void *)ami_gui_chrome_sysi_render_hook;
	g->shared->chrome_depth_hook.h_Data = g->shared;

	newprefs_hook.h_Entry = (void *)ami_gui_newprefs_hook;
	newprefs_hook.h_Data = 0;
	
	g->shared->ctxmenu_hook = ami_ctxmenu_get_hook(g->shared);
	g->shared->history_ctxmenu[AMI_CTXMENU_HISTORY_BACK] = NULL;
	g->shared->history_ctxmenu[AMI_CTXMENU_HISTORY_FORWARD] = NULL;
	g->shared->clicktab_ctxmenu = NULL;

	if(nsoption_bool(window_simple_refresh) == true) {
		refresh_mode = WA_SimpleRefresh;
		defer_layout = FALSE; /* testing reveals this does work with SimpleRefresh,
								but the docs say it doesn't so err on the side of caution. */
	} else {
		refresh_mode = WA_SmartRefresh;
		defer_layout = TRUE;
	}

	if(!nsoption_bool(kiosk_mode))
	{
		ULONG add_closetab_gadget = TAG_IGNORE;
		ULONG add_tabs_gadget = TAG_IGNORE;
		ULONG closetab_weight_w = TAG_IGNORE;
		ULONG closetab_weight_h = TAG_IGNORE;
		ULONG iconifygadget = FALSE;
		ULONG throbber_w = 24;
		ULONG throbber_h = 24;
		int ws_idx = 0;

#ifdef __amigaos4__
		if (nsoption_charp(pubscreen_name) && 
		    (locked_screen == TRUE) &&
		    (strcmp(nsoption_charp(pubscreen_name), "Workbench") == 0))
				iconifygadget = TRUE;
#endif

		NSLOG(netsurf, INFO, "Creating menu");
		struct Menu *menu = ami_gui_menu_create(g->shared);

		NewList(&g->shared->tab_list);
		g->shared->sidebar_expanded = true;
		g->shared->sidebar_weight = 20;
		g->shared->sidebar_def_icon = NULL;
		g->shared->side_hot_count = 0;
		g->shared->gtdrag_registered = false;

		g->tab_node = AllocClickTabNode(TNA_Text,messages_get("NetSurf"),
											TNA_Number, 0,
											TNA_UserData, g,
											TNA_CloseGadget, TRUE,
											TAG_DONE);
		AddTail(&g->shared->tab_list,g->tab_node);


		g->shared->web_search_list = ami_gui_opts_websearch(&ws_idx);
		g->shared->search_bm = NULL;

		g->shared->tabs=1;
		g->shared->next_tab=1;

		g->shared->svbuffer = calloc(1, 2000);

		g->shared->helphints[GID_BACK] =
			translate_escape_chars(messages_get("HelpToolbarBack"));
		g->shared->helphints[GID_FORWARD] =
			translate_escape_chars(messages_get("HelpToolbarForward"));
		g->shared->helphints[GID_STOP] =
			translate_escape_chars(messages_get("HelpToolbarStop"));
		g->shared->helphints[GID_RELOAD] =
			translate_escape_chars(messages_get("HelpToolbarReload"));
		g->shared->helphints[GID_HOME] =
			translate_escape_chars(messages_get("HelpToolbarHome"));
		g->shared->helphints[GID_URL] =
			translate_escape_chars(messages_get("HelpToolbarURL"));
		g->shared->helphints[GID_SEARCHSTRING] =
			translate_escape_chars(messages_get("HelpToolbarWebSearch"));
		/* Nami omni bar: URL gadget also runs search_web_omni */
		if(g->shared->ui_nami && g->shared->helphints[GID_URL] != NULL) {
			char *omni_hint;

			omni_hint = ami_utf8_easy(
				"Enter a web address or search terms");
			if(omni_hint != NULL) {
				ami_utf8_free(g->shared->helphints[GID_URL]);
				g->shared->helphints[GID_URL] = omni_hint;
			}
		}
		g->shared->helphints[GID_ADDTAB_HINT] =
			translate_escape_chars(messages_get("HelpToolbarAddTab"));
		g->shared->helphints[GID_CLOSETAB] =
			ami_utf8_easy(messages_get("CloseTab"));
		g->shared->helphints[GID_SIDE_TOGGLE] =
			ami_utf8_easy(messages_get("Hotlist")); /* sidebar show/hide */

		/*
		 * Nami bubble help: first line only — Messages embed "\nLMB: ..."
		 * secondary rows that look wrong in GA_GadgetHelpText.
		 */
		if(g->shared->ui_nami) {
			int hi;
			WORD hint_ids[8];
			char *h;
			char *nl;

			hint_ids[0] = GID_BACK;
			hint_ids[1] = GID_FORWARD;
			hint_ids[2] = GID_STOP;
			hint_ids[3] = GID_RELOAD;
			hint_ids[4] = GID_HOME;
			hint_ids[5] = GID_URL;
			hint_ids[6] = GID_SEARCHSTRING;
			hint_ids[7] = GID_ADDTAB_HINT;
			for(hi = 0; hi < 8; hi++) {
				h = g->shared->helphints[hint_ids[hi]];
				if(h == NULL)
					continue;
				nl = strchr(h, '\n');
				if(nl != NULL)
					*nl = '\0';
			}
		}

		g->shared->helphints[GID_PAGEINFO_INSECURE_BM] = ami_utf8_easy(messages_get("PageInfoInsecure"));
		g->shared->helphints[GID_PAGEINFO_LOCAL_BM] = ami_utf8_easy(messages_get("PageInfoLocal"));
		g->shared->helphints[GID_PAGEINFO_SECURE_BM] = ami_utf8_easy(messages_get("PageInfoSecure"));
		g->shared->helphints[GID_PAGEINFO_WARNING_BM] = ami_utf8_easy(messages_get("PageInfoWarning"));
		g->shared->helphints[GID_PAGEINFO_INTERNAL_BM] = ami_utf8_easy(messages_get("PageInfoInternal"));

		g->shared->helphints[GID_FAVE_ADD] = ami_utf8_easy(messages_get("HelpToolbarHotlistStar"));
		g->shared->helphints[GID_FAVE_RMV] = ami_utf8_easy(messages_get("HelpToolbarHotlistStarLit"));

		ami_get_theme_filename(nav_west, "theme_nav_west", false);
		ami_get_theme_filename(nav_west_s, "theme_nav_west_s", false);
		ami_get_theme_filename(nav_west_g, "theme_nav_west_g", false);
		ami_get_theme_filename(nav_east, "theme_nav_east", false);
		ami_get_theme_filename(nav_east_s, "theme_nav_east_s", false);
		ami_get_theme_filename(nav_east_g, "theme_nav_east_g", false);
		ami_get_theme_filename(stop, "theme_stop", false);
		ami_get_theme_filename(stop_s, "theme_stop_s", false);
		ami_get_theme_filename(stop_g, "theme_stop_g", false);
		ami_get_theme_filename(reload, "theme_reload", false);
		ami_get_theme_filename(reload_s, "theme_reload_s", false);
		ami_get_theme_filename(reload_g, "theme_reload_g", false);
		ami_get_theme_filename(home, "theme_home", false);
		ami_get_theme_filename(home_s, "theme_home_s", false);
		ami_get_theme_filename(home_g, "theme_home_g", false);
		/* Nami: AISS list_ glyphs (16×16) for back/forward/stop/reload */
		if(g->shared->ui_nami) {
			ami_gui_prefer_list_tbimage(nav_west);
			ami_gui_prefer_list_tbimage(nav_west_s);
			ami_gui_prefer_list_tbimage(nav_west_g);
			ami_gui_prefer_list_tbimage(nav_east);
			ami_gui_prefer_list_tbimage(nav_east_s);
			ami_gui_prefer_list_tbimage(nav_east_g);
			ami_gui_prefer_list_tbimage(stop);
			ami_gui_prefer_list_tbimage(stop_s);
			ami_gui_prefer_list_tbimage(stop_g);
			ami_gui_prefer_list_tbimage(reload);
			ami_gui_prefer_list_tbimage(reload_s);
			ami_gui_prefer_list_tbimage(reload_g);
		}
		ami_get_theme_filename(closetab, "theme_closetab", false);
		ami_get_theme_filename(closetab_s, "theme_closetab_s", false);
		ami_get_theme_filename(closetab_g, "theme_closetab_g", false);
		ami_get_theme_filename(addtab, "theme_addtab", false);
		ami_get_theme_filename(addtab_s, "theme_addtab_s", false);
		ami_get_theme_filename(addtab_g, "theme_addtab_g", false);
		ami_get_theme_filename(tabthrobber, "theme_tab_loading", false);
		ami_get_theme_filename(fave, "theme_fave", false);
		ami_get_theme_filename(unfave, "theme_unfave", false);
		ami_get_theme_filename(pi_insecure, "theme_pageinfo_insecure", false);
		ami_get_theme_filename(pi_internal, "theme_pageinfo_internal", false);
		ami_get_theme_filename(pi_local, "theme_pageinfo_local", false);
		ami_get_theme_filename(pi_secure, "theme_pageinfo_secure", false);
		ami_get_theme_filename(pi_warning, "theme_pageinfo_warning", false);

		g->shared->objects[GID_FAVE_ADD] = BitMapObj,
					BITMAP_SourceFile, fave,
					BITMAP_Screen, scrn,
					BITMAP_Masking, TRUE,
					BitMapEnd;

		g->shared->objects[GID_FAVE_RMV] = BitMapObj,
					BITMAP_SourceFile, unfave,
					BITMAP_Screen, scrn,
					BITMAP_Masking, TRUE,
					BitMapEnd;

		if(ami_clicktab_has_close()) {
			/* Grey theme glyph — avoids AllocBitMap snapshot MemList issues */
			g->shared->objects[GID_CLOSETAB_BM] =
				ami_gui_make_tab_close_image(scrn, closetab_g);
		}
		if(g->shared->objects[GID_CLOSETAB_BM] == NULL) {
			g->shared->objects[GID_CLOSETAB_BM] = BitMapObj,
					BITMAP_SourceFile, closetab,
					BITMAP_SelectSourceFile, closetab_s,
					BITMAP_Screen, scrn,
					BITMAP_Masking, TRUE,
					BitMapEnd;
			g->shared->objects[GID_CLOSETAB_BM_G] = BitMapObj,
					BITMAP_SourceFile, closetab_g,
					BITMAP_Screen, scrn,
					BITMAP_Masking, TRUE,
					BitMapEnd;
		}

		g->shared->objects[GID_PAGEINFO_INSECURE_BM] = BitMapObj,
					BITMAP_SourceFile, pi_insecure,
					BITMAP_Screen, scrn,
					BITMAP_Masking, TRUE,
					BitMapEnd;

		g->shared->objects[GID_PAGEINFO_INTERNAL_BM] = BitMapObj,
					BITMAP_SourceFile, pi_internal,
					BITMAP_Screen, scrn,
					BITMAP_Masking, TRUE,
					BitMapEnd;

		g->shared->objects[GID_PAGEINFO_LOCAL_BM] = BitMapObj,
					BITMAP_SourceFile, pi_local,
					BITMAP_Screen, scrn,
					BITMAP_Masking, TRUE,
					BitMapEnd;

		g->shared->objects[GID_PAGEINFO_SECURE_BM] = BitMapObj,
					BITMAP_SourceFile, pi_secure,
					BITMAP_Screen, scrn,
					BITMAP_Masking, TRUE,
					BitMapEnd;

		g->shared->objects[GID_PAGEINFO_WARNING_BM] = BitMapObj,
					BITMAP_SourceFile, pi_warning,
					BITMAP_Screen, scrn,
					BITMAP_Masking, TRUE,
					BitMapEnd;

		/* Toolbar button images — owned separately (button.gadget does
		 * not dispose BUTTON_RenderImage). */
		if(g->shared->ui_nami) {
			char nav_west_h[100], nav_east_h[100];
			char stop_h[100], reload_h[100], home_h[100];

			/* Nami: normal/_s/_h/_g as distinct BitMapObjs */
			ami_gui_prefer_list_tbimage(nav_west_s);
			ami_gui_prefer_list_tbimage(nav_east_s);
			ami_gui_prefer_list_tbimage(stop_s);
			ami_gui_prefer_list_tbimage(reload_s);
			ami_gui_prefer_list_tbimage(home_s);
			ami_gui_path_aiss_suffix(nav_west_h, sizeof(nav_west_h),
					nav_west, "_h");
			ami_gui_path_aiss_suffix(nav_east_h, sizeof(nav_east_h),
					nav_east, "_h");
			ami_gui_path_aiss_suffix(stop_h, sizeof(stop_h), stop, "_h");
			ami_gui_path_aiss_suffix(reload_h, sizeof(reload_h),
					reload, "_h");
			ami_gui_path_aiss_suffix(home_h, sizeof(home_h), home, "_h");
			ami_gui_prefer_list_tbimage(nav_west_h);
			ami_gui_prefer_list_tbimage(nav_east_h);
			ami_gui_prefer_list_tbimage(stop_h);
			ami_gui_prefer_list_tbimage(reload_h);
			ami_gui_prefer_list_tbimage(home_h);

			g->shared->objects[GID_BACK_BM] = BitMapObj,
					BITMAP_SourceFile, nav_west,
					BITMAP_Screen, scrn,
					BITMAP_Masking, TRUE,
					BitMapEnd;
			g->shared->objects[GID_BACK_BM_S] = BitMapObj,
					BITMAP_SourceFile, nav_west_s,
					BITMAP_Screen, scrn,
					BITMAP_Masking, TRUE,
					BitMapEnd;
			g->shared->objects[GID_BACK_BM_H] = BitMapObj,
					BITMAP_SourceFile, nav_west_h,
					BITMAP_Screen, scrn,
					BITMAP_Masking, TRUE,
					BitMapEnd;
			g->shared->objects[GID_BACK_BM_G] = BitMapObj,
					BITMAP_SourceFile, nav_west_g,
					BITMAP_Screen, scrn,
					BITMAP_Masking, TRUE,
					BitMapEnd;
			g->shared->objects[GID_FORWARD_BM] = BitMapObj,
					BITMAP_SourceFile, nav_east,
					BITMAP_Screen, scrn,
					BITMAP_Masking, TRUE,
					BitMapEnd;
			g->shared->objects[GID_FORWARD_BM_S] = BitMapObj,
					BITMAP_SourceFile, nav_east_s,
					BITMAP_Screen, scrn,
					BITMAP_Masking, TRUE,
					BitMapEnd;
			g->shared->objects[GID_FORWARD_BM_H] = BitMapObj,
					BITMAP_SourceFile, nav_east_h,
					BITMAP_Screen, scrn,
					BITMAP_Masking, TRUE,
					BitMapEnd;
			g->shared->objects[GID_FORWARD_BM_G] = BitMapObj,
					BITMAP_SourceFile, nav_east_g,
					BITMAP_Screen, scrn,
					BITMAP_Masking, TRUE,
					BitMapEnd;
			g->shared->objects[GID_STOP_BM] = BitMapObj,
					BITMAP_SourceFile, stop,
					BITMAP_Screen, scrn,
					BITMAP_Masking, TRUE,
					BitMapEnd;
			g->shared->objects[GID_STOP_BM_S] = BitMapObj,
					BITMAP_SourceFile, stop_s,
					BITMAP_Screen, scrn,
					BITMAP_Masking, TRUE,
					BitMapEnd;
			g->shared->objects[GID_STOP_BM_H] = BitMapObj,
					BITMAP_SourceFile, stop_h,
					BITMAP_Screen, scrn,
					BITMAP_Masking, TRUE,
					BitMapEnd;
			g->shared->objects[GID_STOP_BM_G] = BitMapObj,
					BITMAP_SourceFile, stop_g,
					BITMAP_Screen, scrn,
					BITMAP_Masking, TRUE,
					BitMapEnd;
			g->shared->objects[GID_RELOAD_BM] = BitMapObj,
					BITMAP_SourceFile, reload,
					BITMAP_Screen, scrn,
					BITMAP_Masking, TRUE,
					BitMapEnd;
			g->shared->objects[GID_RELOAD_BM_S] = BitMapObj,
					BITMAP_SourceFile, reload_s,
					BITMAP_Screen, scrn,
					BITMAP_Masking, TRUE,
					BitMapEnd;
			g->shared->objects[GID_RELOAD_BM_H] = BitMapObj,
					BITMAP_SourceFile, reload_h,
					BITMAP_Screen, scrn,
					BITMAP_Masking, TRUE,
					BitMapEnd;
			g->shared->objects[GID_RELOAD_BM_G] = BitMapObj,
					BITMAP_SourceFile, reload_g,
					BITMAP_Screen, scrn,
					BITMAP_Masking, TRUE,
					BitMapEnd;
			g->shared->objects[GID_HOME_BM] = BitMapObj,
					BITMAP_SourceFile, home,
					BITMAP_Screen, scrn,
					BITMAP_Masking, TRUE,
					BitMapEnd;
			g->shared->objects[GID_HOME_BM_S] = BitMapObj,
					BITMAP_SourceFile, home_s,
					BITMAP_Screen, scrn,
					BITMAP_Masking, TRUE,
					BitMapEnd;
			g->shared->objects[GID_HOME_BM_H] = BitMapObj,
					BITMAP_SourceFile, home_h,
					BITMAP_Screen, scrn,
					BITMAP_Masking, TRUE,
					BitMapEnd;
			g->shared->objects[GID_HOME_BM_G] = BitMapObj,
					BITMAP_SourceFile, home_g,
					BITMAP_Screen, scrn,
					BITMAP_Masking, TRUE,
					BitMapEnd;
		} else {
			g->shared->objects[GID_BACK_BM] = BitMapObj,
					BITMAP_SourceFile, nav_west,
					BITMAP_SelectSourceFile, nav_west_s,
					BITMAP_Screen, scrn,
					BITMAP_Masking, TRUE,
					BitMapEnd;
			g->shared->objects[GID_BACK_BM_G] = BitMapObj,
					BITMAP_SourceFile, nav_west_g,
					BITMAP_Screen, scrn,
					BITMAP_Masking, TRUE,
					BitMapEnd;
			g->shared->objects[GID_FORWARD_BM] = BitMapObj,
					BITMAP_SourceFile, nav_east,
					BITMAP_SelectSourceFile, nav_east_s,
					BITMAP_Screen, scrn,
					BITMAP_Masking, TRUE,
					BitMapEnd;
			g->shared->objects[GID_FORWARD_BM_G] = BitMapObj,
					BITMAP_SourceFile, nav_east_g,
					BITMAP_Screen, scrn,
					BITMAP_Masking, TRUE,
					BitMapEnd;
			g->shared->objects[GID_STOP_BM] = BitMapObj,
					BITMAP_SourceFile, stop,
					BITMAP_SelectSourceFile, stop_s,
					BITMAP_Screen, scrn,
					BITMAP_Masking, TRUE,
					BitMapEnd;
			g->shared->objects[GID_STOP_BM_G] = BitMapObj,
					BITMAP_SourceFile, stop_g,
					BITMAP_Screen, scrn,
					BITMAP_Masking, TRUE,
					BitMapEnd;
			g->shared->objects[GID_RELOAD_BM] = BitMapObj,
					BITMAP_SourceFile, reload,
					BITMAP_SelectSourceFile, reload_s,
					BITMAP_Screen, scrn,
					BITMAP_Masking, TRUE,
					BitMapEnd;
			g->shared->objects[GID_RELOAD_BM_G] = BitMapObj,
					BITMAP_SourceFile, reload_g,
					BITMAP_Screen, scrn,
					BITMAP_Masking, TRUE,
					BitMapEnd;
			g->shared->objects[GID_HOME_BM] = BitMapObj,
					BITMAP_SourceFile, home,
					BITMAP_SelectSourceFile, home_s,
					BITMAP_Screen, scrn,
					BITMAP_Masking, TRUE,
					BitMapEnd;
			g->shared->objects[GID_HOME_BM_G] = BitMapObj,
					BITMAP_SourceFile, home_g,
					BITMAP_Screen, scrn,
					BITMAP_Masking, TRUE,
					BitMapEnd;
		}


		/* add a new tab tab — NetSurf ClickTab "+" only; Nami uses SIDE_NEWTAB */
		if(g->shared->ui_nami == false) {
			g->shared->new_tab_tab = AllocClickTabNode(
					TNA_Text, "+",
					TNA_HintInfo, g->shared->helphints[GID_ADDTAB_HINT],
					TAG_DONE);
			AddTail(&g->shared->tab_list, g->shared->new_tab_tab);

			if(ClickTabBase->lib_Version < 53)
			{
				/* Tabs stay in the layout on clicktab < 53 (no auto show/hide). */
				add_tabs_gadget = LAYOUT_AddChild;

				if(ami_clicktab_has_close()) {
					/* OS3.2 V47.5+: embedded per-tab close, no standalone button */
					g->shared->objects[GID_TABS] = ClickTabObj,
							GA_ID, GID_TABS,
							GA_RelVerify, TRUE,
							GA_Underscore, 13, /* disable kb shortcuts */
							ICA_TARGET, ICTARGET_IDCMP,
							CLICKTAB_Labels, &g->shared->tab_list,
							CLICKTAB_LabelTruncate, TRUE,
							CLICKTAB_AutoFit, TRUE,
							CLICKTAB_CloseImage, g->shared->objects[GID_CLOSETAB_BM],
							CLICKTAB_ClosePlacement, PLACECLOSE_RIGHT,
							ClickTabEnd;
				} else {
					add_closetab_gadget = LAYOUT_AddChild;
					closetab_weight_w = CHILD_WeightedWidth;
					closetab_weight_h = CHILD_WeightedHeight;

					g->shared->objects[GID_CLOSETAB] = ButtonObj,
							GA_ID, GID_CLOSETAB,
							GA_RelVerify, TRUE,
							BUTTON_RenderImage, g->shared->objects[GID_CLOSETAB_BM],
							ButtonEnd;

					g->shared->objects[GID_TABS] = ClickTabObj,
							GA_ID, GID_TABS,
							GA_RelVerify, TRUE,
							GA_Underscore, 13, /* disable kb shortcuts */
							CLICKTAB_Labels, &g->shared->tab_list,
							CLICKTAB_LabelTruncate, TRUE,
							CLICKTAB_AutoFit, TRUE,
							ClickTabEnd;
				}
			}
			else
			{
				g->shared->objects[GID_TABS_FLAG] = BitMapObj,
						BITMAP_SourceFile, tabthrobber,
						BITMAP_Screen,scrn,
						BITMAP_Masking,TRUE,
						BitMapEnd;
			}
		} else {
			g->shared->new_tab_tab = NULL;
		}

		/* Network LED + boingball spinner (falls back to theme filmstrip).
		 * boingball.image autodoc omits it, but PENMAP_Palette + MaskBlit
		 * are required or the class renders a solid black silhouette. */
		{
			struct DrawInfo *tdri;
			static ULONG boingball_palette[] = {
				15,
				0xffffffffUL, 0x00000000UL, 0x00000000UL,
				0xffffffffUL, 0x00000000UL, 0x00000000UL,
				0xffffffffUL, 0x00000000UL, 0x00000000UL,
				0xffffffffUL, 0x00000000UL, 0x00000000UL,
				0xffffffffUL, 0x00000000UL, 0x00000000UL,
				0xffffffffUL, 0x00000000UL, 0x00000000UL,
				0xffffffffUL, 0x00000000UL, 0x00000000UL,
				0xffffffffUL, 0xffffffffUL, 0xffffffffUL,
				0xffffffffUL, 0xffffffffUL, 0xffffffffUL,
				0xffffffffUL, 0xffffffffUL, 0xffffffffUL,
				0xffffffffUL, 0xffffffffUL, 0xffffffffUL,
				0xffffffffUL, 0xffffffffUL, 0xffffffffUL,
				0xffffffffUL, 0xffffffffUL, 0xffffffffUL,
				0xffffffffUL, 0xffffffffUL, 0xffffffffUL,
				0xffffffffUL, 0xffffffffUL, 0xffffffffUL
			};

			g->shared->throbber_led = NULL;
			g->shared->throbber_boing = NULL;
			g->shared->throbber_led_vals[0] = 0;
			tdri = GetScreenDrawInfo(scrn);
			/* Nami: boingball only. NetSurf: LED lamps + boingball. */
			if(tdri != NULL && g->shared->ui_nami == false) {
				/* Compact activity lamps (raw segments), not a clock readout */
				g->shared->throbber_led = NewObject(NULL, "led.image",
						SYSIA_DrawInfo, tdri,
						IA_FGPen, -1,
						IA_BGPen, -1,
						IA_Width, 14,
						IA_Height, 24,
						LED_Pairs, 1,
						LED_Time, FALSE,
						LED_Signed, FALSE,
						LED_Raw, TRUE,
						LED_Values, g->shared->throbber_led_vals,
						TAG_DONE);
			}
			if(tdri != NULL)
				FreeScreenDrawInfo(scrn, tdri);
			g->shared->throbber_boing = NewObject(NULL, "boingball.image",
					PENMAP_Palette, boingball_palette,
					PENMAP_Screen, scrn,
					PENMAP_Transparent, TRUE,
					PENMAP_MaskBlit, TRUE,
					TAG_DONE);
			if(g->shared->ui_nami) {
				/* Boing alone is enough */
				if(g->shared->throbber_boing == NULL) {
					/* fall through to theme filmstrip */
				}
			} else if(g->shared->throbber_led == NULL ||
			   g->shared->throbber_boing == NULL) {
				if(g->shared->throbber_led != NULL) {
					DisposeObject(g->shared->throbber_led);
					g->shared->throbber_led = NULL;
				}
				if(g->shared->throbber_boing != NULL) {
					DisposeObject(g->shared->throbber_boing);
					g->shared->throbber_boing = NULL;
				}
			}
		}

		if(g->shared->throbber_boing != NULL &&
		   g->shared->throbber_led != NULL) {
			throbber_w = 14 + 4 + 24;
			throbber_h = 24;
		} else if(g->shared->throbber_boing != NULL) {
			throbber_w = 24;
			throbber_h = 24;
		} else {
			throbber_w = (ULONG)ami_theme_throbber_get_width();
			throbber_h = (ULONG)ami_theme_throbber_get_height();
		}

		/*
		 * Nami chrome: no system title gadgets; close/tabs/zoom/depth row
		 * above the toolbar.  ClickTab always lives in this row.
		 */
		{
			ULONG wa_depth_gad = TRUE;
			ULONG wa_drag_gad = TRUE;
			ULONG wa_close_gad = TRUE;
			ULONG wa_builtin_scroll = TRUE;
			ULONG wa_size_bright = FALSE;
			ULONG space_outer = TRUE;
			ULONG inner_sp_tag = TAG_IGNORE;
			ULONG inner_sp_val = 0;
			ULONG btn_bevel_tag = TAG_IGNORE;
			ULONG btn_bevel_val = 0;
			ULONG chrome_add = TAG_IGNORE;
			ULONG chrome_wh = TAG_IGNORE;
			ULONG tl_add = LAYOUT_AddChild;
			ULONG tl_wh = CHILD_WeightedHeight;
			ULONG home_add = LAYOUT_AddChild;
			ULONG home_ww = CHILD_WeightedWidth;
			ULONG home_wh = CHILD_WeightedHeight;
			ULONG fave_add = LAYOUT_AddChild;
			ULONG fave_ww = CHILD_WeightedWidth;
			ULONG fave_wh = CHILD_WeightedHeight;
			/* Separate Stop button; Nami folds Stop into Reload slot */
			ULONG stop_add = LAYOUT_AddChild;
			ULONG stop_ww = CHILD_WeightedWidth;
			ULONG stop_wh = CHILD_WeightedHeight;
			Object *stop_btn = NULL;
			/* Nami: omni URL only — no search string or provider chooser */
			ULONG weight_bar_tag = LAYOUT_WeightBar;
			ULONG weight_bar_val = TRUE;
			ULONG search_row_add = LAYOUT_AddChild;
			ULONG search_row_ww_tag = CHILD_WeightedWidth;
			ULONG search_row_ww = (ULONG)nsoption_int(web_search_width);
			Object *search_chooser = NULL;
			Object *search_string = NULL;
			Object *search_row = NULL;
			/* Nami: padded hit targets around 16px nav art */
			ULONG nav_mw_tag = TAG_IGNORE;
			ULONG nav_mw = 0;
			ULONG nav_mh_tag = TAG_IGNORE;
			ULONG nav_mh = 0;
			ULONG tb_inner_tag = TAG_IGNORE;
			ULONG tb_inner = 0;
#ifndef __amigaos4__
			/* OS3 status string at bottom — omitted in Nami (screen title) */
			ULONG status_add = LAYOUT_AddChild;
			Object *status_gad = NULL;
#endif
			Object *chrome_obj = NULL;
			Object *tl_obj = NULL;
			Object *home_btn = NULL;
			Object *fave_btn = NULL;
			Object *back_init_bm;
			Object *fwd_init_bm;
			/* Favicon slot: Nami makes it the hotlist control */
			ULONG icon_relverify = FALSE;
			ULONG icon_readonly = TRUE;
			ULONG icon_hint_tag = TAG_IGNORE;
			STRPTR icon_hint = NULL;
			/* Nami: toolbar embeds in chrome; body is sidebar|browser */
			ULONG toolbar_row_add = LAYOUT_AddChild;
			Object *toolbar_obj = NULL;
			Object *body_obj = NULL;
			Object *browser_col = NULL;
			/* Nami: throbber sits in chrome after the drag strip */
			ULONG throb_in_tb = LAYOUT_AddChild;
			Object *throb_gad = NULL;

			/* AISS _g art for initially unavailable history buttons */
			back_init_bm = g->shared->objects[GID_BACK_BM_G];
			if(back_init_bm == NULL)
				back_init_bm = g->shared->objects[GID_BACK_BM];
			fwd_init_bm = g->shared->objects[GID_FORWARD_BM_G];
			if(fwd_init_bm == NULL)
				fwd_init_bm = g->shared->objects[GID_FORWARD_BM];

			/* OS3 has no WA_ZoomGadget; zoom appears only with Size+Depth.
			 * Nami clears Depth so the system zoom gadget stays off. */
			if(g->shared->ui_nami) {
				wa_depth_gad = FALSE;
				wa_drag_gad = FALSE;
				wa_close_gad = FALSE;
				/* Border HTML scrollers; do not let window.class scroll the layout */
				wa_builtin_scroll = FALSE;
				/* SizeBRight + SizeBBottom so RelRight/RelBottom scrollers fit */
				wa_size_bright = TRUE;
				/* Flush chrome to the window inner edges (no outer pad). */
				space_outer = FALSE;
				inner_sp_tag = LAYOUT_InnerSpacing;
				inner_sp_val = 0;
				/* Toolbar image buttons: no raised bevel. Hover/press
				 * use GA_Selected fill + (_s on press) via ami_gui_tb_*. */
				btn_bevel_tag = BUTTON_BevelStyle;
				btn_bevel_val = BVS_NONE;
				/* 16px art with room around each nav button */
				nav_mw_tag = CHILD_MinWidth;
				nav_mw = 24;
				nav_mh_tag = CHILD_MinHeight;
				nav_mh = 24;
				tb_inner_tag = LAYOUT_InnerSpacing;
				tb_inner = 2;
				/* Nami toolbar omits Home, hotlist star, and separate Stop */
				home_add = TAG_IGNORE;
				home_ww = TAG_IGNORE;
				home_wh = TAG_IGNORE;
				fave_add = TAG_IGNORE;
				fave_ww = TAG_IGNORE;
				fave_wh = TAG_IGNORE;
				stop_add = TAG_IGNORE;
				stop_ww = TAG_IGNORE;
				stop_wh = TAG_IGNORE;
				/* Favicon space doubles as +hotlist / remove-hotlist */
				icon_relverify = TRUE;
				icon_readonly = FALSE;
				icon_hint_tag = AMI_GA_HELP;
				icon_hint = g->shared->helphints[GID_FAVE_ADD];
				/* Omni bar only — no search string, no provider chooser */
				weight_bar_tag = TAG_IGNORE;
				weight_bar_val = 0;
				search_row_add = TAG_IGNORE;
				search_row_ww_tag = TAG_IGNORE;
				search_row_ww = 0;
#ifndef __amigaos4__
				status_add = TAG_IGNORE;
#endif
				/* Throbber moves out of the toolbar into the chrome row */
				throb_in_tb = TAG_IGNORE;

				/* Standalone tab-close is NetSurf-layout only. */
				if(g->shared->objects[GID_CLOSETAB] != NULL) {
					DisposeObject(g->shared->objects[GID_CLOSETAB]);
					g->shared->objects[GID_CLOSETAB] = NULL;
				}

#if 0
				/*
				 * OLD Nami chrome: ClickTab strip between close and zoom,
				 * sysiclass SpaceObj hooks for window gadgets.  Kept for
				 * reference; sidebar + AISS ButtonObjs replace this path.
				 */
				if(g->shared->objects[GID_TABS] == NULL) {
					if(ami_clicktab_has_close()) {
						g->shared->objects[GID_TABS] = ClickTabObj,
								GA_ID, GID_TABS,
								GA_RelVerify, TRUE,
								GA_Underscore, 13,
								GA_ContextMenu, ami_ctxmenu_clicktab_create(g->shared, &g->shared->clicktab_ctxmenu),
								ICA_TARGET, ICTARGET_IDCMP,
								CLICKTAB_Labels, &g->shared->tab_list,
								CLICKTAB_LabelTruncate, TRUE,
								CLICKTAB_AutoFit, TRUE,
								CLICKTAB_CloseImage, g->shared->objects[GID_CLOSETAB_BM],
								CLICKTAB_ClosePlacement, PLACECLOSE_RIGHT,
								CLICKTAB_FlagImage, g->shared->objects[GID_TABS_FLAG],
								ClickTabEnd;
					} else {
						g->shared->objects[GID_TABS] = ClickTabObj,
								GA_ID, GID_TABS,
								GA_RelVerify, TRUE,
								GA_Underscore, 13,
								CLICKTAB_Labels, &g->shared->tab_list,
								CLICKTAB_LabelTruncate, TRUE,
								CLICKTAB_AutoFit, TRUE,
								CLICKTAB_FlagImage, g->shared->objects[GID_TABS_FLAG],
								ClickTabEnd;
					}
				}
				g->shared->objects[GID_WIN_CLOSE_BM] =
					ami_gui_make_chrome_sysi_image(scrn, CLOSEIMAGE);
				g->shared->objects[GID_WIN_ZOOM_BM] =
					ami_gui_make_chrome_sysi_image(scrn, ZOOMIMAGE);
				g->shared->objects[GID_WIN_DEPTH_BM] =
					ami_gui_make_chrome_sysi_image(scrn, DEPTHIMAGE);
#endif

				/* Close uses cancel glyph (same as closetab) */
				g->shared->objects[GID_WIN_CLOSE_BM] =
					ami_gui_theme_bitmap(scrn, "theme_closetab",
							"theme_side_closetab");
				g->shared->objects[GID_WIN_ZOOM_BM] =
					ami_gui_make_chrome_sysi_image(scrn, ZOOMIMAGE);
				g->shared->objects[GID_WIN_DEPTH_BM] =
					ami_gui_make_chrome_sysi_image(scrn, DEPTHIMAGE);
				g->shared->objects[GID_SIDE_NEWTAB_BM] =
					ami_gui_theme_bitmap(scrn, "theme_side_newtab", "theme_addtab");
				g->shared->objects[GID_SIDE_CLOSETAB_BM] =
					ami_gui_theme_bitmap(scrn, "theme_side_closetab", "theme_closetab");
				/* Sidebar show/hide — AISS toolbar (fallback: selecttoggle/toggle) */
				g->shared->objects[GID_SIDE_TOGGLE_BM] =
					ami_gui_theme_bitmap(scrn, "theme_side_toggle", NULL);
				g->shared->sidebar_def_icon =
					ami_gui_theme_bitmap(scrn, "theme_tab_loading", "theme_pageinfo_internal");

				/* New tab + close current — no standing bevel frame */
				if(g->shared->objects[GID_SIDE_NEWTAB_BM] != NULL) {
					g->shared->objects[GID_SIDE_NEWTAB] = ButtonObj,
							GA_ID, GID_SIDE_NEWTAB,
							GA_RelVerify, TRUE,
							AMI_GA_HELP, g->shared->helphints[GID_ADDTAB_HINT],
							BUTTON_BevelStyle, BVS_NONE,
							BUTTON_Transparent, TRUE,
							BUTTON_RenderImage, g->shared->objects[GID_SIDE_NEWTAB_BM],
						ButtonEnd;
				} else {
					g->shared->objects[GID_SIDE_NEWTAB] = ButtonObj,
							GA_ID, GID_SIDE_NEWTAB,
							GA_RelVerify, TRUE,
							GA_Text, "+",
							AMI_GA_HELP, g->shared->helphints[GID_ADDTAB_HINT],
							BUTTON_BevelStyle, BVS_NONE,
							BUTTON_Transparent, TRUE,
						ButtonEnd;
				}
				if(g->shared->objects[GID_SIDE_CLOSETAB_BM] != NULL) {
					g->shared->objects[GID_SIDE_CLOSETAB] = ButtonObj,
							GA_ID, GID_SIDE_CLOSETAB,
							GA_RelVerify, TRUE,
							AMI_GA_HELP, g->shared->helphints[GID_CLOSETAB],
							BUTTON_BevelStyle, BVS_NONE,
							BUTTON_Transparent, TRUE,
							BUTTON_RenderImage, g->shared->objects[GID_SIDE_CLOSETAB_BM],
						ButtonEnd;
				} else {
					g->shared->objects[GID_SIDE_CLOSETAB] = ButtonObj,
							GA_ID, GID_SIDE_CLOSETAB,
							GA_RelVerify, TRUE,
							GA_Text, "X",
							AMI_GA_HELP, g->shared->helphints[GID_CLOSETAB],
							BUTTON_BevelStyle, BVS_NONE,
							BUTTON_Transparent, TRUE,
						ButtonEnd;
				}

				/*
				 * Favicon shelf: LayoutV of LayoutH rows so icons wrap
				 * inside the sidebar instead of spilling into the browser.
				 */
				g->shared->objects[GID_SIDE_HOTLAYOUT] = LayoutVObj,
						LAYOUT_SpaceInner, TRUE,
						LAYOUT_SpaceOuter, TRUE,
						LAYOUT_InnerSpacing, 2,
						LAYOUT_VertAlignment, LALIGN_TOP,
						LAYOUT_HorizAlignment, LALIGN_LEFT,
					LayoutEnd;

				{
					int hi;
					int ri;
					int ci;

					for(ri = 0; ri < AMI_SIDE_HOT_ROWS; ri++) {
						g->shared->side_hot_row[ri] = LayoutHObj,
								LAYOUT_SpaceInner, TRUE,
								LAYOUT_SpaceOuter, FALSE,
								LAYOUT_InnerSpacing, 4,
								LAYOUT_HorizAlignment, LALIGN_LEFT,
							LayoutEnd;
						SetAttrs(g->shared->objects[GID_SIDE_HOTLAYOUT],
								LAYOUT_AddChild, g->shared->side_hot_row[ri],
								CHILD_WeightedWidth, 100,
								CHILD_WeightedHeight, 0,
								CHILD_MinWidth, 1,
								CHILD_MinHeight, 0,
								TAG_DONE);
					}

					for(hi = 0; hi < AMI_SIDE_HOTLIST_MAX; hi++) {
						ri = hi / AMI_SIDE_HOT_COLS;
						ci = hi % AMI_SIDE_HOT_COLS;
						(void)ci;
						g->shared->side_hot_bm[hi] = NULL;
						g->shared->side_hot_url[hi] = NULL;
						g->shared->side_hot_btn[hi] = ButtonObj,
								GA_ID, GID_SIDE_HOT_BASE + hi,
								GA_RelVerify, TRUE,
								GA_Hidden, TRUE,
								BUTTON_BevelStyle, BVS_NONE,
								BUTTON_Transparent, FALSE,
							ButtonEnd;
						SetAttrs(g->shared->side_hot_row[ri],
								LAYOUT_AddChild, g->shared->side_hot_btn[hi],
								CHILD_WeightedWidth, 0,
								CHILD_WeightedHeight, 0,
								CHILD_MinWidth, 0,
								CHILD_MaxWidth, 0,
								CHILD_MinHeight, 0,
								CHILD_MaxHeight, 0,
								TAG_DONE);
					}
					g->shared->side_hot_count = 0;
				}

				/*
				 * Fixed pool of tab rows: favicon cell | title button.
				 * Title uses GA_Text + BCJ_LEFT (button.gadget centres images).
				 */
				{
					int ti;

					g->shared->objects[GID_SIDE_TABLIST] = LayoutVObj,
							LAYOUT_SpaceInner, TRUE,
							LAYOUT_SpaceOuter, FALSE,
							LAYOUT_VertAlignment, LALIGN_TOP,
							LAYOUT_HorizAlignment, LALIGN_LEFT,
						LayoutEnd;

					for(ti = 0; ti < AMI_SIDE_TAB_MAX; ti++) {
						g->shared->side_tab_gw[ti] = NULL;
						g->shared->side_tab_icon_btn[ti] = ButtonObj,
								GA_ID, GID_SIDE_TAB_ICON_BASE + ti,
								GA_RelVerify, TRUE,
								GA_Immediate, TRUE,
								GA_Hidden, TRUE,
								BUTTON_PushButton, FALSE,
								BUTTON_BevelStyle, BVS_NONE,
								BUTTON_Transparent, TRUE,
							ButtonEnd;
						g->shared->side_tab_btn[ti] = ButtonObj,
								GA_ID, GID_SIDE_TAB_BASE + ti,
								GA_RelVerify, TRUE,
								GA_Immediate, TRUE,
								GA_Hidden, TRUE,
								GA_Text, (STRPTR)"",
								BUTTON_PushButton, FALSE,
								BUTTON_BevelStyle, BVS_NONE,
								BUTTON_Transparent, FALSE,
								BUTTON_Justification, BCJ_LEFT,
							ButtonEnd;
						g->shared->side_tab_row[ti] = LayoutHObj,
								LAYOUT_SpaceInner, FALSE,
								LAYOUT_SpaceOuter, FALSE,
								LAYOUT_HorizAlignment, LALIGN_LEFT,
								LAYOUT_AddChild, g->shared->side_tab_icon_btn[ti],
								CHILD_WeightedWidth, 0,
								CHILD_MinWidth, 20,
								CHILD_MaxWidth, 20,
								CHILD_MinHeight, 0,
								CHILD_MaxHeight, 0,
								LAYOUT_AddChild, g->shared->side_tab_btn[ti],
								CHILD_WeightedWidth, 100,
								CHILD_MinWidth, 1,
								CHILD_MinHeight, 0,
								CHILD_MaxHeight, 0,
							LayoutEnd;
						SetAttrs(g->shared->objects[GID_SIDE_TABLIST],
								LAYOUT_AddChild, g->shared->side_tab_row[ti],
								CHILD_WeightedWidth, 100,
								CHILD_WeightedHeight, 0,
								CHILD_MinWidth, 1,
								CHILD_MinHeight, 0,
								CHILD_MaxHeight, 0,
								TAG_DONE);
					}
				}

				/*
				 * Top-aligned LayoutV in BODY (no virtual.gadget).
				 *
				 * Match NDK Examples/Backfill: bevel + LAYOUT_BackFill only.
				 * Do not set LAYOUT_FillPen — a BACKGROUNDPEN solid matches the
				 * window and skips EraseRect, so the BackFill hook never runs.
				 */
				{
					ami_nami_sidebar_bf_hook.h_Entry =
							(void *)ami_gui_nami_sidebar_backfill;
					ami_nami_sidebar_bf_hook.h_SubEntry = NULL;
					ami_nami_sidebar_bf_hook.h_Data = NULL;

					g->shared->objects[GID_SIDELAYOUT] = LayoutVObj,
							LAYOUT_SpaceInner, TRUE,
							LAYOUT_SpaceOuter, TRUE,
							LAYOUT_VertAlignment, LALIGN_TOP,
							LAYOUT_HorizAlignment, LALIGN_LEFT,
							LAYOUT_BevelStyle, BVS_GROUP,
							LAYOUT_BackFill, &ami_nami_sidebar_bf_hook,
							LAYOUT_AddChild, LayoutHObj,
								LAYOUT_SpaceInner, TRUE,
								LAYOUT_AddChild, g->shared->objects[GID_SIDE_NEWTAB],
								CHILD_WeightedWidth, 50,
								CHILD_MinHeight, 22,
								LAYOUT_AddChild, g->shared->objects[GID_SIDE_CLOSETAB],
								CHILD_WeightedWidth, 50,
								CHILD_MinHeight, 22,
							LayoutEnd,
							CHILD_WeightedWidth, 100,
							CHILD_WeightedHeight, 0,
							LAYOUT_AddChild, g->shared->objects[GID_SIDE_HOTLAYOUT],
							CHILD_WeightedWidth, 100,
							CHILD_WeightedHeight, 0,
							CHILD_MinHeight, 0,
							CHILD_MaxHeight, 0,
							CHILD_CacheDomain, FALSE,
							LAYOUT_AddChild, g->shared->objects[GID_SIDE_TABLIST],
							CHILD_WeightedWidth, 100,
							CHILD_WeightedHeight, 0,
							CHILD_CacheDomain, FALSE,
							LAYOUT_AddChild, SpaceObj,
								SPACE_MinWidth, 1,
								SPACE_MinHeight, 1,
								SPACE_Transparent, TRUE,
							SpaceEnd,
							CHILD_WeightedWidth, 100,
							CHILD_WeightedHeight, 100,
						LayoutEnd;
				}
				g->shared->objects[GID_SIDE_VIRTUAL] = NULL;

				/*
				 * Legacy collapsed grab strip — kept allocated but always
				 * zero-sized; chrome toolbar toggle fully shows/hides sidebar.
				 */
				g->shared->objects[GID_SIDE_STRIP] = SpaceObj,
						GA_ID, GID_SIDE_STRIP,
						SPACE_MinWidth, 1,
						SPACE_MinHeight, 1,
						SPACE_Transparent, TRUE,
					SpaceEnd;

				/* Sidebar toggle: AISS toolbar glyph (text if image missing) */
				if(g->shared->objects[GID_SIDE_TOGGLE_BM] != NULL) {
					g->shared->objects[GID_SIDE_TOGGLE] = ButtonObj,
							GA_ID, GID_SIDE_TOGGLE,
							GA_RelVerify, TRUE,
							AMI_GA_HELP, g->shared->helphints[GID_SIDE_TOGGLE],
							BUTTON_BevelStyle, BVS_NONE,
							BUTTON_Transparent, FALSE,
							BUTTON_RenderImage, g->shared->objects[GID_SIDE_TOGGLE_BM],
						ButtonEnd;
				} else {
					g->shared->objects[GID_SIDE_TOGGLE] = ButtonObj,
							GA_ID, GID_SIDE_TOGGLE,
							GA_RelVerify, TRUE,
							GA_Text, "=",
							AMI_GA_HELP, g->shared->helphints[GID_SIDE_TOGGLE],
							BUTTON_BevelStyle, BVS_NONE,
							BUTTON_Transparent, FALSE,
						ButtonEnd;
				}

				g->shared->objects[GID_WIN_CLOSE] = ButtonObj,
						GA_ID, GID_WIN_CLOSE,
						GA_RelVerify, TRUE,
						BUTTON_BevelStyle, BVS_NONE,
						BUTTON_Transparent, FALSE,
						BUTTON_RenderImage, g->shared->objects[GID_WIN_CLOSE_BM],
					ButtonEnd;
				/*
				 * Zoom and depth are not layout children.  They are
				 * mounted on the window's top-right corner once the
				 * borders are known (ami_gui_nami_create_border_scrollers).
				 */
				g->shared->objects[GID_WIN_ZOOM] = NULL;
				g->shared->objects[GID_WIN_DEPTH] = NULL;

				/*
				 * Drag strip: modest fixed width, transparent (no FILLPEN
				 * backfill).  Hit-test still works via SpaceObj bounds.
				 */
				g->shared->objects[GID_WIN_DRAG] = SpaceObj,
						GA_ID, GID_WIN_DRAG,
						SPACE_MinWidth, 40,
						SPACE_MinHeight, 16,
						SPACE_Transparent, TRUE,
					SpaceEnd;
				g->shared->objects[GID_CHROME_RPAD] = SpaceObj,
						SPACE_MinWidth, 18,
						SPACE_MinHeight, 16,
						SPACE_Transparent, TRUE,
					SpaceEnd;

				/* Placeholder chrome — rebuilt once toolbar_obj exists */
				g->shared->objects[GID_CHROMELAYOUT] = NULL;
				g->shared->objects[GID_TABLAYOUT] = NULL;
				chrome_obj = NULL;
				chrome_add = LAYOUT_AddChild;
				chrome_wh = CHILD_WeightedHeight;
				tl_add = TAG_IGNORE;
				tl_wh = TAG_IGNORE;
				tl_obj = NULL;
				add_tabs_gadget = TAG_IGNORE;
				add_closetab_gadget = TAG_IGNORE;
				toolbar_row_add = TAG_IGNORE; /* embedded in chrome */
			} else {
				g->shared->objects[GID_TABLAYOUT] = LayoutHObj,
					LAYOUT_SpaceInner,FALSE,
					add_closetab_gadget, g->shared->objects[GID_CLOSETAB],
					closetab_weight_w, 0,
					closetab_weight_h, 0,
					add_tabs_gadget, g->shared->objects[GID_TABS],
					CHILD_CacheDomain,FALSE,
				LayoutEnd;
				tl_obj = g->shared->objects[GID_TABLAYOUT];
			}

			/* Pre-build optional toolbar buttons so Nami can omit them. */
			if(home_add == LAYOUT_AddChild) {
				home_btn = ButtonObj,
					GA_ID, GID_HOME,
					GA_RelVerify, TRUE,
					AMI_GA_HELP, g->shared->helphints[GID_HOME],
					btn_bevel_tag, btn_bevel_val,
					BUTTON_RenderImage, g->shared->objects[GID_HOME_BM],
				ButtonEnd;
				g->shared->objects[GID_HOME] = home_btn;
			}
			if(fave_add == LAYOUT_AddChild) {
				fave_btn = ButtonObj,
					GA_ID, GID_FAVE,
					GA_RelVerify, TRUE,
					btn_bevel_tag, btn_bevel_val,
					BUTTON_RenderImage, g->shared->objects[GID_FAVE_ADD],
				ButtonEnd;
				g->shared->objects[GID_FAVE] = fave_btn;
			}
			if(stop_add == LAYOUT_AddChild) {
				stop_btn = ButtonObj,
					GA_ID, GID_STOP,
					GA_RelVerify, TRUE,
					AMI_GA_HELP, g->shared->helphints[GID_STOP],
					btn_bevel_tag, btn_bevel_val,
					BUTTON_RenderImage, g->shared->objects[GID_STOP_BM],
				ButtonEnd;
				g->shared->objects[GID_STOP] = stop_btn;
			}
#ifndef __amigaos4__
			/* Classic OS3 status strip; Nami uses the Intuition screen title */
			if(status_add == LAYOUT_AddChild) {
				status_gad = StringObj,
					GA_ID, GID_STATUS,
					GA_ReadOnly, TRUE,
					STRINGA_TextVal, NULL,
					GA_RelVerify, TRUE,
				StringEnd;
				g->shared->objects[GID_STATUS] = status_gad;
			}
#endif

			/* Search provider chooser + string — NetSurf only.
			 * Nami omni bar uses Prefs search provider with no chooser UI. */
			if(search_row_add == LAYOUT_AddChild) {
				search_chooser = ChooserObj,
					GA_ID, GID_SEARCH_ICON,
					GA_RelVerify, TRUE,
					CHOOSER_DropDown, TRUE,
					CHOOSER_Labels, g->shared->web_search_list,
					CHOOSER_MaxLabels, 40,
				ChooserEnd;
				g->shared->objects[GID_SEARCH_ICON] = search_chooser;

				search_string = StringObj,
					GA_ID, GID_SEARCHSTRING,
					STRINGA_TextVal, NULL,
					GA_RelVerify, TRUE,
					AMI_GA_HELP, g->shared->helphints[GID_SEARCHSTRING],
				StringEnd;
				g->shared->objects[GID_SEARCHSTRING] = search_string;
				search_row = LayoutHObj,
					LAYOUT_VertAlignment, LALIGN_CENTER,
					LAYOUT_AddChild, search_chooser,
					CHILD_WeightedWidth, 0,
					CHILD_WeightedHeight, 0,
					LAYOUT_AddChild, search_string,
				LayoutEnd;
			} else {
				g->shared->objects[GID_SEARCH_ICON] = NULL;
				g->shared->objects[GID_SEARCHSTRING] = NULL;
			}

		NSLOG(netsurf, INFO, "Creating window object");

			/* Toolbar row — NetSurf: main V child; Nami: embedded in chrome */
			g->shared->objects[GID_TOOLBARLAYOUT] = LayoutHObj,
					LAYOUT_VertAlignment, LALIGN_CENTER,
					tb_inner_tag, tb_inner,
					LAYOUT_AddChild, g->shared->objects[GID_BACK] = ButtonObj,
						GA_ID, GID_BACK,
						GA_RelVerify, TRUE,
						GA_ReadOnly, TRUE,
						GA_ContextMenu, ami_ctxmenu_history_create(AMI_CTXMENU_HISTORY_BACK, g->shared),
						AMI_GA_HELP, g->shared->helphints[GID_BACK],
						btn_bevel_tag, btn_bevel_val,
						BUTTON_RenderImage, back_init_bm,
					ButtonEnd,
					CHILD_WeightedWidth,0,
					CHILD_WeightedHeight,0,
					nav_mw_tag, nav_mw,
					nav_mh_tag, nav_mh,
					LAYOUT_AddChild, g->shared->objects[GID_FORWARD] = ButtonObj,
						GA_ID, GID_FORWARD,
						GA_RelVerify, TRUE,
						GA_ReadOnly, TRUE,
						GA_ContextMenu, ami_ctxmenu_history_create(AMI_CTXMENU_HISTORY_FORWARD, g->shared),
						AMI_GA_HELP, g->shared->helphints[GID_FORWARD],
						btn_bevel_tag, btn_bevel_val,
						BUTTON_RenderImage, fwd_init_bm,
					ButtonEnd,
					CHILD_WeightedWidth,0,
					CHILD_WeightedHeight,0,
					nav_mw_tag, nav_mw,
					nav_mh_tag, nav_mh,
					stop_add, stop_btn,
					stop_ww, 0,
					stop_wh, 0,
					LAYOUT_AddChild, g->shared->objects[GID_RELOAD] = ButtonObj,
						GA_ID,GID_RELOAD,
						GA_RelVerify,TRUE,
						AMI_GA_HELP, g->shared->helphints[GID_RELOAD],
						btn_bevel_tag, btn_bevel_val,
						BUTTON_RenderImage, g->shared->objects[GID_RELOAD_BM],
					ButtonEnd,
					CHILD_WeightedWidth,0,
					CHILD_WeightedHeight,0,
					nav_mw_tag, nav_mw,
					nav_mh_tag, nav_mh,
					home_add, home_btn,
					home_ww, 0,
					home_wh, 0,
					/* Gap: nav buttons → URL / page-info cluster */
					LAYOUT_AddChild, SpaceObj,
						SPACE_MinWidth, AMI_TB_GROUP_GAP,
						SPACE_MinHeight, 1,
						SPACE_Transparent, TRUE,
						GA_ReadOnly, TRUE,
					SpaceEnd,
					CHILD_WeightedWidth, 0,
					CHILD_WeightedHeight, 0,
					LAYOUT_AddChild, LayoutHObj, /* FavIcon, URL, padlock [, star] */
						LAYOUT_VertAlignment, LALIGN_CENTER,
						LAYOUT_AddChild, g->shared->objects[GID_ICON] = SpaceObj,
							GA_ID, GID_ICON,
							SPACE_MinWidth, 16,
							SPACE_MinHeight, 16,
							SPACE_Transparent, TRUE,
							SPACE_RenderHook, &g->shared->favicon_hook,
							GA_RelVerify, icon_relverify,
							GA_ReadOnly, icon_readonly,
							icon_hint_tag, icon_hint,
						SpaceEnd,
						CHILD_WeightedWidth, 0,
						CHILD_WeightedHeight, 0,
						LAYOUT_AddChild, g->shared->objects[GID_URL] =
#ifdef __amigaos4__
							NewObject(urlStringClass, NULL,
#else
							StringObj,
#endif
									STRINGA_MaxChars, 2000,
									GA_ID, GID_URL,
									GA_RelVerify, TRUE,
									AMI_GA_HELP, g->shared->helphints[GID_URL],
									GA_TabCycle, TRUE,
									STRINGA_Buffer, g->shared->svbuffer,
#ifdef __amigaos4__
									STRINGVIEW_Header, URLHistory_GetList(),
#endif
							TAG_DONE),
						LAYOUT_AddChild, g->shared->objects[GID_PAGEINFO] = ButtonObj,
							GA_ID, GID_PAGEINFO,
							GA_RelVerify, TRUE,
							GA_ReadOnly, FALSE,
							btn_bevel_tag, btn_bevel_val,
							BUTTON_RenderImage, g->shared->objects[GID_PAGEINFO_INTERNAL_BM],
						ButtonEnd,
						CHILD_WeightedWidth, 0,
						CHILD_WeightedHeight, 0,
						fave_add, fave_btn,
						fave_ww, 0,
						fave_wh, 0,
					LayoutEnd,
					CHILD_WeightedWidth, 100,
					weight_bar_tag, weight_bar_val,
					search_row_add, search_row,
					search_row_ww_tag, search_row_ww,
					/* Gap: URL/search cluster → throbber */
					LAYOUT_AddChild, SpaceObj,
						SPACE_MinWidth, AMI_TB_GROUP_GAP,
						SPACE_MinHeight, 1,
						SPACE_Transparent, TRUE,
						GA_ReadOnly, TRUE,
					SpaceEnd,
					CHILD_WeightedWidth, 0,
					CHILD_WeightedHeight, 0,
					/* Nami: TAG_IGNORE — SpaceObj still created, chrome places it */
					throb_in_tb, g->shared->objects[GID_THROBBER] = SpaceObj,
						GA_ID,GID_THROBBER,
						SPACE_MinWidth, throbber_w,
						SPACE_MinHeight, throbber_h,
						SPACE_Transparent,TRUE,
					SpaceEnd,
					CHILD_WeightedWidth,0,
					CHILD_WeightedHeight,0,
			LayoutEnd;
			toolbar_obj = g->shared->objects[GID_TOOLBARLAYOUT];
			throb_gad = g->shared->objects[GID_THROBBER];

			if(g->shared->ui_nami) {
				/*
				 * Close | Toggle | toolbar(URL expands) | Drag |
				 * Throbber (boingball) | Zoom | Depth | RPad
				 */
				g->shared->objects[GID_CHROMELAYOUT] = LayoutHObj,
					LAYOUT_SpaceInner, FALSE,
					LAYOUT_SpaceOuter, FALSE,
					LAYOUT_InnerSpacing, 1,
					LAYOUT_VertAlignment, LALIGN_CENTER,
					LAYOUT_AddChild, g->shared->objects[GID_WIN_CLOSE],
					CHILD_WeightedWidth, 0,
					CHILD_WeightedHeight, 0,
					CHILD_MinWidth, 20,
					CHILD_MinHeight, 20,
					LAYOUT_AddChild, g->shared->objects[GID_SIDE_TOGGLE],
					CHILD_WeightedWidth, 0,
					CHILD_WeightedHeight, 0,
					CHILD_MinWidth, 20,
					CHILD_MinHeight, 20,
					LAYOUT_AddChild, toolbar_obj,
					CHILD_WeightedWidth, 100,
					CHILD_WeightedHeight, 0,
					LAYOUT_AddChild, g->shared->objects[GID_WIN_DRAG],
					CHILD_WeightedWidth, 0,
					CHILD_WeightedHeight, 0,
					CHILD_MinWidth, 40,
					CHILD_MaxWidth, 56,
					LAYOUT_AddChild, throb_gad,
					CHILD_WeightedWidth, 0,
					CHILD_WeightedHeight, 0,
					LAYOUT_AddChild, g->shared->objects[GID_CHROME_RPAD],
					CHILD_WeightedWidth, 0,
					CHILD_WeightedHeight, 0,
					CHILD_MinWidth, 18,
					CHILD_MaxWidth, 18,
				LayoutEnd;
				g->shared->objects[GID_TABLAYOUT] = g->shared->objects[GID_CHROMELAYOUT];
				chrome_obj = g->shared->objects[GID_CHROMELAYOUT];
			}

			/* Browser column (scrollers + log + status) */
			browser_col = LayoutVObj,
					LAYOUT_AddChild, g->shared->objects[GID_VSCROLLLAYOUT] = LayoutHObj,
						LAYOUT_AddChild, LayoutVObj,
							LAYOUT_AddChild, g->shared->objects[GID_HSCROLLLAYOUT] = LayoutVObj,
								LAYOUT_AddChild, g->shared->objects[GID_BROWSER] = SpaceObj,
									GA_ID,GID_BROWSER,
									SPACE_Transparent,TRUE,
									SPACE_MinWidth, 16,
									SPACE_MinHeight, 16,
									SPACE_RenderHook, &g->shared->browser_hook,
								SpaceEnd,
							EndGroup,
						EndGroup,
					EndGroup,
//					LAYOUT_WeightBar, TRUE,
					LAYOUT_AddChild, g->shared->objects[GID_LOGLAYOUT] = LayoutVObj,
					EndGroup,
					CHILD_WeightedHeight, 0,
#ifndef __amigaos4__
					status_add, status_gad,
#endif
			EndGroup;

			if(g->shared->ui_nami) {
				Object *side_panel;

				/* SIDELAYOUT | weight bar | browser */
				side_panel = g->shared->objects[GID_SIDELAYOUT];

				g->shared->objects[GID_BROWSERCOL] = browser_col;
				g->shared->objects[GID_BODYLAYOUT] = LayoutHObj,
					LAYOUT_SpaceInner, FALSE,
					LAYOUT_SpaceOuter, FALSE,
					LAYOUT_AddChild, g->shared->objects[GID_SIDE_STRIP],
					CHILD_WeightedWidth, 0,
					CHILD_MinWidth, 0,
					CHILD_MaxWidth, 0,
					LAYOUT_AddChild, side_panel,
					CHILD_WeightedWidth, 20,
					CHILD_MinWidth, 96,
					CHILD_CacheDomain, FALSE,
					CHILD_NoDispose, TRUE,
					LAYOUT_WeightBar, TRUE,
					LAYOUT_AddChild, browser_col,
					CHILD_WeightedWidth, 100,
					CHILD_NoDispose, TRUE,
				LayoutEnd;
				body_obj = g->shared->objects[GID_BODYLAYOUT];
			} else {
				g->shared->objects[GID_BROWSERCOL] = NULL;
				g->shared->objects[GID_SIDE_VIRTUAL] = NULL;
				body_obj = browser_col;
			}


		g->shared->objects[OID_MAIN] = WindowObj,
			WA_ScreenTitle, ami_gui_get_screen_title(),
			WA_Activate, TRUE,
			WA_DepthGadget, wa_depth_gad,
			WA_DragBar, wa_drag_gad,
			WA_CloseGadget, wa_close_gad,
			WA_SizeGadget, TRUE,
			WA_SizeBRight, wa_size_bright,
			WA_PubScreen,scrn,
			WA_ReportMouse,TRUE,
			refresh_mode, TRUE,
			WA_SizeBBottom, TRUE,
			WA_ContextMenuHook, g->shared->ctxmenu_hook,
			WA_IDCMP, IDCMP_MENUPICK | IDCMP_MOUSEMOVE |
				IDCMP_MOUSEBUTTONS | IDCMP_NEWSIZE |
				IDCMP_RAWKEY | idcmp_sizeverify |
				IDCMP_GADGETUP | IDCMP_GADGETDOWN | IDCMP_IDCMPUPDATE |
				IDCMP_REFRESHWINDOW |
				IDCMP_ACTIVEWINDOW | IDCMP_INTUITICKS |
				IDCMP_EXTENDEDMOUSE | gtdrag_idcmp,
			WINDOW_Position, WPOS_FULLSCREEN,
			WINDOW_RefWindow, ref,
			WINDOW_IconifyGadget, iconifygadget,
			WINDOW_MenuStrip, menu,
			WINDOW_MenuUserData, WGUD_HOOK,
			WINDOW_NewPrefsHook, &newprefs_hook,
			WINDOW_IDCMPHook, &g->shared->scrollerhook,
			WINDOW_IDCMPHookBits, IDCMP_IDCMPUPDATE | IDCMP_REFRESHWINDOW |
						IDCMP_EXTENDEDMOUSE | gtdrag_idcmp,
			WINDOW_SharedPort, sport,
			WINDOW_BuiltInScroll, wa_builtin_scroll,
			WINDOW_GadgetHelp, TRUE,
			WINDOW_HintInfo, ami_empty_hintinfo,
#ifdef __amigaos4__
			WINDOW_UniqueID, "NS_MAIN_WIN",
			WINDOW_PopupGadget, TRUE,
#endif
			WINDOW_UserData, g->shared,
  			WINDOW_ParentGroup, g->shared->objects[GID_MAIN] = LayoutVObj,
				LAYOUT_DeferLayout, defer_layout,
				LAYOUT_SpaceOuter, space_outer,
				inner_sp_tag, inner_sp_val,
				chrome_add, chrome_obj,
				chrome_wh, 0,
				toolbar_row_add, toolbar_obj,
				CHILD_WeightedHeight,0,
				LAYOUT_AddImage, BevelObj,
					BEVEL_Style, BVS_SBAR_VERT,
				BevelEnd,
				CHILD_WeightedHeight, 0,
				LAYOUT_AddChild, g->shared->objects[GID_HOTLISTLAYOUT] = LayoutVObj,
					LAYOUT_SpaceInner, FALSE,
				LayoutEnd,
				CHILD_WeightedHeight,0,
				tl_add, tl_obj,
				tl_wh, 0,
				LAYOUT_AddChild, body_obj,
			EndGroup,
		EndWindow;
		} /* Nami/NetSurf layout locals */
	}
	else
	{
		/* borderless kiosk mode window */
		g->tab = 0;
		g->shared->tabs = 0;
		g->tab_node = NULL;

		g->shared->objects[OID_MAIN] = WindowObj,
       	    WA_ScreenTitle, ami_gui_get_screen_title(),
           	WA_Activate, TRUE,
           	WA_DepthGadget, FALSE,
       	   	WA_DragBar, FALSE,
           	WA_CloseGadget, FALSE,
			WA_Borderless,TRUE,
			WA_RMBTrap,TRUE,
			WA_Top,0,
			WA_Left,0,
			WA_Width, scrn->Width,
			WA_Height, scrn->Height,
           	WA_SizeGadget, FALSE,
			WA_PubScreen, scrn,
			WA_ReportMouse, TRUE,
			refresh_mode, TRUE,
       	   	WA_IDCMP, IDCMP_MENUPICK | IDCMP_MOUSEMOVE |
					IDCMP_MOUSEBUTTONS | IDCMP_NEWSIZE |
					IDCMP_RAWKEY | IDCMP_REFRESHWINDOW |
					IDCMP_GADGETUP | IDCMP_IDCMPUPDATE |
					IDCMP_EXTENDEDMOUSE,
			WINDOW_IDCMPHook,&g->shared->scrollerhook,
			WINDOW_IDCMPHookBits, IDCMP_IDCMPUPDATE |
					IDCMP_EXTENDEDMOUSE | IDCMP_REFRESHWINDOW,
			WINDOW_SharedPort,sport,
			WINDOW_UserData,g->shared,
			WINDOW_BuiltInScroll,TRUE,
			WINDOW_ParentGroup, g->shared->objects[GID_MAIN] = LayoutHObj,
			LAYOUT_DeferLayout, defer_layout,
			LAYOUT_SpaceOuter, TRUE,
				LAYOUT_AddChild, g->shared->objects[GID_VSCROLLLAYOUT] = LayoutHObj,
					LAYOUT_AddChild, g->shared->objects[GID_HSCROLLLAYOUT] = LayoutVObj,
						LAYOUT_AddChild, g->shared->objects[GID_BROWSER] = SpaceObj,
							GA_ID,GID_BROWSER,
							SPACE_Transparent,TRUE,
						SpaceEnd,
					EndGroup,
				EndGroup,
			EndGroup,
		EndWindow;
	}

	NSLOG(netsurf, INFO, "Opening window");

	g->shared->win = (struct Window *)RA_OpenWindow(g->shared->objects[OID_MAIN]);

	NSLOG(netsurf, INFO, "Window opened, adding border gadgets");

	/* Clear the reference window pointer to ensure it doesn't get used later */
	SetAttrs(g->shared->objects[OID_MAIN], WINDOW_RefWindow, NULL, TAG_DONE);

	if(!g->shared->win)
	{
		amiga_warn_user("NoMemory","");
		free(g->shared);
		free(g);
		return NULL;
	}

	if(nsoption_bool(kiosk_mode) == false)
	{
#ifdef __amigaos4__
		/* NetSurf layout: bottom-border status gauge. Nami puts status in the screen title. */
		if(g->shared->ui_nami == false) {
		ULONG width, height;
		struct DrawInfo *dri = GetScreenDrawInfo(scrn);
		
		ami_get_border_gadget_size(g->shared,
				(ULONG *)&width, (ULONG *)&height);

		g->shared->objects[GID_STATUS] = NewObject(
				NULL,
				"frbuttonclass",
				GA_ID, GID_STATUS,
				GA_Left, scrn->WBorLeft + 2,
				GA_RelBottom, scrn->WBorBottom - (height/2),
				GA_BottomBorder, TRUE,
				GA_Width, width,
				GA_Height, 1 + height - scrn->WBorBottom,
				GA_DrawInfo, dri,
				GA_ReadOnly, TRUE,
				GA_Disabled, TRUE,
				GA_Image, (struct Image *)NewObject(
					NULL,
					"gaugeiclass",
					GAUGEIA_Level, 0,
					IA_Top, (int)(- ceil((scrn->WBorBottom + height) / 2)),
					IA_Left, -4,
					IA_Height, 2 + height - scrn->WBorBottom, 
					IA_Label, NULL,
					IA_InBorder, TRUE,
					IA_Screen, scrn,
					TAG_DONE),
				TAG_DONE);

		AddGList(g->shared->win, (struct Gadget *)g->shared->objects[GID_STATUS],
				(UWORD)~0, -1, NULL);

		/* Apparently you can't set GA_Width on creation time for frbuttonclass */

		SetGadgetAttrs((struct Gadget *)g->shared->objects[GID_STATUS],
			g->shared->win, NULL,
			GA_Width, width,
			TAG_DONE);

		RefreshGadgets((APTR)g->shared->objects[GID_STATUS],
				g->shared->win, NULL);
				
		FreeScreenDrawInfo(scrn, dri);
		}
#endif //__amigaos4__				
		if(g->shared->ui_nami == false)
			ami_gui_hotlist_toolbar_add(g->shared);
		if(g->shared->ui_nami == false &&
		   nsoption_bool(tab_always_show))
			ami_toggletabbar(g->shared, true);
		if(g->shared->ui_nami) {
			ami_update_buttons(g->shared);
			ami_gui_nami_create_border_scrollers(g->shared);
			ami_gui_update_screentitle(g->shared);
			/* First tab button + HotlistToolbar favicon cluster */
			ami_gui_nami_sidebar_create_tab_btn(g);
			ami_gui_nami_sidebar_refresh_hotlist(g->shared);
			ami_gui_nami_sidebar_sync_selection(g->shared);
			ami_gui_nami_gtdrag_window_add(g->shared);
		}
	}

	g->shared->gw = g;
	cur_gw = g;

	g->shared->appwin = AddAppWindowA((ULONG)g->shared->objects[OID_MAIN],
							(ULONG)g->shared, g->shared->win, appport, NULL);

	ami_gui_win_list_add(g->shared, AMINS_WINDOW, &ami_gui_table);

	if(locked_screen) {
		UnlockPubScreen(NULL,scrn);
		locked_screen = FALSE;
	}

	ScreenToFront(scrn);

	return g;
}

static void ami_gui_close_tabs(struct gui_window_2 *gwin, bool other_tabs)
{
	struct Node *tab;
	struct Node *ntab;
	struct gui_window *gw;

	if((gwin->tabs > 1) && (nsoption_bool(tab_close_warn) == true)) {
		int32 res = amiga_warn_user_multi(messages_get("MultiTabClose"), "Yes", "No", gwin->win);

		if(res == 0) return;
	}

	if(gwin->tabs) {
		tab = GetHead(&gwin->tab_list);

		do {
			ntab=GetSucc(tab);
			GetClickTabNodeAttrs(tab,
								TNA_UserData,&gw,
								TAG_DONE);

			if(gw &&((other_tabs == false) || (gwin->gw != gw))) {
				browser_window_destroy(gw->bw);
			}
		} while((tab=ntab));
	} else {
		if(other_tabs == false) browser_window_destroy(gwin->gw->bw);
	}
}

void ami_gui_close_window(void *w)
{
	struct gui_window_2 *gwin = (struct gui_window_2 *)w;
	ami_gui_close_tabs(gwin, false);
}

void ami_gui_close_inactive_tabs(struct gui_window_2 *gwin)
{
	ami_gui_close_tabs(gwin, true);
}

static void ami_gui_purge_after_close(void *p)
{
	(void)p;
#ifndef __amigaos4__
	/* Content refs are released after guit->window->destroy returns. */
	ami_memory_purge_after_close();
#endif
}

static void gui_window_destroy(struct gui_window *g)
{
	struct Node *ptab = NULL;
	int gid;

	if(!g) return;

	if(ami_gtd_pend_src_gw == g || ami_gtd_pend_destroy_gw == g)
		ami_gui_nami_gtdrag_cancel_pending();
	if(ami_gtdrag_get_drag_source() == g)
		ami_gtdrag_clear_drag_source();

	if(g->shared != NULL && ami_gtd_abs_owner == g->shared)
		ami_gui_nami_gtdrag_restore_all(g->shared);

	if (ami_search_get_gwin(g->shared->searchwin) == g)
	{
		ami_search_close();
		win_destroyed = true;
	}

	if(g->hw)
	{
		ami_history_local_destroy(g->hw);
		win_destroyed = true;
	}

	ami_free_download_list(&g->dllist);
	/* Drain deferred rects so itempool payloads leave the tracker before DeletePool */
	ami_gui_window_update_box_deferred(g, false);
	FreeObjList(g->deferred_rects);
	ami_memory_itempool_delete(g->deferred_rects_pool);
	gui_window_stop_throbber(g);

	cur_gw = NULL;

	if(g->shared->tabs > 1) {
		if(g->shared->ui_nami) {
			struct Node *sptab;
			struct gui_window *next_gw;

			sptab = NULL;
			next_gw = NULL;

			/* Prefer successor/predecessor in the ClickTab node list */
			if(g == g->shared->gw || g->shared->gw == NULL) {
				sptab = GetSucc(g->tab_node);
				if(sptab == NULL || sptab == g->shared->new_tab_tab)
					sptab = GetPred(g->tab_node);
				if(sptab != NULL && sptab != g->shared->new_tab_tab)
					GetClickTabNodeAttrs(sptab, TNA_UserData, &next_gw, TAG_DONE);
			} else {
				next_gw = g->shared->gw;
			}

			ami_gui_nami_sidebar_destroy_tab_btn(g);
			Remove(g->tab_node);
			FreeClickTabNode(g->tab_node);
			g->tab_node = NULL;

			g->shared->tabs--;
			if(next_gw != NULL)
				ami_switch_tab_to(g->shared, next_gw, true);
			else
				ami_gui_nami_sidebar_sync_selection(g->shared);
			ami_gui_relabel_all_tabs(g->shared);
			ami_schedule(0, ami_gui_refresh_favicon, g->shared);

			if((g->shared->tabs == 1) && (nsoption_bool(tab_always_show) == false))
				ami_toggletabbar(g->shared, false);

			FreeListBrowserList(&g->loglist);
			if(g->logcolumns != NULL)
				FreeLBColumnInfo(g->logcolumns);

			/* destroy_tab_btn already freed sidebar_help / sidebar_icon */
			if(g->tabtitle) free(g->tabtitle);
			if(g->tab_label) free(g->tab_label);
			free(g);
			ami_schedule(0, ami_gui_purge_after_close, NULL);
			return;
		} else {
			SetGadgetAttrs((struct Gadget *)g->shared->objects[GID_TABS],g->shared->win,NULL,
							CLICKTAB_Labels,~0,
							TAG_DONE);

			GetAttr(CLICKTAB_CurrentNode, g->shared->objects[GID_TABS], (ULONG *)&ptab);

			if((ptab == g->tab_node) || (ptab == g->shared->new_tab_tab)) {
				ptab = GetSucc(g->tab_node);
				if((ptab == NULL) || (ptab == g->shared->new_tab_tab)) ptab = GetPred(g->tab_node);
			}

			Remove(g->tab_node);
			FreeClickTabNode(g->tab_node);
			RefreshSetGadgetAttrs((struct Gadget *)g->shared->objects[GID_TABS], g->shared->win, NULL,
							CLICKTAB_Labels, &g->shared->tab_list,
							CLICKTAB_CurrentNode, ptab,
							TAG_DONE);

			if(ClickTabBase->lib_Version < 53) {
				RethinkLayout((struct Gadget *)g->shared->objects[GID_TABLAYOUT],
					g->shared->win, NULL, TRUE);
			}
		}

		g->shared->tabs--;
		ami_switch_tab(g->shared,true);
		ami_gui_relabel_all_tabs(g->shared);
		ami_schedule(0, ami_gui_refresh_favicon, g->shared);

		if((g->shared->tabs == 1) && (nsoption_bool(tab_always_show) == false))
			ami_toggletabbar(g->shared, false);

		FreeListBrowserList(&g->loglist);
		if(g->logcolumns != NULL)
			FreeLBColumnInfo(g->logcolumns);

		if(g->tabtitle) free(g->tabtitle);
		if(g->tab_label) free(g->tab_label);
		if(g->sidebar_help) free(g->sidebar_help);
		if(g->sidebar_icon != NULL) {
			DisposeObject(g->sidebar_icon);
			g->sidebar_icon = NULL;
		}
		free(g);
		/* After browser_window_destroy finishes releasing content */
		ami_schedule(0, ami_gui_purge_after_close, NULL);
		return;
	}

	ami_plot_release_pens(g->shared->shared_pens);
	free(g->shared->shared_pens);
	ami_schedule_redraw_remove(g->shared);
	ami_schedule(-1, ami_gui_refresh_favicon, g->shared);
	ami_schedule(-1, ami_gui_nami_sidebar_toggle_cb, g->shared);
	ami_schedule(-1, ami_gui_nami_new_tab_cb, g->shared);
	ami_schedule(-1, ami_gui_nami_close_tab_cb, g->shared);
	ami_schedule(-1, ami_gui_nami_sidebar_refresh_cb, g->shared);
	ami_schedule(-1, ami_gui_nami_gtdrag_refresh_cb, g->shared);
	ami_schedule(-1, ami_gui_nami_gtdrag_unlock_poll_cb, NULL);
	/* Cancel throbber before disposing LED/boingball images */
	ami_throbber_redraw_schedule(-1, g);

	/* Detach and dispose tab-close image before disposing the window */
	if(g->shared->objects[GID_CLOSETAB_BM] != NULL && g->shared->win != NULL) {
		if(g->shared->objects[GID_TABS] != NULL) {
			SetGadgetAttrs((struct Gadget *)g->shared->objects[GID_TABS],
					g->shared->win, NULL,
					CLICKTAB_CloseImage, NULL,
					TAG_DONE);
		}
		if(g->shared->objects[GID_CLOSETAB] != NULL) {
			SetGadgetAttrs((struct Gadget *)g->shared->objects[GID_CLOSETAB],
					g->shared->win, NULL,
					BUTTON_RenderImage, NULL,
					TAG_DONE);
		}
		DisposeObject(g->shared->objects[GID_CLOSETAB_BM]);
		DisposeObject(g->shared->objects[GID_CLOSETAB_BM_G]);
		g->shared->objects[GID_CLOSETAB_BM] = NULL;
		g->shared->objects[GID_CLOSETAB_BM_G] = NULL;
	}

	/* Detach Nami border scrollers/depth before disposing the window object.
	 * Clears GA_Image on buttongclass first to avoid double-free of sysi. */
	if(g->shared->ui_nami) {
		/* Unregister gtdrag before tearing down gadgets */
		ami_gui_nami_gtdrag_window_rem(g->shared);

		/* Tear down hotlist faces/URLs; pool buttons die with SIDELAYOUT */
		ami_gui_nami_sidebar_clear_hotlist(g->shared);
		ami_gui_nami_sidebar_destroy_tab_btn(g);

		/* Clear tab pool faces (button.gadget does not free RenderImage) */
		{
			int ti;

			for(ti = 0; ti < AMI_SIDE_TAB_MAX; ti++) {
				if(g->shared->side_tab_btn[ti] != NULL) {
					if(g->shared->win != NULL) {
						SetGadgetAttrs((struct Gadget *)g->shared->side_tab_btn[ti],
								g->shared->win, NULL,
								GA_Text, (STRPTR)"",
								BUTTON_RenderImage, NULL,
								TAG_DONE);
					} else {
						SetAttrs(g->shared->side_tab_btn[ti],
								GA_Text, (STRPTR)"",
								BUTTON_RenderImage, NULL,
								TAG_DONE);
					}
				}
				if(g->shared->side_tab_icon_btn[ti] != NULL) {
					if(g->shared->win != NULL) {
						SetGadgetAttrs((struct Gadget *)g->shared->side_tab_icon_btn[ti],
								g->shared->win, NULL,
								BUTTON_RenderImage, NULL,
								TAG_DONE);
					} else {
						SetAttrs(g->shared->side_tab_icon_btn[ti],
								BUTTON_RenderImage, NULL,
								TAG_DONE);
					}
				}
				g->shared->side_tab_btn[ti] = NULL;
				g->shared->side_tab_icon_btn[ti] = NULL;
				g->shared->side_tab_row[ti] = NULL;
				g->shared->side_tab_gw[ti] = NULL;
			}
		}

		/* Hotlist pool buttons are owned by HOTLAYOUT/SIDELAYOUT */
		{
			int hi;
			int ri;

			for(hi = 0; hi < AMI_SIDE_HOTLIST_MAX; hi++)
				g->shared->side_hot_btn[hi] = NULL;
			for(ri = 0; ri < AMI_SIDE_HOT_ROWS; ri++)
				g->shared->side_hot_row[ri] = NULL;
		}

		/* button.gadget does not free BUTTON_RenderImage — clear refs first */
		if(g->shared->win != NULL) {
			if(g->shared->objects[GID_WIN_CLOSE] != NULL)
				SetGadgetAttrs((struct Gadget *)g->shared->objects[GID_WIN_CLOSE],
						g->shared->win, NULL,
						BUTTON_RenderImage, NULL, TAG_DONE);
			/* Zoom/depth are SpaceObjs — no BUTTON_RenderImage to clear */
			if(g->shared->objects[GID_SIDE_TOGGLE] != NULL)
				SetGadgetAttrs((struct Gadget *)g->shared->objects[GID_SIDE_TOGGLE],
						g->shared->win, NULL,
						BUTTON_RenderImage, NULL, TAG_DONE);
			if(g->shared->objects[GID_SIDE_NEWTAB] != NULL)
				SetGadgetAttrs((struct Gadget *)g->shared->objects[GID_SIDE_NEWTAB],
						g->shared->win, NULL,
						BUTTON_RenderImage, NULL, TAG_DONE);
			if(g->shared->objects[GID_SIDE_CLOSETAB] != NULL)
				SetGadgetAttrs((struct Gadget *)g->shared->objects[GID_SIDE_CLOSETAB],
						g->shared->win, NULL,
						BUTTON_RenderImage, NULL, TAG_DONE);
		}

		/*
		 * Side panel uses CHILD_NoDispose so collapse RemoveChild does not
		 * free it.  Detach from BODY if still attached, then dispose
		 * SIDELAYOUT ourselves.  Clear browser NoDispose so OID_MAIN can
		 * free the browser column.
		 */
		{
			Object *body;
			Object *browser;
			Object *side;

			body = g->shared->objects[GID_BODYLAYOUT];
			side = g->shared->objects[GID_SIDELAYOUT];
			browser = g->shared->objects[GID_BROWSERCOL];

			if(body != NULL && g->shared->win != NULL) {
				if(g->shared->sidebar_expanded && side != NULL) {
					SetGadgetAttrs((struct Gadget *)body, g->shared->win, NULL,
							LAYOUT_RemoveChild, side,
							TAG_DONE);
					g->shared->sidebar_expanded = false;
				}
				if(browser != NULL) {
					SetGadgetAttrs((struct Gadget *)body, g->shared->win, NULL,
							LAYOUT_ModifyChild, browser,
								CHILD_NoDispose, FALSE,
							TAG_DONE);
				}
			}

			if(side != NULL) {
				DisposeObject(side);
				g->shared->objects[GID_SIDELAYOUT] = NULL;
			}
			g->shared->objects[GID_SIDE_VIRTUAL] = NULL;
		}

		ami_gui_nami_destroy_border_scrollers(g->shared);
	}

	DisposeObject(g->shared->objects[OID_MAIN]);
	g->shared->objects[OID_MAIN] = NULL;
	/* Layout chrome slots were owned by the window object */
	g->shared->objects[GID_WIN_CLOSE] = NULL;
	g->shared->objects[GID_WIN_ZOOM] = NULL;
	g->shared->objects[GID_WIN_DEPTH] = NULL;
	g->shared->objects[GID_WIN_DRAG] = NULL;
	g->shared->objects[GID_CHROME_RPAD] = NULL;
	g->shared->objects[GID_CHROMELAYOUT] = NULL;
	g->shared->objects[GID_TABLAYOUT] = NULL;
	g->shared->objects[GID_TABS] = NULL;
	g->shared->objects[GID_THROBBER] = NULL;
	g->shared->objects[GID_SIDE_NEWTAB] = NULL;
	g->shared->objects[GID_SIDE_CLOSETAB] = NULL;
	g->shared->objects[GID_SIDE_HOTLAYOUT] = NULL;
	g->shared->objects[GID_SIDE_TABLIST] = NULL;
	g->shared->objects[GID_SIDE_TOGGLE] = NULL;
	g->shared->objects[GID_SIDE_STRIP] = NULL;
	g->shared->objects[GID_SIDELAYOUT] = NULL;
	g->shared->objects[GID_SIDE_VIRTUAL] = NULL;
	g->shared->objects[GID_BODYLAYOUT] = NULL;
	g->shared->objects[GID_BROWSERCOL] = NULL;
	g->shared->objects[GID_TOOLBARLAYOUT] = NULL;

	if(g->shared->throbber_led != NULL) {
		DisposeObject(g->shared->throbber_led);
		g->shared->throbber_led = NULL;
	}
	if(g->shared->throbber_boing != NULL) {
		DisposeObject(g->shared->throbber_boing);
		g->shared->throbber_boing = NULL;
	}

	ami_gui_appicon_remove(g->shared);
	if(g->shared->appwin) RemoveAppWindow(g->shared->appwin);
	ami_gui_hotlist_toolbar_free(g->shared, &g->shared->hotlist_toolbar_list);

	/* button.gadget does not dispose BUTTON_RenderImage objects */
	DisposeObject(g->shared->objects[GID_BACK_BM]);
	DisposeObject(g->shared->objects[GID_BACK_BM_S]);
	DisposeObject(g->shared->objects[GID_BACK_BM_H]);
	DisposeObject(g->shared->objects[GID_BACK_BM_G]);
	DisposeObject(g->shared->objects[GID_FORWARD_BM]);
	DisposeObject(g->shared->objects[GID_FORWARD_BM_S]);
	DisposeObject(g->shared->objects[GID_FORWARD_BM_H]);
	DisposeObject(g->shared->objects[GID_FORWARD_BM_G]);
	DisposeObject(g->shared->objects[GID_STOP_BM]);
	DisposeObject(g->shared->objects[GID_STOP_BM_S]);
	DisposeObject(g->shared->objects[GID_STOP_BM_H]);
	DisposeObject(g->shared->objects[GID_STOP_BM_G]);
	DisposeObject(g->shared->objects[GID_RELOAD_BM]);
	DisposeObject(g->shared->objects[GID_RELOAD_BM_S]);
	DisposeObject(g->shared->objects[GID_RELOAD_BM_H]);
	DisposeObject(g->shared->objects[GID_RELOAD_BM_G]);
	DisposeObject(g->shared->objects[GID_HOME_BM]);
	DisposeObject(g->shared->objects[GID_HOME_BM_S]);
	DisposeObject(g->shared->objects[GID_HOME_BM_H]);
	DisposeObject(g->shared->objects[GID_HOME_BM_G]);
	DisposeObject(g->shared->objects[GID_TABS_FLAG]);
	/* Chrome AISS BitMapObjs (cleared from buttons above) */
	DisposeObject(g->shared->objects[GID_WIN_CLOSE_BM]);
	DisposeObject(g->shared->objects[GID_WIN_ZOOM_BM]);
	DisposeObject(g->shared->objects[GID_WIN_DEPTH_BM]);
	DisposeObject(g->shared->objects[GID_SIDE_NEWTAB_BM]);
	DisposeObject(g->shared->objects[GID_SIDE_CLOSETAB_BM]);
	DisposeObject(g->shared->objects[GID_SIDE_TOGGLE_BM]);
	g->shared->objects[GID_WIN_CLOSE_BM] = NULL;
	g->shared->objects[GID_WIN_ZOOM_BM] = NULL;
	g->shared->objects[GID_WIN_DEPTH_BM] = NULL;
	g->shared->objects[GID_SIDE_NEWTAB_BM] = NULL;
	g->shared->objects[GID_SIDE_CLOSETAB_BM] = NULL;
	g->shared->objects[GID_SIDE_TOGGLE_BM] = NULL;
	g->shared->objects[GID_CLOSETAB_BM] = NULL;
	if(g->shared->sidebar_def_icon != NULL) {
		DisposeObject(g->shared->sidebar_def_icon);
		g->shared->sidebar_def_icon = NULL;
	}
	DisposeObject(g->shared->objects[GID_FAVE_ADD]);
	DisposeObject(g->shared->objects[GID_FAVE_RMV]);
	DisposeObject(g->shared->objects[GID_PAGEINFO_INSECURE_BM]);
	DisposeObject(g->shared->objects[GID_PAGEINFO_INTERNAL_BM]);
	DisposeObject(g->shared->objects[GID_PAGEINFO_LOCAL_BM]);
	DisposeObject(g->shared->objects[GID_PAGEINFO_SECURE_BM]);
	DisposeObject(g->shared->objects[GID_PAGEINFO_WARNING_BM]);

	ami_gui_opts_websearch_free(g->shared->web_search_list);
	if(g->shared->search_bm) DisposeObject(g->shared->search_bm);

	/* This appears to be disposed along with the ClickTab object
	if(g->shared->clicktab_ctxmenu) DisposeObject((Object *)g->shared->clicktab_ctxmenu); */
	DisposeObject((Object *)g->shared->history_ctxmenu[AMI_CTXMENU_HISTORY_BACK]);
	DisposeObject((Object *)g->shared->history_ctxmenu[AMI_CTXMENU_HISTORY_FORWARD]);
	ami_ctxmenu_release_hook(g->shared->ctxmenu_hook);
	ami_gui_menu_free(g->shared);

	FreeListBrowserList(&g->loglist);
	if(g->logcolumns != NULL)
		FreeLBColumnInfo(g->logcolumns);

	free(g->shared->wintitle);
	ami_utf8_free(g->shared->status);
	if(ami_last_screentitle_gwin == g->shared)
		ami_last_screentitle_gwin = NULL;
	if(g->shared->screentitle) {
		free(g->shared->screentitle);
		g->shared->screentitle = NULL;
	}
	free(g->shared->svbuffer);
	if(g->shared->new_tab_tab != NULL)
		FreeClickTabNode(g->shared->new_tab_tab);

	for(gid = 0; gid < GID_LAST; gid++)
		ami_utf8_free(g->shared->helphints[gid]);

	ami_gui_win_list_remove(g->shared);
	if(g->tab_node) {
		Remove(g->tab_node);
		FreeClickTabNode(g->tab_node);
	}
	g->tab_node = NULL;
	/* Button already removed in destroy_tab_btn; strings still need freeing */
	if(g->tabtitle) free(g->tabtitle);
	if(g->tab_label) free(g->tab_label);
	if(g->sidebar_help) free(g->sidebar_help);
	if(g->sidebar_icon != NULL) {
		DisposeObject(g->sidebar_icon);
		g->sidebar_icon = NULL;
	}
	free(g); // g->shared should be freed by DelObject()

	if(IsMinListEmpty(window_list))
	{
		/* last window closed, so exit */
		ami_try_quit();
	}

	win_destroyed = true;
	ami_schedule(0, ami_gui_purge_after_close, NULL);
}

static void ami_redraw_callback(void *p)
{
	struct gui_window_2 *gwin = (struct gui_window_2 *)p;

	/*
	 * Full redraw discards deferred boxes (already painted). Do not
	 * also paint deferred fragments in the same tick — that re-blitted
	 * the same images after a full paint. If the document is not ready,
	 * leave deferred queued and retry via redraw_required.
	 */
	if(gwin->redraw_required) {
		ami_do_redraw(gwin);
		if(gwin->redraw_required == false) {
			if(gwin->gw->c_h) {
				gui_window_place_caret(gwin->gw, gwin->gw->c_x,
					gwin->gw->c_y, gwin->gw->c_h, NULL);
			}
			return;
		}
		/* Not ready yet — do not paint stale deferred strips. */
		return;
	}

	ami_gui_window_update_box_deferred(gwin->gw, true);
	gwin->redraw_interactive = false;

	if(gwin->gw->c_h)
	{
		gui_window_place_caret(gwin->gw, gwin->gw->c_x,
		gwin->gw->c_y, gwin->gw->c_h, NULL);
	}
}

/**
 * Schedule a redraw of the browser window - Amiga-specific function
 *
 * \param  gwin         a struct gui_window_2
 * \param  full_redraw  set to true to schedule a full redraw,
                        should only be set to false when called from amiga_window_invalidate_area()
 */
static void ami_schedule_redraw_ex(struct gui_window_2 *gwin, bool full_redraw,
		bool interactive)
{
	int ms;

	if(interactive) {
		ms = AMI_REDRAW_INTERACTIVE_MS;
		gwin->redraw_interactive = true;
	} else if(gwin->redraw_interactive) {
		/* Content settle arrived while interactive paint pending —
		 * keep the short timer so scroll/resize stay snappy. */
		ms = AMI_REDRAW_INTERACTIVE_MS;
	} else {
		ms = AMI_REDRAW_CONTENT_MS;
	}

	if(full_redraw) gwin->redraw_required = true;
	ami_schedule(ms, ami_redraw_callback, gwin);
}

void ami_schedule_redraw(struct gui_window_2 *gwin, bool full_redraw)
{
	ami_schedule_redraw_ex(gwin, full_redraw, false);
}

void ami_schedule_redraw_interactive(struct gui_window_2 *gwin, bool full_redraw)
{
	ami_schedule_redraw_ex(gwin, full_redraw, true);
}

static void ami_schedule_redraw_remove(struct gui_window_2 *gwin)
{
	ami_schedule(-1, ami_redraw_callback, gwin);
}

static void ami_gui_window_update_box_deferred(struct gui_window *g, bool draw)
{
	struct nsObject *node;
	struct nsObject *nnode;
	struct rect *rect;
	struct rect union_rect;
	BOOL have_union;
	struct IBox *bbox;
	LONG view_w, view_h, union_w, union_h;

	if(!g) return;
	if(IsMinListEmpty(g->deferred_rects)) return;

	have_union = FALSE;
	bbox = NULL;
	view_w = 0;
	view_h = 0;
	union_w = 0;
	union_h = 0;

	if(draw == true) {
		ami_set_pointer(g->shared, GUI_POINTER_WAIT, false);
	} else {
		NSLOG(netsurf, INFO, "Ignoring deferred box redraw queue");
	}

	node = (struct nsObject *)GetHead((struct List *)g->deferred_rects);

	do {
		nnode = (struct nsObject *)GetSucc((struct Node *)node);
		if(draw == true) {
			rect = (struct rect *)node->objstruct;
			if(have_union == FALSE) {
				union_rect = *rect;
				have_union = TRUE;
			} else {
				if(rect->x0 < union_rect.x0)
					union_rect.x0 = rect->x0;
				if(rect->y0 < union_rect.y0)
					union_rect.y0 = rect->y0;
				if(rect->x1 > union_rect.x1)
					union_rect.x1 = rect->x1;
				if(rect->y1 > union_rect.y1)
					union_rect.y1 = rect->y1;
			}
		}
		ami_memory_itempool_free(g->deferred_rects_pool, node->objstruct,
					sizeof(struct rect));
		DelObjectNoFree(node);
	} while((node = nnode));

	if((draw == true) && (have_union == TRUE)) {
		/*
		 * Near-full unions re-blit almost everything strip-by-strip.
		 * Promote only when the union covers ~90% of both axes —
		 * half-view promotion was forcing a second full paint on
		 * news pages after images already filled most of the view.
		 */
		if(ami_gui_get_space_box((Object *)g->shared->objects[GID_BROWSER],
					&bbox) == NSERROR_OK) {
			view_w = bbox->Width;
			view_h = bbox->Height;
			ami_gui_free_space_box(bbox);
			union_w = union_rect.x1 - union_rect.x0;
			union_h = union_rect.y1 - union_rect.y0;
			if(view_w > 0 && view_h > 0 &&
			   (union_w * 10 >= view_w * 9) &&
			   (union_h * 10 >= view_h * 9)) {
				NSLOG(netsurf, INFO,
				      "Deferred union %ldx%ld >= 90%% view "
				      "%ldx%ld — full redraw",
				      (long)union_w, (long)union_h,
				      (long)view_w, (long)view_h);
				ami_schedule_redraw(g->shared, true);
				ami_reset_pointer(g->shared);
				return;
			}
		}
		ami_do_redraw_limits(g, g->bw, false,
			union_rect.x0, union_rect.y0,
			union_rect.x1, union_rect.y1);
		ami_reset_pointer(g->shared);
	}
}

bool ami_gui_window_update_box_deferred_check(struct MinList *deferred_rects,
				const struct rect *restrict new_rect, APTR mempool)
{
	struct nsObject *node;
	struct nsObject *nnode;
	struct rect *restrict rect;
	
	if(IsMinListEmpty(deferred_rects)) return true;

	node = (struct nsObject *)GetHead((struct List *)deferred_rects);

	do {
		nnode=(struct nsObject *)GetSucc((struct Node *)node);
		rect = (struct rect *)node->objstruct;
		
		if((rect->x0 <= new_rect->x0) &&
			(rect->y0 <= new_rect->y0) &&
			(rect->x1 >= new_rect->x1) &&
			(rect->y1 >= new_rect->y1)) {
			return false;
		}
		
		if ((new_rect->x0 <= rect->x0) &&
			(new_rect->y0 <= rect->y0) &&
			(new_rect->x1 >= rect->x1) &&
			(new_rect->y1 >= rect->y1)) {
			NSLOG(netsurf, INFO,
			      "Removing queued redraw that is a subset of new box redraw");
			ami_memory_itempool_free(mempool, node->objstruct, sizeof(struct rect));
			DelObjectNoFree(node);
			/* Don't return - we might find more */
		}
	} while((node = nnode));

	return true;
}


static void ami_do_redraw(struct gui_window_2 *gwin)
{
	ULONG hcurrent,vcurrent,xoffset,yoffset,width=800,height=600;
	struct IBox *bbox;
	ULONG oldh = gwin->oldh, oldv=gwin->oldv;

	if(browser_window_redraw_ready(gwin->gw->bw) == false) return;

	ami_get_hscroll_pos(gwin, (ULONG *)&hcurrent);
	ami_get_vscroll_pos(gwin, (ULONG *)&vcurrent);

	gwin->gw->scrollx = hcurrent;
	gwin->gw->scrolly = vcurrent;

	if(ami_gui_get_space_box((Object *)gwin->objects[GID_BROWSER], &bbox) != NSERROR_OK) {
		amiga_warn_user("NoMemory", "");
		return;
	}

	width=bbox->Width;
	height=bbox->Height;
	xoffset=bbox->Left;
	yoffset=bbox->Top;

	if(gwin->redraw_scroll)
	{
		if((abs(vcurrent-oldv) > height) ||	(abs(hcurrent-oldh) > width))
			gwin->redraw_scroll = false;

 		if(gwin->new_content) gwin->redraw_scroll = false;
	}

	if(gwin->redraw_scroll)
	{
		struct rect rect;
		
		gwin->gw->c_h_temp = gwin->gw->c_h;
		gui_window_remove_caret(gwin->gw);

		ScrollWindowRaster(gwin->win, hcurrent - oldh, vcurrent - oldv,
				xoffset, yoffset, xoffset + width - 1, yoffset + height - 1);

		gwin->gw->c_h = gwin->gw->c_h_temp;

		if(vcurrent>oldv) /* Going down */
		{
			ami_spacebox_to_ns_coords(gwin, &rect.x0, &rect.y0, 0, height - (vcurrent - oldv) - 1);
			ami_spacebox_to_ns_coords(gwin, &rect.x1, &rect.y1, width + 1, height + 1);
			amiga_window_invalidate_area(gwin->gw, &rect);
		}
		else if(vcurrent<oldv) /* Going up */
		{
			ami_spacebox_to_ns_coords(gwin, &rect.x0, &rect.y0, 0, 0);
			ami_spacebox_to_ns_coords(gwin, &rect.x1, &rect.y1, width + 1, oldv - vcurrent + 1);
			amiga_window_invalidate_area(gwin->gw, &rect);
		}

		if(hcurrent>oldh) /* Going right */
		{
			ami_spacebox_to_ns_coords(gwin, &rect.x0, &rect.y0, width - (hcurrent - oldh), 0);
			ami_spacebox_to_ns_coords(gwin, &rect.x1, &rect.y1, width + 1, height + 1);
			amiga_window_invalidate_area(gwin->gw, &rect);
		}
		else if(hcurrent<oldh) /* Going left */
		{
			ami_spacebox_to_ns_coords(gwin, &rect.x0, &rect.y0, 0, 0);
			ami_spacebox_to_ns_coords(gwin, &rect.x1, &rect.y1, oldh - hcurrent + 1, height + 1);
			amiga_window_invalidate_area(gwin->gw, &rect);
		}
	}
	else
	{
		struct redraw_context ctx = {
			.interactive = true,
			.background_images = true,
			.plot = &amiplot,
			.priv = browserglob
		};

		ami_do_redraw_tiled(gwin, true, hcurrent, vcurrent, width, height, hcurrent, vcurrent, bbox, &ctx);

		/* Tell NetSurf not to bother with the next queued box redraw, as we've redrawn everything. */
		ami_gui_window_update_box_deferred(gwin->gw, false);
	}

	ami_update_buttons(gwin);

	gwin->oldh = hcurrent;
	gwin->oldv = vcurrent;

	gwin->redraw_scroll = false;
	/* Keep interactive coalesce if strip invalidates are still queued */
	if(gwin->gw == NULL || IsMinListEmpty(gwin->gw->deferred_rects))
		gwin->redraw_interactive = false;
	gwin->redraw_required = false;
	gwin->new_content = false;

	ami_gui_free_space_box(bbox);
}


static void ami_get_hscroll_pos(struct gui_window_2 *gwin, ULONG *xs)
{
	if(gwin->objects[GID_HSCROLL])
	{
		GetAttr(gwin->ui_nami ? PGA_Top : SCROLLER_Top,
				(Object *)gwin->objects[GID_HSCROLL], xs);
		if(gwin->ui_nami)
			*xs <<= gwin->nami_hprop_shift;
	} else {
		*xs = 0;
	}
}

static void ami_get_vscroll_pos(struct gui_window_2 *gwin, ULONG *ys)
{
	if(gwin->objects[GID_VSCROLL]) {
		GetAttr(gwin->ui_nami ? PGA_Top : SCROLLER_Top,
				gwin->objects[GID_VSCROLL], ys);
		if(gwin->ui_nami)
			*ys <<= gwin->nami_vprop_shift;
	} else {
		*ys = 0;
	}
}

static bool gui_window_get_scroll(struct gui_window *g, int *restrict sx, int *restrict sy)
{
	ami_get_hscroll_pos(g->shared, (ULONG *)sx);
	ami_get_vscroll_pos(g->shared, (ULONG *)sy);

	return true;
}

/**
 * Set the scroll position of a amiga browser window.
 *
 * Scrolls the viewport to ensure the specified rectangle of the
 *   content is shown. The amiga implementation scrolls the contents so
 *   the specified point in the content is at the top of the viewport.
 *
 * \param g gui_window to scroll
 * \param rect The rectangle to ensure is shown.
 * \return NSERROR_OK on success or apropriate error code.
 */
static nserror
static gui_window_set_scroll(struct gui_window *g, const struct rect *rect)
{
	struct IBox *bbox;
	int width, height;
	nserror res;
	int sx = 0, sy = 0;
	ULONG vis_w, vis_h;

	if(!g) {
		return NSERROR_BAD_PARAMETER;
	}
	if(!g->bw || browser_window_has_content(g->bw) == false) {
		return NSERROR_BAD_PARAMETER;
	}

	res = ami_gui_get_space_box((Object *)g->shared->objects[GID_BROWSER], &bbox);
	if(res != NSERROR_OK) {
		amiga_warn_user("NoMemory", "");
		return res;
	}

	if (rect->x0 > 0) {
		sx = rect->x0;
	}
	if (rect->y0 > 0) {
		sy = rect->y0;
	}

	browser_window_get_extents(g->bw, false, &width, &height);

	vis_w = (ULONG)bbox->Width;
	vis_h = (ULONG)bbox->Height;

	if(sx >= width - bbox->Width)
		sx = width - bbox->Width;
	if(sy >= height - bbox->Height)
		sy = height - bbox->Height;

	if(width <= bbox->Width) sx = 0;
	if(height <= bbox->Height) sy = 0;

	ami_gui_free_space_box(bbox);

	if(g == g->shared->gw) {
		if(g->shared->objects[GID_VSCROLL]) {
			if(g->shared->ui_nami) {
				ami_gui_nami_set_prop(g->shared, g->shared->objects[GID_VSCROLL],
						&g->shared->nami_vprop_shift,
						(ULONG)height, vis_h,
						(ULONG)sy, TRUE);
			} else {
				RefreshSetGadgetAttrs((struct Gadget *)(APTR)g->shared->objects[GID_VSCROLL],
					g->shared->win, NULL,
					SCROLLER_Top, (ULONG)(sy),
				TAG_DONE);
			}
		}

		if(g->shared->objects[GID_HSCROLL])
		{
			if(g->shared->ui_nami) {
				ami_gui_nami_set_prop(g->shared, g->shared->objects[GID_HSCROLL],
						&g->shared->nami_hprop_shift,
						(ULONG)width, vis_w,
						(ULONG)sx, TRUE);
			} else {
				RefreshSetGadgetAttrs((struct Gadget *)(APTR)g->shared->objects[GID_HSCROLL],
					g->shared->win, NULL,
					SCROLLER_Top, (ULONG)(sx),
					TAG_DONE);
			}
		}

		ami_schedule_redraw_interactive(g->shared, true);
		g->shared->redraw_scroll = true;

		g->scrollx = sx;
		g->scrolly = sy;
	}
	return NSERROR_OK;
}

static void gui_window_set_status(struct gui_window *g, const char *text)
{
	char *utf8text;
	ULONG size;
	UWORD chars;
	struct TextExtent textex;
	ULONG secs;
	ULONG micros;

	if(!g) return;
	if(!text) return;
	if(g != g->shared->gw) return;

	utf8text = ami_utf8_easy((char *)text);
	if(utf8text == NULL) return;

	/* Nami: no bottom status gadget — promote useful text into the screen title.
	 * Skip the core "Done (n.ns)" flash; telemetry stays visible instead. */
	if(g->shared->ui_nami) {
		if(g->shared->status) ami_utf8_free(g->shared->status);
		g->shared->status = utf8text;
		if(ami_gui_status_is_done(text)) {
			g->shared->status_screentime = 0;
		} else {
			CurrentTime(&secs, &micros);
			g->shared->status_screentime = secs;
		}
		ami_gui_update_screentitle(g->shared);
		return;
	}

	if(!g->shared->objects[GID_STATUS]) {
		ami_utf8_free(utf8text);
		return;
	}

	GetAttr(GA_Width, g->shared->objects[GID_STATUS], (ULONG *)&size);
	chars = TextFit(&scrn->RastPort, utf8text, (UWORD)strlen(utf8text),
				&textex, NULL, 1, size - 4, scrn->RastPort.TxHeight);

	utf8text[chars] = 0;

	SetGadgetAttrs((struct Gadget *)g->shared->objects[GID_STATUS],
		g->shared->win, NULL,
		NSA_STATUS_TEXT, utf8text,
		TAG_DONE);

	RefreshGList((struct Gadget *)g->shared->objects[GID_STATUS],
			g->shared->win, NULL, 1);

	if(g->shared->status) ami_utf8_free(g->shared->status);
	g->shared->status = utf8text;
}

static nserror gui_window_set_url(struct gui_window *g, nsurl *url)
{
	size_t idn_url_l;
	char *idn_url_s = NULL;
	char *url_lc = NULL;

	if(!g) return NSERROR_OK;

	if(g == g->shared->gw) {
		if(nsoption_bool(display_decoded_idn) == true) {
			if (nsurl_get_utf8(url, &idn_url_s, &idn_url_l) == NSERROR_OK) {
				url_lc = ami_utf8_easy(idn_url_s);
			}
		}

		RefreshSetGadgetAttrs((struct Gadget *)g->shared->objects[GID_URL],
								g->shared->win, NULL,
								STRINGA_TextVal, url_lc ? url_lc : nsurl_access(url),
								TAG_DONE);

		if(url_lc) {
			ami_utf8_free(url_lc);
			if(idn_url_s) free(idn_url_s);
		}

		if(g->shared->ui_nami)
			ami_gui_update_screentitle(g->shared);
	}

	/* Nami tab button help shows the full URL */
	if(g->shared->ui_nami && url != NULL) {
		const char *uacc;
		const char *help;
		Object *btn;

		uacc = nsurl_access(url);
		if(g->sidebar_help != NULL) {
			free(g->sidebar_help);
			g->sidebar_help = NULL;
		}
		if(uacc != NULL)
			g->sidebar_help = strdup(uacc);
		help = (g->sidebar_help != NULL) ? g->sidebar_help :
				((g->tab_label != NULL) ? g->tab_label : "");
		if(g->sidebar_tab_slot >= 0 &&
		   g->sidebar_tab_slot < AMI_SIDE_TAB_MAX) {
			btn = g->shared->side_tab_btn[g->sidebar_tab_slot];
			if(btn != NULL) {
				if(g->shared->win != NULL) {
					SetGadgetAttrs((struct Gadget *)btn,
							g->shared->win, NULL,
							AMI_GA_HELP, help,
							TAG_DONE);
				} else {
					SetAttrs(btn, AMI_GA_HELP, help, TAG_DONE);
				}
			}
		}
	}

	ami_update_buttons(g->shared);

	return NSERROR_OK;
}

HOOKF(uint32, ami_set_favicon_render_hook, APTR, space, struct gpRender *)
{
	ami_schedule(0, ami_gui_refresh_favicon, hook->h_Data);
	return 0;
}

/**
 * Gui callback when search provider details are updated.
 *
 * \param provider_name The providers name.
 * \param ico_bitmap The icon bitmap representing the provider.
 * \return NSERROR_OK on success else error code.
 */
static nserror gui_search_web_provider_update(const char *provider_name,
	struct bitmap *ico_bitmap)
{
	struct BitMap *bm = NULL;
	struct nsObject *node;
	struct nsObject *nnode;
	struct gui_window_2 *gwin;

	if(IsMinListEmpty(window_list))	return NSERROR_BAD_PARAMETER;
	if(nsoption_bool(kiosk_mode) == true) return NSERROR_BAD_PARAMETER;

	if (ico_bitmap != NULL) {
		bm = ami_bitmap_get_native(ico_bitmap, 16, 16, ami_plot_screen_is_palettemapped(), NULL, nsoption_colour(sys_colour_ButtonFace));
	}

	if(bm == NULL) return NSERROR_BAD_PARAMETER;

	node = (struct nsObject *)GetHead((struct List *)window_list);

	do {
		nnode=(struct nsObject *)GetSucc((struct Node *)node);
		gwin = node->objstruct;

		if(node->Type == AMINS_WINDOW)
		{
			if(gwin->objects[GID_SEARCH_ICON] == NULL)
				continue;

			if(gwin->search_bm != NULL)
				DisposeObject(gwin->search_bm);

			ULONG bm_masking_tag = TAG_IGNORE;

			if(LIB_IS_AT_LEAST((struct Library *)ChooserBase, 53, 21)) {
				/* Broken in earlier versions */
				bm_masking_tag = BITMAP_Masking;
			}

			gwin->search_bm = BitMapObj,
						BITMAP_Screen, scrn,
						BITMAP_Width, 16,
						BITMAP_Height, 16,
						BITMAP_BitMap, bm,
						BITMAP_HasAlpha, TRUE,
						bm_masking_tag, TRUE,
					BitMapEnd;

			RefreshSetGadgetAttrs((struct Gadget *)gwin->objects[GID_SEARCH_ICON],
				gwin->win, NULL,
				AMI_GA_HELP, provider_name,
				GA_Image, gwin->search_bm,
				TAG_DONE);

		}
	} while((node = nnode));

	return NSERROR_OK;
}

HOOKF(uint32, ami_set_throbber_render_hook, APTR, space, struct gpRender *)
{
	struct gui_window_2 *gwin = hook->h_Data;
	ami_throbber_redraw_schedule(0, gwin->gw);
	return 0;
}

HOOKF(uint32, ami_gui_browser_render_hook, APTR, space, struct gpRender *)
{
	struct gui_window_2 *gwin = hook->h_Data;

	NSLOG(netsurf, DEBUG, "Render hook called with %ld (REDRAW=1)", msg->gpr_Redraw);

	if(msg->gpr_Redraw != GREDRAW_REDRAW) return 0;

	ami_schedule_redraw(gwin, true);

	return 0;
}

static void gui_window_place_caret(struct gui_window *g, int x, int y, int height,
		const struct rect *clip)
{
	struct IBox *bbox;
	int xs,ys;

	if(!g) return;

	gui_window_remove_caret(g);

	xs = g->scrollx;
	ys = g->scrolly;

	SetAPen(g->shared->win->RPort,3);

	if(ami_gui_get_space_box((Object *)g->shared->objects[GID_BROWSER], &bbox) != NSERROR_OK) {
		amiga_warn_user("NoMemory", "");
		return;
	}

	if((y-ys+height) > (bbox->Height)) height = bbox->Height-y+ys;

	if(((x-xs) <= 0) || ((x-xs+2) >= (bbox->Width)) || ((y-ys) <= 0) || ((y-ys) >= (bbox->Height))) {
		ami_gui_free_space_box(bbox);
		return;
	}

	g->c_w = 2;

	SetDrMd(g->shared->win->RPort,COMPLEMENT);
	RectFill(g->shared->win->RPort, x + bbox->Left - xs, y + bbox->Top - ys,
		x + bbox->Left + g->c_w - xs, y+bbox->Top + height - ys);
	SetDrMd(g->shared->win->RPort,JAM1);

	ami_gui_free_space_box(bbox);

	g->c_x = x;
	g->c_y = y;
	g->c_h = height;

	if((nsoption_bool(kiosk_mode) == false))
		ami_gui_menu_set_disabled(g->shared->win, g->shared->imenu, M_PASTE, false);
}

static void gui_window_remove_caret(struct gui_window *g)
{
	if(!g) return;
	if(g->c_h == 0) return;

	if((nsoption_bool(kiosk_mode) == false))
		ami_gui_menu_set_disabled(g->shared->win, g->shared->imenu, M_PASTE, true);

	ami_do_redraw_limits(g, g->bw, false, g->c_x, g->c_y,
		g->c_x + g->c_w + 1, g->c_y + g->c_h + 1);

	g->c_h = 0;
}

static void gui_window_new_content(struct gui_window *g)
{
	struct hlcache_handle *c;

	if(g && g->shared && g->bw && browser_window_has_content(g->bw))
		c = browser_window_get_content(g->bw);
	else return;

	ami_clearclipreg(browserglob);
	g->shared->new_content = true;
	g->scrollx = 0;
	g->scrolly = 0;
	g->shared->oldh = 0;
	g->shared->oldv = 0;
	g->favicon = NULL;
	ami_plot_release_pens(g->shared->shared_pens);
	ami_gui_menu_update_disabled(g, c);
	ami_gui_update_hotlist_button(g->shared);
	ami_gui_scroller_update(g->shared);
}

static bool gui_window_drag_start(struct gui_window *g, gui_drag_type type,
		const struct rect *rect)
{
#ifdef __amigaos4__
	g->shared->drag_op = type;
	if(rect) g->shared->ptr_lock = ami_ns_rect_to_ibox(g->shared, rect);

	if(type == GDRAGGING_NONE)
	{
		SetWindowAttrs(g->shared->win, WA_GrabFocus, 0,
			WA_MouseLimits, NULL, TAG_DONE);

		if(g->shared->ptr_lock)
		{
			free(g->shared->ptr_lock);
			g->shared->ptr_lock = NULL;
		}
	}
#endif
	return true;
}

/* return the text box at posn x,y in window coordinates
   x,y are updated to be document co-ordinates */

bool ami_text_box_at_point(struct gui_window_2 *gwin, ULONG *restrict x, ULONG *restrict y)
{
	struct IBox *bbox;
	ULONG xs, ys;
	struct browser_window_features data;

	if(ami_gui_get_space_box((Object *)gwin->objects[GID_BROWSER], &bbox) != NSERROR_OK) {
		amiga_warn_user("NoMemory", "");
		return false;
	}

	ami_get_hscroll_pos(gwin, (ULONG *)&xs);
	*x = *x - (bbox->Left) +xs;

	ami_get_vscroll_pos(gwin, (ULONG *)&ys);
	*y = *y - (bbox->Top) + ys;

	ami_gui_free_space_box(bbox);

	browser_window_get_features(gwin->gw->bw, *x, *y, &data);

	if (data.form_features == CTX_FORM_TEXT)
		return true;

	return false;
}

BOOL ami_gadget_hit(Object *obj, int x, int y)
{
	int top, left, width, height;
	struct Gadget *gad;

	if(obj == NULL)
		return FALSE;

	gad = (struct Gadget *)obj;
	left = gad->LeftEdge;
	top = gad->TopEdge;
	width = gad->Width;
	height = gad->Height;

	/* Prefer GetAttr when it agrees with a non-empty box; else raw edges. */
	GetAttrs(obj,
		GA_Left, &left,
		GA_Top, &top,
		GA_Width, &width,
		GA_Height, &height,
		TAG_DONE);
	if((width < 1 || height < 1) && gad->Width > 0 && gad->Height > 0) {
		left = gad->LeftEdge;
		top = gad->TopEdge;
		width = gad->Width;
		height = gad->Height;
	}

	if((x >= left) && (x <= (left + width)) &&
	   (y >= top) && (y <= (top + height)))
		return TRUE;
	return FALSE;
}

static Object *ami_gui_splash_open(void)
{
	/* Splash disabled for now — art/layout not ready */
	return NULL;
#if 0
	Object *restrict win_obj, *restrict bm_obj;
	struct Window *win;
	struct Screen *wbscreen = LockPubScreen("Workbench");
	uint32 top = 0, left = 0;
	struct TextAttr tattr;
	struct TextFont *tfont;

	win_obj = WindowObj,
#ifdef __amigaos4__
				WA_ToolBox, TRUE,
#endif
				WA_Borderless, TRUE,
				WA_BusyPointer, TRUE,
				WINDOW_Position, WPOS_CENTERSCREEN,
				WINDOW_LockWidth, TRUE,
				WINDOW_LockHeight, TRUE,
				WINDOW_ParentGroup, LayoutVObj,
					LAYOUT_AddImage, bm_obj = BitMapObj,
						BITMAP_SourceFile, "PROGDIR:Resources/splash.png",
						BITMAP_Screen, wbscreen,
						BITMAP_Precision, PRECISION_IMAGE,
					BitMapEnd,
				LayoutEnd,
			EndWindow;

	if(win_obj == NULL) {
		NSLOG(netsurf, INFO, "Splash window object not created");
		return NULL;
	}

	NSLOG(netsurf, INFO, "Attempting to open splash window...");
	win = RA_OpenWindow(win_obj);

	if(win == NULL) {
		NSLOG(netsurf, INFO, "Splash window did not open");
		return NULL;
	}

	if(bm_obj == NULL) {
		NSLOG(netsurf, INFO, "BitMap object not created");
		return NULL;
	}

	GetAttrs(bm_obj, IA_Top, &top,
				IA_Left, &left,
				TAG_DONE);

	SetDrMd(win->RPort, JAM1);
#ifdef __amigaos4__
	SetRPAttrs(win->RPort, RPTAG_APenColor, 0xFF3F6DFE, TAG_DONE);
	tattr.ta_Name = "DejaVu Serif Italic.font";
#else
	SetAPen(win->RPort, 3); /* Pen 3 is usually blue */
	tattr.ta_Name = "ruby.font";
#endif
	tattr.ta_YSize = 24;
	tattr.ta_Style = 0;
	tattr.ta_Flags = 0;

	if((tfont = ami_font_open_disk_font(&tattr)))
	{
		SetFont(win->RPort, tfont);
	}
	else
	{
		tattr.ta_Name = "DejaVu Serif Oblique.font";
		if((tfont = ami_font_open_disk_font(&tattr)))
			SetFont(win->RPort, tfont);
	}

	Move(win->RPort, left + 5, top + 25);
	Text(win->RPort, "Initialising...", strlen("Initialising..."));

	if(tfont) ami_font_close_disk_font(tfont);

#ifdef __amigaos4__
	tattr.ta_Name = "DejaVu Sans.font";
#else
	tattr.ta_Name = "helvetica.font";
#endif
	tattr.ta_YSize = 16;
	tattr.ta_Style = 0;
	tattr.ta_Flags = 0;

	if((tfont = ami_font_open_disk_font(&tattr)))
		SetFont(win->RPort, tfont);

	Move(win->RPort, left + 185, top + 220);
	Text(win->RPort, netsurf_version, strlen(netsurf_version));

	if(tfont) ami_font_close_disk_font(tfont);

	UnlockPubScreen(NULL, wbscreen);

	return win_obj;
#endif
}

static void ami_gui_splash_close(Object *win_obj)
{
	if(win_obj == NULL) return;

	NSLOG(netsurf, INFO, "Closing splash window");
	DisposeObject(win_obj);
}

static void gui_file_gadget_open(struct gui_window *g, struct hlcache_handle *hl, 
	struct form_control *gadget)
{
	NSLOG(netsurf, INFO, "File open dialog request for %p/%p", g, gadget);

	if(AslRequestTags(filereq,
			ASLFR_Window, g->shared->win,
			ASLFR_SleepWindow, TRUE,
			ASLFR_TitleText, messages_get("NetSurf"),
			ASLFR_Screen, scrn,
			ASLFR_DoSaveMode, FALSE,
			TAG_DONE)) {
		char fname[1024];
		strlcpy(fname, filereq->fr_Drawer, 1024);
		AddPart(fname, filereq->fr_File, 1024);
		browser_window_set_gadget_filename(g->bw, gadget, fname);
	}
}

/* exported function documented in amiga/gui.h */
uint32 ami_gui_get_app_id(void)
{
	return ami_appid;
}

/* Get current user directory for user-specific NetSurf data
 * Returns NULL on error
 */
static char *ami_gui_get_user_dir(STRPTR current_user)
{
	BPTR lock = 0;
	char temp[1024];
	int32 user = 0;

	if(current_user == NULL) {
		user = GetVar("user", temp, 1024, GVF_GLOBAL_ONLY);
		current_user = ASPrintf("%s", (user == -1) ? "Default" : temp);
	}
	NSLOG(netsurf, INFO, "User: %s", current_user);

	if(users_dir == NULL) {
		users_dir = ASPrintf("%s", USERS_DIR);
		if(users_dir == NULL) {
			ami_misc_fatal_error("Failed to allocate memory");
			FreeVec(current_user);
			return NULL;
		}
	}

	if(LIB_IS_AT_LEAST((struct Library *)DOSBase, 51, 96)) {
#ifdef __amigaos4__
		struct InfoData *infodata = AllocDosObject(DOS_INFODATA, 0);
		if(infodata == NULL) {
			ami_misc_fatal_error("Failed to allocate memory");
			FreeVec(current_user);
			return NULL;
		}
		GetDiskInfoTags(GDI_StringNameInput, users_dir,
					GDI_InfoData, infodata,
					TAG_DONE);
		if(infodata->id_DiskState == ID_DISKSTATE_WRITE_PROTECTED) {
			FreeDosObject(DOS_INFODATA, infodata);
			ami_misc_fatal_error("User directory MUST be on a writeable volume");
			FreeVec(current_user);
			return NULL;
		}
		FreeDosObject(DOS_INFODATA, infodata);
#endif
	} else {
		/* Classic Info() — InfoData must be longword-aligned */
		BPTR vol_lock;
		UBYTE info_buf[sizeof(struct InfoData) + 3];
		struct InfoData *infodata;

		infodata = (struct InfoData *)(((ULONG)info_buf + 3UL) & ~3UL);
		vol_lock = Lock(users_dir, SHARED_LOCK);
		if(vol_lock != 0) {
			if(Info(vol_lock, infodata) != 0) {
				if(infodata->id_DiskState == ID_WRITE_PROTECTED) {
					UnLock(vol_lock);
					ami_misc_fatal_error("User directory MUST be on a writeable volume");
					FreeVec(current_user);
					return NULL;
				}
			}
			UnLock(vol_lock);
		}
	}

	int len = strlen(current_user);
	len += strlen(users_dir);
	len += 2; /* for poss path sep and NULL term */

	current_user_dir = malloc(len);
	if(current_user_dir == NULL) {
		ami_misc_fatal_error("Failed to allocate memory");
		FreeVec(current_user);
		return NULL;
	}

	strlcpy(current_user_dir, users_dir, len);
	AddPart(current_user_dir, current_user, len);
	FreeVec(users_dir);
	FreeVec(current_user);

	NSLOG(netsurf, INFO, "User dir: %s", current_user_dir);

	if((lock = CreateDirTree(current_user_dir)))
		UnLock(lock);

	ami_nsoption_set_location(current_user_dir);

	current_user_faviconcache = ASPrintf("%s/IconCache", current_user_dir);
	if((lock = CreateDirTree(current_user_faviconcache))) UnLock(lock);

	return current_user_dir;
}


/**
 * process miscellaneous window events
 *
 * \param gw The window receiving the event.
 * \param event The event code.
 * \return NSERROR_OK when processed ok
 */
static nserror
gui_window_event(struct gui_window *gw, enum gui_window_event event)
{
	switch (event) {
	case GW_EVENT_UPDATE_EXTENT:
		gui_window_update_extent(gw);
		break;

	case GW_EVENT_REMOVE_CARET:
		gui_window_remove_caret(gw);
		break;

	case GW_EVENT_NEW_CONTENT:
		gui_window_new_content(gw);
		break;

	case GW_EVENT_START_SELECTION:
		gui_start_selection(gw);
		break;

	case GW_EVENT_START_THROBBER:
		gui_window_start_throbber(gw);
		break;

	case GW_EVENT_STOP_THROBBER:
		gui_window_stop_throbber(gw);
		break;

	case GW_EVENT_PAGE_INFO_CHANGE:
		gui_page_info_change(gw);
		break;

	default:
		break;
	}
	return NSERROR_OK;
}


static struct gui_window_table amiga_window_table = {
	.create = gui_window_create,
	.destroy = gui_window_destroy,
	.invalidate = amiga_window_invalidate_area,
	.get_scroll = gui_window_get_scroll,
	.set_scroll = gui_window_set_scroll,
	.get_dimensions = gui_window_get_dimensions,
	.event = gui_window_event,

	.set_icon = gui_window_set_icon,
	.set_title = gui_window_set_title,
	.set_url = gui_window_set_url,
	.set_status = gui_window_set_status,
	.place_caret = gui_window_place_caret,
	.drag_start = gui_window_drag_start,
	.create_form_select_menu = gui_create_form_select_menu,
	.file_gadget_open = gui_file_gadget_open,
	.drag_save_object = gui_drag_save_object,
	.drag_save_selection = gui_drag_save_selection,

	.console_log = gui_window_console_log,

	/* from theme */
	.set_pointer = gui_window_set_pointer,

	/* from download */
	.save_link = gui_window_save_link,
};


static struct gui_fetch_table amiga_fetch_table = {
	.filetype = fetch_filetype,

	.get_resource_url = gui_get_resource_url,
};

static struct gui_search_web_table amiga_search_web_table = {
	.provider_update = gui_search_web_provider_update,
};

static struct gui_misc_table amiga_misc_table = {
	.schedule = ami_schedule,

	.quit = gui_quit,
	.launch_url = gui_launch_url,
	.present_cookies = ami_cookies_present,
};

/** Normal entry point from OS */
int main(int argc, char** argv)
{
	setbuf(stderr, NULL);
	char messages[100];
	char script[1024];
	char temp[1024];
	STRPTR current_user_cache = NULL;
	STRPTR current_user = NULL;
	BPTR lock = 0;
	nserror ret;
	int nargc = 0;
	char **nargv = NULL;

	struct netsurf_table amiga_table = {
		.misc = &amiga_misc_table,
		.window = &amiga_window_table,
		.corewindow = amiga_core_window_table,
		.clipboard = amiga_clipboard_table,
		.download = amiga_download_table,
		.fetch = &amiga_fetch_table,
		.file = amiga_file_table,
		.utf8 = amiga_utf8_table,
		.search = amiga_search_table,
		.search_web = &amiga_search_web_table,
		.llcache = filesystem_llcache_table,
		.bitmap = amiga_bitmap_table,
		.layout = ami_layout_table,
	};

#ifdef __amigaos4__
	signal(SIGINT, SIG_IGN);
#endif
	ret = netsurf_register(&amiga_table);
	if (ret != NSERROR_OK) {
		ami_misc_fatal_error("Nami operation table failed registration");
		return RETURN_FAIL;
	}

	/* initialise logging. Not fatal if it fails but not much we
	 * can do about it either.
	 */
	nslog_init(NULL, &argc, argv);

#ifndef __amigaos4__
	/*
	 * OS3 lockup triage: if the user did not pass -v/-V, mirror INFO
	 * logs to PROGDIR:ns.log so Workbench launches still leave a trail.
	 * Shell: NetSurf -v   or   NetSurf -V RAM:ns.log
	 */
	if (verbose_log == false) {
		char *force_argv[4];
		int force_argc;

		/* Do not touch argv — from Workbench it is a WBStartup *, not char ** */
		force_argv[0] = (char *)"Nami";
		force_argv[1] = (char *)"-V";
		force_argv[2] = (char *)"PROGDIR:ns.log";
		force_argv[3] = NULL;
		force_argc = 3;
		nslog_init(NULL, &force_argc, force_argv);
	}
#endif

	/* Need to do this before opening any splash windows etc... */
	if ((ami_libs_open() == false)) {
		return RETURN_FAIL;
	}
	ami_gtdrag_init();

	/* Open splash window */
	Object *splash_window = ami_gui_splash_open();

#ifndef __amigaos4__
	/* OS3 low memory handler */
	struct Interupt *memhandler = ami_memory_init();
#endif

	if (ami_gui_resources_open() == false) { /* alloc msgports, objects and other miscelleny */
		ami_misc_fatal_error("Unable to allocate resources");
		ami_gui_splash_close(splash_window);
		ami_gtdrag_fini();
		ami_libs_close();
		return RETURN_FAIL;
	}

	current_user = ami_gui_read_all_tooltypes(argc, argv);
	char **args = ami_gui_commandline(&argc, argv, &nargc, &nargv);

	if(cli_rdargs_help_done) {
		if(args != NULL) {
			int hi;
			for(hi = 0; hi < nargc; hi++) {
				if(args[hi] != NULL)
					free(args[hi]);
			}
			free(args);
		}
		ami_gui_resources_free();
		ami_gui_splash_close(splash_window);
#ifndef __amigaos4__
		ami_memory_fini(memhandler);
#endif
		ami_libs_close();
		return RETURN_OK;
	}

	current_user_dir = ami_gui_get_user_dir(current_user);
	if(current_user_dir == NULL) {
		ami_gui_resources_free();
		ami_gui_splash_close(splash_window);
		ami_libs_close();
		return RETURN_FAIL;
	}

	ami_mime_init("PROGDIR:Resources/mimetypes");
	sprintf(temp, "%s/mimetypes.user", current_user_dir);
	ami_mime_init(temp);

#ifdef __amigaos4__
	amiga_plugin_hack_init();
#endif

	/* user options setup */
	ret = nsoption_init(ami_set_options, &nsoptions, &nsoptions_default);
	if (ret != NSERROR_OK) {
		ami_misc_fatal_error("Options failed to initialise");
		ami_gui_resources_free();
		ami_gui_splash_close(splash_window);
		ami_libs_close();
		return RETURN_FAIL;
	}
	ami_nsoption_read();
#ifndef __amigaos4__
	/* User options may re-enable a POSIX disc cache — keep it off on OS3 */
	nsoption_set_uint(disc_cache_size, 0);
	/*
	 * Options file often still has desktop defaults (12MB cache, 24
	 * fetchers). Cap after read so classic RAM is not exhausted.
	 */
	if (nsoption_int(memory_cache_size) > 768 * 1024) {
		nsoption_set_int(memory_cache_size, 768 * 1024);
	}
	if (nsoption_int(max_fetchers) > 8) {
		nsoption_set_int(max_fetchers, 8);
	}
	if (nsoption_int(max_fetchers_per_host) > 2) {
		nsoption_set_int(max_fetchers_per_host, 2);
	}
	if (nsoption_int(max_cached_fetch_handles) > 2) {
		nsoption_set_int(max_cached_fetch_handles, 2);
	}
	/*
	 * Old Choices may still have the desktop 25 cs reflow period.
	 * Floor at Amiga default (100 cs) so incremental layout is not
	 * thrashed while images arrive.
	 */
	if (nsoption_uint(min_reflow_period) < 100) {
		nsoption_set_uint(min_reflow_period, 100);
	}
#endif
	if(args != NULL) {
		nsoption_commandline(&nargc, nargv, NULL);

		for(int i = 0; i < nargc; i++) {
			if(args[i]) {
				NSLOG(netsurf, INFO, "Freeing fake arg %d: %s", i, args[i]);
				free(args[i]);
			}
		}
		NSLOG(netsurf, INFO, "Freeing fake arg array");
		free(args);
	}

	/* NAMI/NETSURF (or tooltypes) beat Prefs and --ui_style for this run */
	if(cli_ui_style_override == 0 || cli_ui_style_override == 1) {
		nsoption_set_int(ui_style, cli_ui_style_override);
		NSLOG(netsurf, INFO, "ui_style forced to %d for this launch",
		      cli_ui_style_override);
	}

	if (ami_locate_resource(messages, "Messages") == false) {
		ami_misc_fatal_error("Cannot open Messages file");
		ami_nsoption_free();
		nsoption_finalise(nsoptions, nsoptions_default);
		ami_gui_resources_free();
		ami_gui_splash_close(splash_window);
		ami_libs_close();
		return RETURN_FAIL;
	}

	ret = messages_add_from_file(messages);

	current_user_cache = ASPrintf("%s/Cache", current_user_dir);
	if((lock = CreateDirTree(current_user_cache))) UnLock(lock);

	NSLOG(netsurf, INFO, "Calling netsurf_init (cache=%s)",
	      current_user_cache ? (char *)current_user_cache : "(null)");
	ret = netsurf_init(current_user_cache);

	if(current_user_cache != NULL) FreeVec(current_user_cache);

	if (ret != NSERROR_OK) {
		ami_misc_fatal_error("Nami failed to initialise");
		ami_nsoption_free();
		nsoption_finalise(nsoptions, nsoptions_default);
		ami_gui_resources_free();
		ami_gui_splash_close(splash_window);
		ami_libs_close();
		return RETURN_FAIL;
	}
	NSLOG(netsurf, INFO, "netsurf_init OK");

#ifdef WITH_AMIGA_DATATYPES
	/* Register after netsurf_init so content_factory / lwc are live.
	 * Do not gate on datatypes.library v45 - OS3 picture.datatype works
	 * with NewDTObject; the old gate left image MIME types unregistered.
	 */
	ret = amiga_datatypes_init();
	if (ret != NSERROR_OK) {
		NSLOG(netsurf, WARNING,
		      "amiga_datatypes_init failed (%d) - images may not display",
		      (int)ret);
	}
#endif

	ret = amiga_icon_init();
	NSLOG(netsurf, INFO, "amiga_icon_init done");
	ret = amiga_ico_init();
	if (ret != NSERROR_OK) {
		NSLOG(netsurf, WARNING,
		      "amiga_ico_init failed (%d) - favicons may be blank",
		      (int)ret);
	} else {
		NSLOG(netsurf, INFO, "amiga_ico_init done");
	}

	search_web_init(nsoption_charp(search_engines_file));
	NSLOG(netsurf, INFO, "search_web_init done");
	ami_clipboard_init();
	ami_openurl_open();
	ami_amiupdate(); /* set env-vars for AmiUpdate */
	NSLOG(netsurf, INFO, "ami_font_init...");
	ami_font_init();
	NSLOG(netsurf, INFO, "ami_font_init done");
	NSLOG(netsurf, INFO, "save_complete_init...");
	save_complete_init();
	NSLOG(netsurf, INFO, "save_complete_init done");
	NSLOG(netsurf, INFO, "ami_theme_init...");
	ami_theme_init();
	NSLOG(netsurf, INFO, "ami_theme_init done");
	ami_init_mouse_pointers();
	NSLOG(netsurf, INFO, "mouse pointers done");
	ami_file_req_init();

	win_destroyed = false;
	ami_font_setdevicedpi(0); /* for early font requests, eg treeview init */

	window_list = NewObjList();

	urldb_load(nsoption_charp(url_file));
	urldb_load_cookies(nsoption_charp(cookie_file));

	NSLOG(netsurf, INFO, "gui_init2...");
	gui_init2(argc, argv);
	NSLOG(netsurf, INFO, "gui_init2 done");

	ami_ctxmenu_init(); /* Requires screen pointer */

	ami_gui_splash_close(splash_window);

	strlcpy(script, nsoption_charp(arexx_dir), 1024);
	AddPart(script, nsoption_charp(arexx_startup), 1024);
	ami_arexx_execute(script);

	NSLOG(netsurf, INFO, "Entering main loop");

	while (!ami_quit) {
		ami_get_msg();
	}

	strlcpy(script, nsoption_charp(arexx_dir), 1024);
	AddPart(script, nsoption_charp(arexx_shutdown), 1024);
	ami_arexx_execute(script);

	ami_mime_free();

	NSLOG(netsurf, INFO, "Leaving main loop, netsurf_exit");
	netsurf_exit();
	NSLOG(netsurf, INFO, "netsurf_exit returned");

	nsoption_finalise(nsoptions, nsoptions_default);
	ami_nsoption_free();
	free(current_user_dir);
	FreeVec(current_user_faviconcache);

	/* finalise logging */
	nslog_finalise();

#ifndef __amigaos4__
	/* OS3 low memory handler */
	ami_memory_fini(memhandler);
#endif

	ami_bitmap_fini();
	ami_gtdrag_fini();
	ami_libs_close();

	return RETURN_OK;
}

