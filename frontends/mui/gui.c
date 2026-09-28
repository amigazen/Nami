/*
 * Copyright 2026 amigazen project
 *
 * This file is part of NetSurf / Tsunami (MUI frontend).
 *
 * MUI Application host, shared chrome (nav / URL / Register tabs /
 * scrollbars / TLS / synthwave throbber), menu strip, gui_window_table.
 *
 * Tabs (MUI 3.8): RegisterGroup as in SDK Pages.c — Title.mui is MUI4/Odyssey
 * only. MUIA_Register_Titles is init-only, so extra tabs rebuild the Register
 * (pages RemMember'd, not disposed). New-tab / close-tab / secondary windows
 * run via tsunami_schedule(0) so rebuild/open happens outside button ReturnID.
 */

#include "mui/os3support.h"

#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/utility.h>
#include <proto/muimaster.h>
#include <libraries/mui.h>
#include <libraries/gadtools.h>
#include <intuition/imageclass.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sys/time.h>

#include "utils/log.h"
#include "utils/messages.h"
#include "utils/nsurl.h"
#include "utils/nsoption.h"
#include "netsurf/browser_window.h"
#include "netsurf/window.h"
#include "netsurf/mouse.h"
#include "desktop/browser_history.h"

#include "mui/gui.h"
#include "mui/browser.h"
#include "mui/throbber.h"
#include "mui/misc.h"
#include "mui/libs.h"
#include "mui/schedule.h"
#include "mui/theme.h"
#include "mui/file.h"
#include "mui/search.h"
#include "mui/hotlist.h"
#include "mui/history.h"
#include "mui/history_local.h"
#include "mui/cookies.h"
#include "mui/gui_options.h"
#include "mui/clipboard.h"
#include "mui/corewindow.h"
#include "mui/pageinfo.h"

#include "desktop/hotlist.h"
#include "netsurf/content.h"
#include "netsurf/keypress.h"

#include "content/fetchers/amihttp.h"

#define TSUNAMI_MAX_TABS 12

/**
 * Force-flush breadcrumb for new-tab lockup diagnosis.
 * NSLOG already fflushs; this adds a stable TABTRACE: prefix to grep.
 */
static void tabtrace(const char *msg)
{
	NSLOG(netsurf, INFO, "TABTRACE: %s", msg != NULL ? msg : "(null)");
}

static void tabtracef(const char *fmt, ULONG a, ULONG b)
{
	NSLOG(netsurf, INFO, "TABTRACE: %s a=%lu b=%lu",
	      fmt != NULL ? fmt : "?", (unsigned long)a, (unsigned long)b);
}

Object *tsunami_app;

static struct gui_window *gw_list;
static struct gui_window *gw_active;

/* Shared chrome (one MUI Window for all tabs). */
static Object *chrome_win;
static Object *chrome_root;	/* Outer VGroup */
static Object *chrome_pages;	/* RegisterGroup (MUI 3.8 tab control) */
static Object *chrome_statusbar;	/* Bottom status/throb row */
static Object *chrome_url;
static Object *chrome_status;
static Object *chrome_throb;	/* PageGroup: idle / gauge / busy */
static Object *chrome_gauge;
static Object *chrome_busy;
static Object *chrome_secure;	/* PageGroup: TLS / page-info theme icons */
static Object *chrome_back;
static Object *chrome_fwd;
static Object *chrome_stop_reload;	/* Combined Stop/Reload (Amiga Nami) */
static Object *chrome_stop_bm;	/* Art for Stop mode */
static Object *chrome_reload_bm;	/* Art for Reload mode */
static int chrome_sr_is_stop;	/* Nonzero when button shows Stop */
static int chrome_sr_text;	/* Nonzero when using text fallback */
static Object *chrome_home;
static Object *chrome_newtab;
static Object *chrome_closetab;
static Object *chrome_menustrip;
static int chrome_tab_count;
static char chrome_title_buf[TSUNAMI_MAX_TABS][48];
static STRPTR chrome_titles[TSUNAMI_MAX_TABS + 1];
/* MUI keeps MUIA_Window_Title as a pointer — must outlive the set() call. */
static char *chrome_wintitle;

/* Toolbar BitMaps live on the first gui_window that built chrome. */
static struct gui_window *chrome_owner;

/* ReturnIDs — menu IDs mirror Amiga gui_menu.h / MUI NetSurf mainmenu. */
enum {
	OID_BACK = 1,
	OID_FORWARD,
	OID_STOP_RELOAD,
	OID_HOME,
	OID_URL,
	OID_GO,
	OID_NEWTAB,
	OID_CLOSETAB,
	OID_TAB_SWITCH,
	MID_NEW_TAB,
	MID_CLOSE_TAB,
	MID_OPEN,
	MID_RELOAD,
	MID_HOME,
	MID_STOP,
	MID_BACK,
	MID_FORWARD,
	MID_ABOUT,
	MID_QUIT,
	MID_SAVE_SOURCE,
	MID_SAVE_TEXT,
	MID_PRINT,
	MID_CUT,
	MID_COPY,
	MID_PASTE,
	MID_SELALL,
	MID_FIND,
	MID_HIST_LOCAL,
	MID_HIST_GLOBAL,
	MID_COOKIES,
	MID_SCALE_DEC,
	MID_SCALE_NORM,
	MID_SCALE_INC,
	MID_REDRAW,
	MID_HOTLIST_ADD,
	MID_HOTLIST_SHOW,
	MID_PREFS,
	MID_MUI_PREFS
};

/* chrome_secure PageGroup indices (Voyager key bitmap / Amiga pageinfo). */
enum {
	SEC_UNKNOWN = 0,
	SEC_LOCAL,
	SEC_INSECURE,
	SEC_WARNING,
	SEC_SECURE
};

/* chrome_throb: always-visible KITT bar (or gauge fallback). */
enum {
	THROB_GAUGE = 0,
	THROB_BUSY
};

/* Recent layout/redraw activity — decays each throb tick. */
static int throb_layout_pulse;
static int throb_running;
static int throb_in_heavy;	/* paint / long work: bump activity */
static struct timeval throb_last_pulse;

/*
 * Full NetSurf-style strip (MUI NetSurf applicationclass.c + Amiga menu tree).
 * Handlers that need ASL/requesters are stubbed with a status line for now.
 */
static struct NewMenu tsunami_menu[] = {
	{ NM_TITLE, (STRPTR)"Project",		0, 0, 0, NULL },
	{ NM_ITEM,  (STRPTR)"N\0New tab",	0, 0, 0, (APTR)MID_NEW_TAB },
	{ NM_ITEM,  (STRPTR)"K\0Close tab",	0, 0, 0, (APTR)MID_CLOSE_TAB },
	{ NM_ITEM,  NM_BARLABEL,		0, 0, 0, NULL },
	{ NM_ITEM,  (STRPTR)"O\0Open...",	0, 0, 0, (APTR)MID_OPEN },
	{ NM_ITEM,  NM_BARLABEL,		0, 0, 0, NULL },
	{ NM_ITEM,  (STRPTR)"R\0Reload",	0, 0, 0, (APTR)MID_RELOAD },
	{ NM_ITEM,  (STRPTR)"H\0Home",		0, 0, 0, (APTR)MID_HOME },
	{ NM_ITEM,  (STRPTR)"S\0Stop",		0, 0, 0, (APTR)MID_STOP },
	{ NM_ITEM,  NM_BARLABEL,		0, 0, 0, NULL },
	{ NM_ITEM,  (STRPTR)"Save as...",	0, 0, 0, NULL },
	{ NM_SUB,   (STRPTR)"Source...",	0, 0, 0, (APTR)MID_SAVE_SOURCE },
	{ NM_SUB,   (STRPTR)"Text...",		0, 0, 0, (APTR)MID_SAVE_TEXT },
	{ NM_ITEM,  NM_BARLABEL,		0, 0, 0, NULL },
	{ NM_ITEM,  (STRPTR)"P\0Print...",	0, 0, 0, (APTR)MID_PRINT },
	{ NM_ITEM,  NM_BARLABEL,		0, 0, 0, NULL },
	{ NM_ITEM,  (STRPTR)"About...",		0, 0, 0, (APTR)MID_ABOUT },
	{ NM_ITEM,  NM_BARLABEL,		0, 0, 0, NULL },
	{ NM_ITEM,  (STRPTR)"Q\0Quit",		0, 0, 0, (APTR)MID_QUIT },

	{ NM_TITLE, (STRPTR)"Edit",		0, 0, 0, NULL },
	{ NM_ITEM,  (STRPTR)"X\0Cut",		0, 0, 0, (APTR)MID_CUT },
	{ NM_ITEM,  (STRPTR)"C\0Copy",		0, 0, 0, (APTR)MID_COPY },
	{ NM_ITEM,  (STRPTR)"V\0Paste",		0, 0, 0, (APTR)MID_PASTE },
	{ NM_ITEM,  NM_BARLABEL,		0, 0, 0, NULL },
	{ NM_ITEM,  (STRPTR)"A\0Select all",	0, 0, 0, (APTR)MID_SELALL },
	{ NM_ITEM,  NM_BARLABEL,		0, 0, 0, NULL },
	{ NM_ITEM,  (STRPTR)"F\0Find...",	0, 0, 0, (APTR)MID_FIND },

	{ NM_TITLE, (STRPTR)"Browser",		0, 0, 0, NULL },
	{ NM_ITEM,  (STRPTR)"B\0Back",		0, 0, 0, (APTR)MID_BACK },
	{ NM_ITEM,  (STRPTR)"W\0Forward",	0, 0, 0, (APTR)MID_FORWARD },
	{ NM_ITEM,  NM_BARLABEL,		0, 0, 0, NULL },
	{ NM_ITEM,  (STRPTR)"Local history...",	0, 0, 0, (APTR)MID_HIST_LOCAL },
	{ NM_ITEM,  (STRPTR)"Global history...",0, 0, 0, (APTR)MID_HIST_GLOBAL },
	{ NM_ITEM,  NM_BARLABEL,		0, 0, 0, NULL },
	{ NM_ITEM,  (STRPTR)"Cookies...",	0, 0, 0, (APTR)MID_COOKIES },
	{ NM_ITEM,  NM_BARLABEL,		0, 0, 0, NULL },
	{ NM_ITEM,  (STRPTR)"Scale",		0, 0, 0, NULL },
	{ NM_SUB,   (STRPTR)"-\0Smaller",	0, 0, 0, (APTR)MID_SCALE_DEC },
	{ NM_SUB,   (STRPTR)"=\0Normal",	0, 0, 0, (APTR)MID_SCALE_NORM },
	{ NM_SUB,   (STRPTR)"+\0Larger",	0, 0, 0, (APTR)MID_SCALE_INC },
	{ NM_ITEM,  NM_BARLABEL,		0, 0, 0, NULL },
	{ NM_ITEM,  (STRPTR)"Redraw",		0, 0, 0, (APTR)MID_REDRAW },

	{ NM_TITLE, (STRPTR)"Hotlist",		0, 0, 0, NULL },
	{ NM_ITEM,  (STRPTR)"Add to hotlist",	0, 0, 0, (APTR)MID_HOTLIST_ADD },
	{ NM_ITEM,  (STRPTR)"Show hotlist...",	0, 0, 0, (APTR)MID_HOTLIST_SHOW },

	{ NM_TITLE, (STRPTR)"Settings",		0, 0, 0, NULL },
	{ NM_ITEM,  (STRPTR)"Preferences...",	0, 0, 0, (APTR)MID_PREFS },
	{ NM_ITEM,  (STRPTR)"MUI...",		0, 0, 0, (APTR)MID_MUI_PREFS },

	{ NM_END,   NULL,			0, 0, 0, NULL }
};

static struct gui_window *active_gw(void);
static bool gw_still_alive(struct gui_window *gw);
static void deferred_close_tab(void *p);
static void request_close_tab(struct gui_window *gw);
static void open_new_tab(void);

/*
 * Secondary MUI windows must not be created/opened while Application_NewInput
 * is still on the stack (PushMethod ReturnIDs still nest there). schedule(0)
 * runs from tsunami_gui_run after NewInput returns — safe for OM_ADDMEMBER /
 * Window_Open / ASL / Register rebuild.
 */
static bool gw_still_alive(struct gui_window *gw)
{
	struct gui_window *t;

	if (gw == NULL) {
		return false;
	}
	for (t = gw_list; t != NULL; t = t->next) {
		if (t == gw) {
			return true;
		}
	}
	return false;
}

static void deferred_open_find(void *p)
{
	struct gui_window *gw;

	gw = (struct gui_window *)p;
	if (gw == NULL) {
		gw = active_gw();
	}
	if (gw_still_alive(gw)) {
		NSLOG(netsurf, INFO, "deferred_open_find");
		ami_search_open(gw);
	}
}

static void deferred_open_hist_local(void *p)
{
	struct gui_window *gw;

	gw = (struct gui_window *)p;
	if (gw == NULL) {
		gw = active_gw();
	}
	if (gw_still_alive(gw)) {
		NSLOG(netsurf, INFO, "deferred_open_hist_local");
		ami_history_local_present(gw);
	}
}

static void deferred_open_hist_global(void *p)
{
	(void)p;
	NSLOG(netsurf, INFO, "deferred_open_hist_global");
	ami_history_global_present();
}

static void deferred_open_cookies(void *p)
{
	(void)p;
	NSLOG(netsurf, INFO, "deferred_open_cookies");
	ami_cookies_present(NULL);
}

static void deferred_open_hotlist(void *p)
{
	(void)p;
	NSLOG(netsurf, INFO, "deferred_open_hotlist");
	ami_hotlist_present();
}

static void deferred_open_prefs(void *p)
{
	(void)p;
	NSLOG(netsurf, INFO, "deferred_open_prefs");
	ami_gui_opts_open();
}

static void deferred_new_tab(void *p)
{
	(void)p;
	tabtrace("deferred_new_tab");
	open_new_tab();
}

static void deferred_open_mui_prefs(void *p)
{
	(void)p;
	if (tsunami_app != NULL) {
		DoMethod(tsunami_app, MUIM_Application_OpenConfigWindow, 0);
	}
}

static void deferred_file_open(void *p)
{
	struct gui_window *gw;

	gw = (struct gui_window *)p;
	if (gw == NULL) {
		gw = active_gw();
	}
	if (gw_still_alive(gw)) {
		ami_file_open(gw);
	}
}

struct deferred_save {
	struct gui_window *gw;
	int type;
};

static void deferred_file_save(void *p)
{
	struct deferred_save *ds;

	ds = (struct deferred_save *)p;
	if (ds == NULL) {
		return;
	}
	if (gw_still_alive(ds->gw) && ds->gw->bw != NULL &&
	    browser_window_has_content(ds->gw->bw)) {
		ami_file_save_req(ds->type, ds->gw,
				  browser_window_get_content(ds->gw->bw));
	}
	free(ds);
}

static void request_file_save(struct gui_window *gw, int type)
{
	struct deferred_save *ds;

	if (gw == NULL) {
		return;
	}
	ds = malloc(sizeof(*ds));
	if (ds == NULL) {
		return;
	}
	ds->gw = gw;
	ds->type = type;
	tsunami_schedule(0, deferred_file_save, ds);
}

static void prop_nnset(Object *obj, ULONG attr, ULONG val)
{
	if (obj == NULL) {
		return;
	}
	set(obj, MUIA_NoNotify, TRUE);
	set(obj, attr, val);
	set(obj, MUIA_NoNotify, FALSE);
}

static void status_note(struct gui_window *gw, const char *msg)
{
	if (gw != NULL && gw->status != NULL && msg != NULL) {
		set(gw->status, MUIA_Text_Contents, msg);
	} else if (chrome_status != NULL && msg != NULL) {
		set(chrome_status, MUIA_Text_Contents, msg);
	}
}

static void gw_remember_bm(struct gui_window *gw, Object *bmo)
{
	if (gw == NULL || bmo == NULL) {
		return;
	}
	if (gw->tb_bm_count < (int)(sizeof(gw->tb_bm) / sizeof(gw->tb_bm[0]))) {
		gw->tb_bm[gw->tb_bm_count] = bmo;
		gw->tb_bm_count++;
	}
}

static Object *make_text_tb_button(const char *label, const char *help)
{
	return TextObject,
		ButtonFrame,
		MUIA_Font, MUIV_Font_Button,
		MUIA_Text_Contents, label,
		MUIA_Text_PreParse, "\33c",
		MUIA_InputMode, MUIV_InputMode_RelVerify,
		MUIA_Background, MUII_ButtonBack,
		MUIA_CycleChain, TRUE,
		MUIA_ShortHelp, help,
		MUIA_Weight, 0,
	End;
}

static Object *theme_bm_load(struct gui_window *gw, const char *theme_key)
{
	Object *bmo;
	struct Image *im;
	struct Screen *scrn;
	BOOL locked;

	scrn = ami_gui_get_screen();
	locked = FALSE;
	if (scrn == NULL) {
		scrn = LockPubScreen(NULL);
		locked = TRUE;
	}
	bmo = ami_gui_theme_bitmap(scrn, theme_key, NULL);
	if (locked && scrn != NULL) {
		UnlockPubScreen(NULL, scrn);
	}
	im = (struct Image *)bmo;
	if (bmo == NULL || im == NULL || im->Width < 1 || im->Height < 1) {
		if (bmo != NULL) {
			DisposeObject(bmo);
		}
		return NULL;
	}
	gw_remember_bm(gw, bmo);
	return bmo;
}

static Object *make_image_tb_button(Object *bmo, const char *help)
{
	if (bmo == NULL) {
		return NULL;
	}
	return ImageObject,
		ButtonFrame,
		MUIA_InputMode, MUIV_InputMode_RelVerify,
		MUIA_Background, MUII_ButtonBack,
		MUIA_Image_OldImage, (struct Image *)bmo,
		MUIA_ShortHelp, help,
		MUIA_CycleChain, TRUE,
		MUIA_Weight, 0,
	End;
}

static Object *make_tb_button(struct gui_window *gw,
			       const char *theme_key, const char *label,
			       const char *help)
{
	Object *bmo;
	Object *btn;

	bmo = theme_bm_load(gw, theme_key);
	btn = make_image_tb_button(bmo, help);
	if (btn != NULL) {
		return btn;
	}
	return make_text_tb_button(label, help);
}

/**
 * Combined Stop/Reload control — same behaviour as Amiga Nami GID_RELOAD.
 * While loading: Stop art + stop action. Idle: Reload art + reload action.
 */
static Object *make_stop_reload_button(struct gui_window *gw)
{
	Object *btn;

	chrome_stop_bm = theme_bm_load(gw, "theme_stop");
	chrome_reload_bm = theme_bm_load(gw, "theme_reload");
	chrome_sr_is_stop = 0;
	chrome_sr_text = 0;

	if (chrome_stop_bm != NULL && chrome_reload_bm != NULL) {
		btn = make_image_tb_button(chrome_reload_bm, "Reload");
		if (btn != NULL) {
			return btn;
		}
	}
	chrome_sr_text = 1;
	return make_text_tb_button("Reload", "Reload");
}

/**
 * Mirror ami_update_buttons: ghost Back/Forward from history availability,
 * and flip the combined Stop/Reload slot from browser_window_stop/reload_available.
 */
static void update_nav_buttons(void)
{
	struct gui_window *gw;
	BOOL can_back;
	BOOL can_fwd;
	BOOL can_stop;
	BOOL can_reload;
	BOOL want_stop;

	gw = active_gw();
	can_back = FALSE;
	can_fwd = FALSE;
	can_stop = FALSE;
	can_reload = FALSE;
	if (gw != NULL && gw->bw != NULL) {
		can_back = browser_window_back_available(gw->bw) ? TRUE : FALSE;
		can_fwd = browser_window_forward_available(gw->bw) ? TRUE : FALSE;
		can_stop = browser_window_stop_available(gw->bw) ? TRUE : FALSE;
		can_reload = browser_window_reload_available(gw->bw) ? TRUE : FALSE;
	}

	if (chrome_back != NULL) {
		set(chrome_back, MUIA_Disabled, can_back ? FALSE : TRUE);
	}
	if (chrome_fwd != NULL) {
		set(chrome_fwd, MUIA_Disabled, can_fwd ? FALSE : TRUE);
	}

	want_stop = can_stop;
	if (chrome_stop_reload != NULL) {
		if (want_stop && !chrome_sr_is_stop) {
			if (chrome_sr_text) {
				set(chrome_stop_reload, MUIA_Text_Contents, "Stop");
			} else if (chrome_stop_bm != NULL) {
				set(chrome_stop_reload, MUIA_Image_OldImage,
				    (struct Image *)chrome_stop_bm);
			}
			set(chrome_stop_reload, MUIA_ShortHelp, "Stop");
			chrome_sr_is_stop = 1;
		} else if (!want_stop && chrome_sr_is_stop) {
			if (chrome_sr_text) {
				set(chrome_stop_reload, MUIA_Text_Contents, "Reload");
			} else if (chrome_reload_bm != NULL) {
				set(chrome_stop_reload, MUIA_Image_OldImage,
				    (struct Image *)chrome_reload_bm);
			}
			set(chrome_stop_reload, MUIA_ShortHelp, "Reload");
			chrome_sr_is_stop = 0;
		}
		if (want_stop) {
			set(chrome_stop_reload, MUIA_Disabled, FALSE);
		} else {
			set(chrome_stop_reload, MUIA_Disabled,
			    can_reload ? FALSE : TRUE);
		}
	}

	/*
	 * MUI 3.8 Register has no per-tab close gadget (unlike ClickTab /
	 * Title.mui). Close sits next to New tab, like Amiga Nami sidebar.
	 */
	if (chrome_closetab != NULL) {
		set(chrome_closetab, MUIA_Disabled,
		    (chrome_tab_count <= 1) ? TRUE : FALSE);
	}
}

static Object *make_url_gadget(void)
{
	Object *obj;

	obj = MUI_NewObject("Textinput.mcc",
			    MUIA_CycleChain, TRUE,
			    MUIA_Frame, MUIV_Frame_String,
			    TAG_DONE);
	if (obj != NULL) {
		return obj;
	}

	return StringObject,
		MUIA_CycleChain, TRUE,
		MUIA_Frame, MUIV_Frame_String,
		StringFrame,
		MUIA_String_MaxLen, 2048,
	End;
}

static const char *url_gadget_contents(Object *url)
{
	const char *s;

	s = NULL;
	get(url, MUIA_String_Contents, &s);
	return s;
}

void tsunami_gui_go_url(struct gui_window *gw, const char *urltext)
{
	nsurl *url;
	nserror error;

	if (gw == NULL || gw->bw == NULL || urltext == NULL || urltext[0] == '\0') {
		return;
	}

	error = nsurl_create(urltext, &url);
	if (error != NSERROR_OK) {
		return;
	}

	browser_window_navigate(gw->bw, url, NULL,
				BW_NAVIGATE_HISTORY,
				NULL, NULL, NULL);
	nsurl_unref(url);
}

static void url_go_func(struct gui_window *gw)
{
	const char *text;

	if (gw == NULL || gw->url == NULL) {
		return;
	}
	text = url_gadget_contents(gw->url);
	tsunami_gui_go_url(gw, text);
}

/**
 * Amiga-style scrollbar sync: Prop Entries = doc size, Visible = viewport.
 */
void tsunami_gui_sync_scrollbars(struct gui_window *gw)
{
	int width;
	int height;
	ULONG vis_w;
	ULONG vis_h;

	if (gw == NULL || gw->bw == NULL) {
		return;
	}
	if (browser_window_has_content(gw->bw) == false) {
		return;
	}

	browser_window_get_extents(gw->bw, true, &width, &height);
	gw->doc_w = width;
	gw->doc_h = height;

	vis_w = (ULONG)(gw->width > 0 ? gw->width : 1);
	vis_h = (ULONG)(gw->height > 0 ? gw->height : 1);

	if (gw->vbar != NULL) {
		prop_nnset(gw->vbar, MUIA_Prop_Entries, (ULONG)height);
		prop_nnset(gw->vbar, MUIA_Prop_Visible, vis_h);
		if (gw->scrolly > height - (int)vis_h) {
			gw->scrolly = height - (int)vis_h;
			if (gw->scrolly < 0) {
				gw->scrolly = 0;
			}
			prop_nnset(gw->vbar, MUIA_Prop_First, (ULONG)gw->scrolly);
		}
	}
	if (gw->hbar != NULL) {
		prop_nnset(gw->hbar, MUIA_Prop_Entries, (ULONG)width);
		prop_nnset(gw->hbar, MUIA_Prop_Visible, vis_w);
		if (gw->scrollx > width - (int)vis_w) {
			gw->scrollx = width - (int)vis_w;
			if (gw->scrollx < 0) {
				gw->scrollx = 0;
			}
			prop_nnset(gw->hbar, MUIA_Prop_First, (ULONG)gw->scrollx);
		}
	}
}

/**
 * Activity score → eye speed. Network fetches + tab loading + layout pulses.
 */
static int throb_activity_score(void)
{
	struct gui_window *g;
	int score;
	ULONG active;
	ULONG queued;

	score = 0;
	active = 0;
	queued = 0;

	for (g = gw_list; g != NULL; g = g->next) {
		if (g->loading) {
			score += 3;
		}
	}

#ifdef WITH_AMIHTTP
	fetch_amihttp_counts(&active, &queued);
	score += (int)(active * 2UL + queued);
#else
	(void)active;
	(void)queued;
#endif

	if (throb_layout_pulse > 0) {
		score += throb_layout_pulse;
	}
	return score;
}

/** ms between frames — busier = snappier. */
static int throb_interval_ms(int activity)
{
	if (activity <= 0) {
		return 70;	/* decay-only fade to black */
	}
	if (activity <= 2) {
		return 85;
	}
	if (activity <= 5) {
		return 55;
	}
	if (activity <= 9) {
		return 40;
	}
	return 28;
}

static void throbber_tick(void *p)
{
	int activity;
	int keep;

	(void)p;
	if (chrome_busy == NULL) {
		throb_running = 0;
		return;
	}

	activity = throb_activity_score();
	if (throb_layout_pulse > 0) {
		throb_layout_pulse--;
	}

	keep = tsunami_throbber_tick(chrome_busy, activity);
	if (keep) {
		tsunami_schedule(throb_interval_ms(activity), throbber_tick,
				 NULL);
		throb_running = 1;
	} else {
		throb_running = 0;
	}
}

static void throbber_kick(void)
{
	if (chrome_busy == NULL) {
		return;
	}
	if (throb_running) {
		return;
	}
	throb_running = 1;
	tsunami_schedule(0, throbber_tick, NULL);
}

/**
 * Wall-clock gated frame for when the event loop is stuck in paint/layout.
 */
void tsunami_gui_throbber_pulse(void)
{
	struct timeval now;
	long elapsed_ms;
	int activity;
	int keep;

	if (chrome_busy == NULL) {
		return;
	}

	gettimeofday(&now, NULL);
	if (throb_last_pulse.tv_sec != 0 || throb_last_pulse.tv_usec != 0) {
		elapsed_ms = (now.tv_sec - throb_last_pulse.tv_sec) * 1000L +
			     (now.tv_usec - throb_last_pulse.tv_usec) / 1000L;
		if (elapsed_ms >= 0 && elapsed_ms < 35L) {
			return;
		}
	}
	throb_last_pulse = now;

	activity = throb_activity_score();
	if (throb_in_heavy) {
		activity += 5;
	}
	keep = tsunami_throbber_tick(chrome_busy, activity);
	if (keep && !throb_running) {
		throbber_kick();
	}
}

void tsunami_gui_throbber_heavy_begin(void)
{
	throb_in_heavy++;
	tsunami_gui_throbber_pulse();
}

void tsunami_gui_throbber_heavy_end(void)
{
	if (throb_in_heavy > 0) {
		throb_in_heavy--;
	}
	tsunami_gui_throbber_pulse();
}

static void set_throbber(struct gui_window *gw, int on)
{
	gw->loading = on;
	if (chrome_busy != NULL) {
		throbber_kick();
		return;
	}
	/* Gauge fallback when custom class unavailable. */
	if (chrome_throb == NULL || chrome_gauge == NULL) {
		return;
	}
	if (on) {
		set(chrome_throb, MUIA_Group_ActivePage, THROB_GAUGE);
		set(chrome_gauge, MUIA_Gauge_Current, 50);
		set(chrome_gauge, MUIA_Gauge_InfoText, "Loading...");
	} else {
		set(chrome_gauge, MUIA_Gauge_Current, 0);
		set(chrome_gauge, MUIA_Gauge_InfoText, "");
	}
}

static void throb_note_layout(void)
{
	throb_layout_pulse += 4;
	if (throb_layout_pulse > 16) {
		throb_layout_pulse = 16;
	}
	throbber_kick();
}

static void update_page_info(struct gui_window *gw)
{
	browser_window_page_info_state st;
	ULONG page;

	if (gw == NULL || gw->bw == NULL || chrome_secure == NULL) {
		return;
	}
	if (gw != gw_active && gw_active != NULL) {
		return;
	}

	st = browser_window_get_page_info_state(gw->bw);
	switch (st) {
	case PAGE_STATE_LOCAL:
		page = SEC_LOCAL;
		break;
	case PAGE_STATE_INSECURE:
		page = SEC_INSECURE;
		break;
	case PAGE_STATE_SECURE_OVERRIDE:
	case PAGE_STATE_SECURE_ISSUES:
		page = SEC_WARNING;
		break;
	case PAGE_STATE_SECURE:
		page = SEC_SECURE;
		break;
	case PAGE_STATE_INTERNAL:
	case PAGE_STATE_UNKNOWN:
	default:
		page = SEC_UNKNOWN;
		break;
	}
	set(chrome_secure, MUIA_Group_ActivePage, page);
}

static void activate_tab(struct gui_window *gw)
{
	ULONG cur;

	if (gw == NULL) {
		return;
	}
	gw_active = gw;

	if (gw->win == chrome_win && chrome_pages != NULL) {
		cur = ~(ULONG)0;
		get(chrome_pages, MUIA_Group_ActivePage, &cur);
		if (cur != (ULONG)gw->tab_index) {
			prop_nnset(chrome_pages, MUIA_Group_ActivePage,
				   (ULONG)gw->tab_index);
		}
	}

	if (gw->url != NULL && gw->bw != NULL &&
	    browser_window_has_content(gw->bw)) {
		nsurl *u;

		u = NULL;
		if (browser_window_get_url(gw->bw, true, &u) == NSERROR_OK &&
		    u != NULL) {
			set(gw->url, MUIA_String_Contents, nsurl_access(u));
			nsurl_unref(u);
		}
	}
	update_page_info(gw);
	update_nav_buttons();
}

static struct gui_window *active_gw(void)
{
	if (gw_active != NULL) {
		return gw_active;
	}
	return gw_list;
}

struct gui_window *tsunami_gui_get_active(void)
{
	return active_gw();
}

/**
 * Build viewport + Amiga-style Prop scrollbars for one tab page.
 * Prop notifies are wired after the page is in the Register tree.
 */
static Object *make_browser_page(struct gui_window *gw)
{
	Object *browser;
	Object *hbar;
	Object *vbar;
	Object *page;

	browser = NULL;
	if (TsunamiBrowserClass != NULL &&
	    TsunamiBrowserClass->mcc_Class != NULL) {
		browser = NewObject(TsunamiBrowserClass->mcc_Class, NULL,
				    MUIA_FillArea, FALSE,
				    MUIA_Weight, 100,
				    MUIA_Frame, MUIV_Frame_Virtual,
				    MUIA_Background,
				    "2:FFFFFFFF,FFFFFFFF,FFFFFFFF:",
				    MUIA_Tsunami_Browser_GuiWindow, (ULONG)gw,
				    MUIA_InnerLeft, 0,
				    MUIA_InnerTop, 0,
				    MUIA_InnerRight, 0,
				    MUIA_InnerBottom, 0,
				    TAG_DONE);
	}
	if (browser == NULL) {
		browser = RectangleObject,
			MUIA_Weight, 100,
			MUIA_Background, MUII_BACKGROUND,
			MUIA_Frame, MUIV_Frame_Virtual,
		End;
	}

	hbar = ScrollbarObject,
		MUIA_Group_Horiz, TRUE,
		MUIA_Prop_Entries, 1,
		MUIA_Prop_Visible, 1,
		MUIA_Prop_First, 0,
	End;

	vbar = ScrollbarObject,
		MUIA_Group_Horiz, FALSE,
		MUIA_Prop_Entries, 1,
		MUIA_Prop_Visible, 1,
		MUIA_Prop_First, 0,
	End;

	page = HGroup,
		MUIA_Group_Spacing, 0,
		Child, VGroup,
			MUIA_Group_Spacing, 0,
			Child, browser,
			Child, hbar,
		End,
		Child, vbar,
	End;

	gw->browser = browser;
	gw->hbar = hbar;
	gw->vbar = vbar;
	gw->page = page;
	gw->tab_btn = NULL;

	return page;
}

static void wire_scroll_notifies(struct gui_window *gw)
{
	if (gw == NULL || gw->browser == NULL) {
		return;
	}
	if (gw->hbar != NULL) {
		DoMethod(gw->hbar, MUIM_KillNotify, MUIA_Prop_First);
		DoMethod(gw->hbar, MUIM_Notify, MUIA_Prop_First, MUIV_EveryTime,
			 gw->browser, 1, MUIM_Tsunami_Browser_SyncScroll);
	}
	if (gw->vbar != NULL) {
		DoMethod(gw->vbar, MUIM_KillNotify, MUIA_Prop_First);
		DoMethod(gw->vbar, MUIM_Notify, MUIA_Prop_First, MUIV_EveryTime,
			 gw->browser, 1, MUIM_Tsunami_Browser_SyncScroll);
	}
}

/**
 * Refresh chrome_titles[] from title buffers (NULL-terminated for Register).
 */
static void refresh_register_titles(void)
{
	int i;

	for (i = 0; i < TSUNAMI_MAX_TABS + 1; i++) {
		chrome_titles[i] = NULL;
	}
	for (i = 0; i < chrome_tab_count; i++) {
		if (chrome_title_buf[i][0] == '\0') {
			snprintf(chrome_title_buf[i],
				 sizeof(chrome_title_buf[0]),
				 "Tab %d", i + 1);
		}
		chrome_titles[i] = chrome_title_buf[i];
	}
}

/**
 * Rebuild RegisterGroup so titles match children.
 * MUIA_Register_Titles is init-only on MUI 3.8 — AddMember alone cannot
 * grow the tab strip. Pages are RemMember'd into a new Register, not disposed.
 */
static BOOL rebuild_register_group(void)
{
	Object *pages[TSUNAMI_MAX_TABS];
	Object *neu;
	Object *old;
	Object *parent;
	struct gui_window *g;
	int i;
	int n;

	tabtrace("rebuild_register enter");
	if (chrome_root == NULL || chrome_pages == NULL ||
	    chrome_statusbar == NULL) {
		tabtrace("rebuild_register missing chrome");
		return FALSE;
	}

	n = 0;
	for (i = 0; i < TSUNAMI_MAX_TABS; i++) {
		pages[i] = NULL;
	}
	for (g = gw_list; g != NULL; g = g->next) {
		if (g->page != NULL && g->tab_index >= 0 &&
		    g->tab_index < TSUNAMI_MAX_TABS) {
			pages[g->tab_index] = g->page;
			if (g->tab_index + 1 > n) {
				n = g->tab_index + 1;
			}
		}
	}
	if (n == 0) {
		tabtrace("rebuild_register no pages");
		return FALSE;
	}

	refresh_register_titles();
	old = chrome_pages;

	tabtracef("rebuild_register detach n", (ULONG)n, (ULONG)old);
	DoMethod(chrome_root, MUIM_Group_InitChange);
	DoMethod(old, MUIM_Group_InitChange);
	for (i = 0; i < n; i++) {
		if (pages[i] == NULL) {
			continue;
		}
		parent = NULL;
		get(pages[i], MUIA_Parent, &parent);
		if (parent == old) {
			DoMethod(old, OM_REMMEMBER, pages[i]);
		}
	}
	DoMethod(old, MUIM_Group_ExitChange);
	DoMethod(chrome_root, OM_REMMEMBER, old);
	DoMethod(chrome_root, OM_REMMEMBER, chrome_statusbar);

	neu = RegisterGroup(chrome_titles),
		MUIA_Register_Frame, TRUE,
		MUIA_Background, MUII_RegisterBack,
		MUIA_Weight, 100,
	End;
	if (neu == NULL) {
		tabtrace("rebuild_register NEW FAILED");
		DoMethod(chrome_root, OM_ADDMEMBER, old);
		DoMethod(chrome_root, OM_ADDMEMBER, chrome_statusbar);
		DoMethod(chrome_root, MUIM_Group_ExitChange);
		return FALSE;
	}

	for (i = 0; i < n; i++) {
		if (pages[i] != NULL) {
			DoMethod(neu, OM_ADDMEMBER, pages[i]);
		}
	}

	DoMethod(chrome_root, OM_ADDMEMBER, neu);
	DoMethod(chrome_root, OM_ADDMEMBER, chrome_statusbar);
	DoMethod(chrome_root, MUIM_Group_ExitChange);

	DoMethod(neu, MUIM_Notify, MUIA_Group_ActivePage, MUIV_EveryTime,
		 tsunami_app, 2, MUIM_Application_ReturnID, OID_TAB_SWITCH);

	chrome_pages = neu;
	MUI_DisposeObject(old);
	tabtrace("rebuild_register done");
	return TRUE;
}

/**
 * MUIA_Register_Titles is create-time only — after set_title updates the
 * title buffers, rebuild so the strip shows the new labels. Coalesced via
 * schedule(0) so a burst of title changes (redirects) rebuilds once.
 */
static void deferred_rebuild_titles(void *p)
{
	struct gui_window *gw;

	(void)p;
	if (chrome_pages == NULL || chrome_tab_count < 1) {
		return;
	}
	tabtrace("deferred_rebuild_titles");
	if (!rebuild_register_group()) {
		return;
	}
	gw = active_gw();
	if (gw != NULL) {
		activate_tab(gw);
	}
}

static void request_title_rebuild(void)
{
	tsunami_schedule(0, deferred_rebuild_titles, NULL);
}

/**
 * Tab-strip label: document title as-is, or host when core fell back to the
 * URL (no HTML title element). Register buffers are short — keep labels compact.
 */
static void format_tab_title(const char *title, char *out, size_t outsz)
{
	const char *s;
	const char *end;
	size_t n;

	if (out == NULL || outsz < 2) {
		return;
	}
	out[0] = '\0';
	if (title == NULL || title[0] == '\0') {
		return;
	}

	if (strcmp(title, "Nami") == 0) {
		title = "Tsunami";
	}

	s = title;
	if (strncmp(s, "https://", 8) == 0) {
		s += 8;
	} else if (strncmp(s, "http://", 7) == 0) {
		s += 7;
	} else {
		strncpy(out, title, outsz - 1);
		out[outsz - 1] = '\0';
		return;
	}

	/* Strip userinfo */
	end = strchr(s, '@');
	if (end != NULL) {
		s = end + 1;
	}
	end = s;
	while (*end != '\0' && *end != '/' && *end != '?' && *end != '#') {
		end++;
	}
	n = (size_t)(end - s);
	if (n == 0) {
		strncpy(out, title, outsz - 1);
		out[outsz - 1] = '\0';
		return;
	}
	if (n >= outsz) {
		n = outsz - 1;
	}
	memcpy(out, s, n);
	out[n] = '\0';
}

/**
 * One page-info glyph — Amiga FE theme_pageinfo_* BitMapObj, text fallback.
 */
static Object *make_pageinfo_icon(struct gui_window *gw,
				  const char *theme_key,
				  const char *fallback_label,
				  const char *help)
{
	Object *bmo;
	Object *img;
	struct Image *im;
	struct Screen *scrn;
	BOOL locked;

	scrn = ami_gui_get_screen();
	locked = FALSE;
	if (scrn == NULL) {
		scrn = LockPubScreen(NULL);
		locked = TRUE;
	}
	bmo = ami_gui_theme_bitmap(scrn, theme_key, NULL);
	if (locked && scrn != NULL) {
		UnlockPubScreen(NULL, scrn);
	}

	im = (struct Image *)bmo;
	if (bmo != NULL && im != NULL && im->Width > 0 && im->Height > 0) {
		img = ImageObject,
			MUIA_Image_OldImage, im,
			MUIA_Weight, 0,
			MUIA_ShortHelp, help,
			MUIA_InputMode, MUIV_InputMode_None,
		End;
		if (img != NULL) {
			gw_remember_bm(gw, bmo);
			return img;
		}
		DisposeObject(bmo);
	} else if (bmo != NULL) {
		DisposeObject(bmo);
	}

	return TextObject,
		ButtonFrame,
		MUIA_Font, MUIV_Font_Tiny,
		MUIA_Text_Contents, fallback_label,
		MUIA_Text_SetMin, TRUE,
		MUIA_ShortHelp, help,
		MUIA_Weight, 0,
	End;
}

/**
 * PageGroup of theme page-info icons (same keys as Amiga gui.c).
 */
static Object *make_secure_indicator(struct gui_window *gw)
{
	return PageGroup,
		MUIA_Weight, 0,
		Child, make_pageinfo_icon(gw, "theme_pageinfo_internal",
					  "\33c?",
					  messages_get("PageInfoInternal")),
		Child, make_pageinfo_icon(gw, "theme_pageinfo_local",
					  "\33cfile",
					  messages_get("PageInfoLocal")),
		Child, make_pageinfo_icon(gw, "theme_pageinfo_insecure",
					  "\33cinsecure",
					  messages_get("PageInfoInsecure")),
		Child, make_pageinfo_icon(gw, "theme_pageinfo_warning",
					  "\33cwarning",
					  messages_get("PageInfoWarning")),
		Child, make_pageinfo_icon(gw, "theme_pageinfo_secure",
					  "\33cTLS",
					  messages_get("PageInfoSecure")),
	End;
}

static Object *make_throbber_group(void)
{
	chrome_gauge = NULL;
	chrome_busy = tsunami_throbber_new();
	if (chrome_busy != NULL) {
		/* Always visible — idle is dark LEDs, not a hidden page. */
		return chrome_busy;
	}

	chrome_gauge = GaugeObject,
		GaugeFrame,
		MUIA_Font, MUIV_Font_Tiny,
		MUIA_Gauge_Horiz, TRUE,
		MUIA_Gauge_Max, 100,
		MUIA_Gauge_Current, 0,
		MUIA_Gauge_InfoText, "",
		MUIA_FixWidth, 120,
		MUIA_Weight, 0,
	End;
	return chrome_gauge;
}

/**
 * Create (or add a tab to) the shared MUI Window.
 * First tab builds RegisterGroup; further tabs rebuild it with new titles.
 */
static Object *ensure_chrome_and_add_tab(struct gui_window *gw)
{
	Object *page;

	tabtracef("ensure_chrome enter tab_count", (ULONG)chrome_tab_count,
		  (ULONG)(chrome_win != NULL));

	if (chrome_tab_count >= TSUNAMI_MAX_TABS) {
		tabtrace("ensure_chrome TAB LIMIT");
		status_note(gw_active, "Tab limit reached");
		return NULL;
	}

	gw->tab_index = chrome_tab_count;
	tabtrace("ensure_chrome make_browser_page");
	page = make_browser_page(gw);
	tabtracef("ensure_chrome page/browser", (ULONG)page,
		  (ULONG)gw->browser);
	if (page == NULL) {
		tabtrace("ensure_chrome page NULL");
		return NULL;
	}

	snprintf(chrome_title_buf[gw->tab_index],
		 sizeof(chrome_title_buf[0]),
		 "Tab %d", gw->tab_index + 1);

	if (chrome_win == NULL) {
		tabtrace("ensure_chrome FIRST window path");
		chrome_owner = gw;
		chrome_url = make_url_gadget();
		chrome_status = TextObject,
			MUIA_Text_Contents, "Tsunami",
			MUIA_Text_PreParse, "\33l",
			MUIA_Font, MUIV_Font_Tiny,
			MUIA_Weight, 100,
		End;
		chrome_secure = make_secure_indicator(gw);
		chrome_throb = make_throbber_group();
		chrome_newtab = make_tb_button(gw, "theme_addtab", "+", "New tab");
		chrome_closetab = make_tb_button(gw, "theme_closetab", "X",
						 "Close tab");

		chrome_back = make_tb_button(gw, "theme_nav_west", "Back", "Back");
		chrome_fwd = make_tb_button(gw, "theme_nav_east", "Forward", "Forward");
		chrome_stop_reload = make_stop_reload_button(gw);
		chrome_home = make_tb_button(gw, "theme_home", "Home", "Home");

		chrome_titles[0] = chrome_title_buf[0];
		chrome_titles[1] = NULL;

		/* MUI 3.8 tab control — same as SDK Examples/Pages.c */
		chrome_pages = RegisterGroup(chrome_titles),
			MUIA_Register_Frame, TRUE,
			MUIA_Background, MUII_RegisterBack,
			MUIA_Weight, 100,
			Child, page,
		End;

		chrome_statusbar = HGroup,
			MUIA_Font, MUIV_Font_Tiny,
			MUIA_InnerLeft, 4,
			MUIA_InnerRight, 4,
			MUIA_InnerTop, 2,
			MUIA_InnerBottom, 2,
			Child, chrome_status,
			Child, chrome_throb,
		End;

		chrome_root = VGroup,
			Child, HGroup,
				MUIA_Group_HorizSpacing, 4,
				Child, HGroup,
					MUIA_Weight, 0,
					MUIA_Group_HorizSpacing, 1,
					Child, chrome_back,
					Child, chrome_fwd,
					Child, chrome_stop_reload,
					Child, chrome_home,
				End,
				Child, chrome_secure,
				Child, chrome_url,
				Child, HGroup,
					MUIA_Weight, 0,
					MUIA_Group_HorizSpacing, 1,
					Child, chrome_closetab,
					Child, chrome_newtab,
				End,
			End,
			Child, chrome_pages,
			Child, chrome_statusbar,
		End;

		/*
		 * Visible % sizing like NetSurf-mui; new Window_ID so an old
		 * half-size snapshot is not restored.
		 */
		chrome_win = WindowObject,
			MUIA_Window_Title, "Tsunami",
			MUIA_Window_ID, MAKE_ID('T','S','U','2'),
			MUIA_Window_AppWindow, TRUE,
			MUIA_Window_Width, MUIV_Window_Width_Visible(75),
			MUIA_Window_Height, MUIV_Window_Height_Visible(90),
			WindowContents, chrome_root,
		End;

		if (chrome_win == NULL) {
			return NULL;
		}

		DoMethod(chrome_win, MUIM_Notify, MUIA_Window_CloseRequest, TRUE,
			 tsunami_app, 2, MUIM_Application_ReturnID,
			 MUIV_Application_ReturnID_Quit);

		DoMethod(chrome_back, MUIM_Notify, MUIA_Pressed, FALSE,
			 tsunami_app, 2, MUIM_Application_ReturnID, OID_BACK);
		DoMethod(chrome_fwd, MUIM_Notify, MUIA_Pressed, FALSE,
			 tsunami_app, 2, MUIM_Application_ReturnID, OID_FORWARD);
		DoMethod(chrome_stop_reload, MUIM_Notify, MUIA_Pressed, FALSE,
			 tsunami_app, 2, MUIM_Application_ReturnID,
			 OID_STOP_RELOAD);
		DoMethod(chrome_home, MUIM_Notify, MUIA_Pressed, FALSE,
			 tsunami_app, 2, MUIM_Application_ReturnID, OID_HOME);
		DoMethod(chrome_newtab, MUIM_Notify, MUIA_Pressed, FALSE,
			 tsunami_app, 2, MUIM_Application_ReturnID, OID_NEWTAB);
		DoMethod(chrome_closetab, MUIM_Notify, MUIA_Pressed, FALSE,
			 tsunami_app, 2, MUIM_Application_ReturnID, OID_CLOSETAB);
		DoMethod(chrome_url, MUIM_Notify, MUIA_String_Acknowledge,
			 MUIV_EveryTime,
			 tsunami_app, 2, MUIM_Application_ReturnID, OID_GO);
		DoMethod(chrome_pages, MUIM_Notify, MUIA_Group_ActivePage,
			 MUIV_EveryTime, tsunami_app, 2,
			 MUIM_Application_ReturnID, OID_TAB_SWITCH);

		DoMethod(tsunami_app, OM_ADDMEMBER, chrome_win);
		set(chrome_win, MUIA_Window_Open, TRUE);

		gw->win = chrome_win;
		gw->url = chrome_url;
		gw->status = chrome_status;
		gw->gauge = chrome_gauge;
		gw->width = 640;
		gw->height = 400;

		chrome_tab_count++;
		wire_scroll_notifies(gw);
		activate_tab(gw);
		update_nav_buttons();
		return chrome_win;
	}

	/*
	 * Extra tab: share chrome gadgets and rebuild Register so the new
	 * title appears (MUIA_Register_Titles is create-time only).
	 */
	tabtrace("ensure_chrome EXTRA register path");
	chrome_tab_count++;
	gw->win = chrome_win;
	gw->url = chrome_url;
	gw->status = chrome_status;
	gw->gauge = chrome_gauge;
	gw->width = 640;
	gw->height = 400;

	if (!rebuild_register_group()) {
		tabtrace("ensure_chrome rebuild FAILED");
		chrome_tab_count--;
		MUI_DisposeObject(page);
		gw->page = NULL;
		gw->browser = NULL;
		gw->hbar = NULL;
		gw->vbar = NULL;
		return NULL;
	}
	wire_scroll_notifies(gw);
	activate_tab(gw);
	tabtrace("ensure_chrome EXTRA done");
	return chrome_win;
}

/* ---------- gui_window_table ---------- */

static struct gui_window *
gui_window_create(struct browser_window *bw,
		  struct gui_window *existing,
		  gui_window_create_flags flags)
{
	struct gui_window *gw;

	(void)existing;

	tabtracef("gui_window_create flags", (ULONG)flags,
		  (ULONG)chrome_tab_count);

	gw = calloc(1, sizeof(*gw));
	if (gw == NULL) {
		tabtrace("gui_window_create NOMEM");
		return NULL;
	}

	gw->bw = bw;
	gw->next = gw_list;
	gw_list = gw;

	if (ensure_chrome_and_add_tab(gw) == NULL) {
		tabtrace("gui_window_create ensure FAILED");
		gw_list = gw->next;
		free(gw);
		return NULL;
	}

	tabtracef("gui_window_create OK tab", (ULONG)gw->tab_index,
		  (ULONG)gw->win);
	return gw;
}

static void gui_window_destroy(struct gui_window *gw)
{
	struct gui_window **p;
	struct gui_window *t;
	struct gui_window *survivor;
	Object *dead_page;
	Object *dead_browser;
	Object *parent;
	int i;
	int remaining;
	int idx;
	int dying_index;

	if (gw == NULL) {
		return;
	}

	tabtracef("gui_window_destroy enter tab", (ULONG)gw->tab_index,
		  (ULONG)chrome_tab_count);

	/* Drop any deferred close / title rebuild aimed at this tab. */
	tsunami_schedule(-1, deferred_close_tab, gw);
	tsunami_schedule(-1, deferred_open_find, gw);
	tsunami_schedule(-1, deferred_open_hist_local, gw);
	tsunami_schedule(-1, deferred_file_open, gw);
	tsunami_schedule(-1, deferred_rebuild_titles, NULL);

	/* Find / local history hold pointers into this gui_window. */
	ami_search_close_if_gw(gw);
	ami_history_local_close_if_gw(gw);

	dying_index = gw->tab_index;
	dead_page = gw->page;
	dead_browser = gw->browser;

	/* Kill Prop → browser notifies before the page leaves the tree. */
	if (gw->hbar != NULL) {
		DoMethod(gw->hbar, MUIM_KillNotify, MUIA_Prop_First);
	}
	if (gw->vbar != NULL) {
		DoMethod(gw->vbar, MUIM_KillNotify, MUIA_Prop_First);
	}
	tsunami_browser_detach(dead_browser);

	if (gw_active == gw) {
		gw_active = NULL;
	}

	remaining = 0;
	for (p = &gw_list; *p != NULL; p = &(*p)->next) {
		if (*p == gw) {
			*p = gw->next;
			break;
		}
	}
	for (t = gw_list; t != NULL; t = t->next) {
		remaining++;
	}

	/* Prefer the tab immediately left of the dying one for ActivePage. */
	survivor = NULL;
	for (t = gw_list; t != NULL; t = t->next) {
		if (t->tab_index < dying_index) {
			if (survivor == NULL ||
			    t->tab_index > survivor->tab_index) {
				survivor = t;
			}
		}
	}
	if (survivor == NULL) {
		survivor = gw_list;
	}

	if (remaining == 0 && chrome_win != NULL) {
		tabtrace("destroy last tab — dispose chrome window");
		set(chrome_win, MUIA_Window_Open, FALSE);
		DoMethod(tsunami_app, OM_REMMEMBER, chrome_win);
		MUI_DisposeObject(chrome_win);
		chrome_win = NULL;
		chrome_root = NULL;
		chrome_pages = NULL;
		chrome_statusbar = NULL;
		chrome_url = NULL;
		chrome_status = NULL;
		chrome_gauge = NULL;
		chrome_busy = NULL;
		chrome_throb = NULL;
		chrome_secure = NULL;
		chrome_tab_count = 0;
		chrome_owner = NULL;
		chrome_back = NULL;
		chrome_fwd = NULL;
		chrome_stop_reload = NULL;
		chrome_stop_bm = NULL;
		chrome_reload_bm = NULL;
		chrome_home = NULL;
		chrome_newtab = NULL;
		chrome_closetab = NULL;
		chrome_sr_is_stop = 0;
		chrome_sr_text = 0;
		free(chrome_wintitle);
		chrome_wintitle = NULL;
		gw->page = NULL;
		gw->browser = NULL;
		gw->hbar = NULL;
		gw->vbar = NULL;
	} else if (gw->win != NULL && gw->win != chrome_win) {
		/* Legacy sibling window from older builds. */
		set(gw->win, MUIA_Window_Open, FALSE);
		DoMethod(tsunami_app, OM_REMMEMBER, gw->win);
		MUI_DisposeObject(gw->win);
		gw->win = NULL;
		gw->page = NULL;
		gw->browser = NULL;
		gw->url = NULL;
		gw->status = NULL;
		chrome_tab_count--;
	} else if (dead_page != NULL && chrome_pages != NULL) {
		tabtracef("destroy multi-tab RemMember", (ULONG)dying_index,
			  (ULONG)(survivor != NULL ? survivor->tab_index : 999));

		/*
		 * Never RemMember/Dispose the Register ActivePage. Classic MUI
		 * crashes if the visible page is pulled out from under it —
		 * log died here after close of tab 2 with ActivePage still 2.
		 */
		if (survivor != NULL && survivor->page != NULL) {
			prop_nnset(chrome_pages, MUIA_Group_ActivePage,
				   (ULONG)survivor->tab_index);
		} else {
			prop_nnset(chrome_pages, MUIA_Group_ActivePage, 0);
		}

		gw->page = NULL;
		gw->browser = NULL;
		gw->hbar = NULL;
		gw->vbar = NULL;
		chrome_tab_count--;

		parent = NULL;
		get(dead_page, MUIA_Parent, &parent);
		if (parent == chrome_pages && chrome_root != NULL) {
			DoMethod(chrome_root, MUIM_Group_InitChange);
			DoMethod(chrome_pages, MUIM_Group_InitChange);
			DoMethod(chrome_pages, OM_REMMEMBER, dead_page);
			DoMethod(chrome_pages, MUIM_Group_ExitChange);
			DoMethod(chrome_root, MUIM_Group_ExitChange);
		}
		tabtrace("destroy RemMember done — reindex");

		/* Reassign tab_index in ascending old-index order; shift titles. */
		{
			struct gui_window *ordered[TSUNAMI_MAX_TABS];
			int oi;

			for (oi = 0; oi < TSUNAMI_MAX_TABS; oi++) {
				ordered[oi] = NULL;
			}
			for (t = gw_list; t != NULL; t = t->next) {
				if (t->tab_index >= 0 &&
				    t->tab_index < TSUNAMI_MAX_TABS) {
					ordered[t->tab_index] = t;
				}
			}
			idx = 0;
			for (oi = 0; oi < TSUNAMI_MAX_TABS; oi++) {
				if (ordered[oi] == NULL) {
					continue;
				}
				if (idx != oi) {
					memcpy(chrome_title_buf[idx],
					       chrome_title_buf[oi],
					       sizeof(chrome_title_buf[0]));
				}
				ordered[oi]->tab_index = idx;
				idx++;
			}
			for (; idx < TSUNAMI_MAX_TABS; idx++) {
				chrome_title_buf[idx][0] = '\0';
			}
		}

		if (chrome_tab_count > 0) {
			rebuild_register_group();
		}

		tabtrace("destroy dispose orphaned page");
		MUI_DisposeObject(dead_page);

		if (survivor != NULL) {
			activate_tab(survivor);
		} else if (gw_list != NULL) {
			activate_tab(gw_list);
		}
		update_nav_buttons();
		tabtrace("destroy multi-tab done");
	}

	if (chrome_owner == gw) {
		/* Toolbar BitMapObjs stay alive with the shared window. */
		if (gw_list != NULL) {
			memcpy(gw_list->tb_bm, gw->tb_bm, sizeof(gw->tb_bm));
			gw_list->tb_bm_count = gw->tb_bm_count;
			memset(gw->tb_bm, 0, sizeof(gw->tb_bm));
			gw->tb_bm_count = 0;
			chrome_owner = gw_list;
		} else {
			for (i = 0; i < gw->tb_bm_count; i++) {
				if (gw->tb_bm[i] != NULL) {
					DisposeObject(gw->tb_bm[i]);
					gw->tb_bm[i] = NULL;
				}
			}
			gw->tb_bm_count = 0;
			chrome_owner = NULL;
		}
	}

	tabtrace("gui_window_destroy free gw");
	free(gw);
}

static nserror gui_window_invalidate(struct gui_window *gw,
				     const struct rect *rect)
{
	(void)rect;
	if (gw != NULL) {
		tsunami_browser_redraw(gw->browser);
	}
	return NSERROR_OK;
}

static bool gui_window_get_scroll(struct gui_window *gw, int *sx, int *sy)
{
	ULONG first;

	if (gw == NULL) {
		return false;
	}
	if (gw->hbar != NULL) {
		first = 0;
		get(gw->hbar, MUIA_Prop_First, &first);
		gw->scrollx = (int)first;
	}
	if (gw->vbar != NULL) {
		first = 0;
		get(gw->vbar, MUIA_Prop_First, &first);
		gw->scrolly = (int)first;
	}
	*sx = gw->scrollx;
	*sy = gw->scrolly;
	return true;
}

static nserror gui_window_set_scroll(struct gui_window *gw,
				     const struct rect *rect)
{
	int sx;
	int sy;
	int width;
	int height;

	if (gw == NULL || gw->bw == NULL) {
		return NSERROR_BAD_PARAMETER;
	}
	if (browser_window_has_content(gw->bw) == false) {
		return NSERROR_BAD_PARAMETER;
	}

	sx = rect->x0 > 0 ? rect->x0 : 0;
	sy = rect->y0 > 0 ? rect->y0 : 0;

	browser_window_get_extents(gw->bw, false, &width, &height);

	if (sx >= width - gw->width) {
		sx = width - gw->width;
	}
	if (sy >= height - gw->height) {
		sy = height - gw->height;
	}
	if (width <= gw->width) {
		sx = 0;
	}
	if (height <= gw->height) {
		sy = 0;
	}
	if (sx < 0) {
		sx = 0;
	}
	if (sy < 0) {
		sy = 0;
	}

	gw->scrollx = sx;
	gw->scrolly = sy;
	prop_nnset(gw->hbar, MUIA_Prop_First, (ULONG)sx);
	prop_nnset(gw->vbar, MUIA_Prop_First, (ULONG)sy);
	tsunami_browser_redraw(gw->browser);
	return NSERROR_OK;
}

static nserror gui_window_get_dimensions(struct gui_window *gw,
					 int *width, int *height)
{
	if (gw == NULL) {
		return NSERROR_BAD_PARAMETER;
	}
	*width = gw->width > 0 ? gw->width : 640;
	*height = gw->height > 0 ? gw->height : 400;
	return NSERROR_OK;
}

static void gui_window_set_title(struct gui_window *gw, const char *title)
{
	char *new_title;
	char tab_label[sizeof(chrome_title_buf[0])];
	size_t len;
	int idx;

	if (gw == NULL || title == NULL) {
		return;
	}

	/* Document title alone was still "Nami" from about:welcome. */
	if (strcmp(title, "Nami") == 0) {
		title = "Tsunami";
	}

	idx = gw->tab_index;
	if (idx >= 0 && idx < TSUNAMI_MAX_TABS) {
		format_tab_title(title, tab_label, sizeof(tab_label));
		if (tab_label[0] == '\0') {
			snprintf(tab_label, sizeof(tab_label), "Tab %d", idx + 1);
		}
		if (strcmp(chrome_title_buf[idx], tab_label) != 0) {
			strncpy(chrome_title_buf[idx], tab_label,
				sizeof(chrome_title_buf[0]) - 1);
			chrome_title_buf[idx][sizeof(chrome_title_buf[0]) - 1] =
				'\0';
			chrome_titles[idx] = chrome_title_buf[idx];
			/* Register titles are init-only — rebuild outside notify. */
			request_title_rebuild();
		}
	}

	if (gw->win == NULL || gw != gw_active) {
		return;
	}

	/*
	 * Amiga FE strdup's into shared->wintitle before SetWindowTitles.
	 * MUIA_Window_Title stores the pointer; a stack buffer is corrupted
	 * as soon as another window open / refresh reuses that stack.
	 */
	len = strlen(title) + sizeof("Tsunami: ");
	new_title = malloc(len);
	if (new_title == NULL) {
		return;
	}
	snprintf(new_title, len, "Tsunami: %s", title);
	if (chrome_wintitle != NULL && strcmp(chrome_wintitle, new_title) == 0) {
		free(new_title);
		return;
	}
	free(chrome_wintitle);
	chrome_wintitle = new_title;
	set(gw->win, MUIA_Window_Title, chrome_wintitle);
}

static nserror gui_window_set_url(struct gui_window *gw, struct nsurl *url)
{
	const char *s;

	if (gw == NULL || gw->url == NULL || url == NULL) {
		return NSERROR_BAD_PARAMETER;
	}
	s = nsurl_access(url);
	if (gw == gw_active || gw_active == NULL) {
		set(gw->url, MUIA_String_Contents, s);
	}
	return NSERROR_OK;
}

static void gui_window_set_status(struct gui_window *gw, const char *text)
{
	if (gw != NULL && gw->status != NULL && text != NULL &&
	    (gw == gw_active || gw_active == NULL)) {
		set(gw->status, MUIA_Text_Contents, text);
	}
}

static nserror gui_window_event(struct gui_window *gw,
				enum gui_window_event event)
{
	if (gw == NULL) {
		return NSERROR_OK;
	}

	switch (event) {
	case GW_EVENT_UPDATE_EXTENT:
		throb_note_layout();
		tsunami_gui_sync_scrollbars(gw);
		tsunami_browser_redraw(gw->browser);
		break;
	case GW_EVENT_START_THROBBER:
		set_throbber(gw, 1);
		update_nav_buttons();
		break;
	case GW_EVENT_STOP_THROBBER:
		set_throbber(gw, 0);
		update_nav_buttons();
		break;
	case GW_EVENT_NEW_CONTENT:
		throb_note_layout();
		tsunami_browser_release_pens();
		gw->scrollx = 0;
		gw->scrolly = 0;
		prop_nnset(gw->hbar, MUIA_Prop_First, 0);
		prop_nnset(gw->vbar, MUIA_Prop_First, 0);
		tsunami_gui_sync_scrollbars(gw);
		tsunami_browser_redraw(gw->browser);
		update_page_info(gw);
		update_nav_buttons();
		break;
	case GW_EVENT_PAGE_INFO_CHANGE:
		update_page_info(gw);
		break;
	default:
		break;
	}
	return NSERROR_OK;
}

static struct gui_window_table window_table = {
	.create = gui_window_create,
	.destroy = gui_window_destroy,
	.invalidate = gui_window_invalidate,
	.get_scroll = gui_window_get_scroll,
	.set_scroll = gui_window_set_scroll,
	.get_dimensions = gui_window_get_dimensions,
	.event = gui_window_event,
	.set_title = gui_window_set_title,
	.set_url = gui_window_set_url,
	.set_status = gui_window_set_status,
};

struct gui_window_table *tsunami_window_table = &window_table;

static void open_new_tab(void)
{
	struct browser_window *bw;
	nsurl *url;
	const char *home;

	tabtrace("open_new_tab enter");
	home = nsoption_charp(homepage_url);
	if (home == NULL) {
		home = "resource:welcome-tsunami.html";
	}
	tabtrace(home);
	if (nsurl_create(home, &url) != NSERROR_OK) {
		tabtrace("open_new_tab nsurl_create FAIL");
		return;
	}
	tabtrace("open_new_tab browser_window_create...");
	browser_window_create(BW_CREATE_HISTORY | BW_CREATE_TAB |
			      BW_CREATE_FOREGROUND,
			      url, NULL, NULL, &bw);
	nsurl_unref(url);
	tabtracef("open_new_tab done bw", (ULONG)bw, 0);
}

/**
 * Amiga Nami: ami_schedule(0, ami_gui_nami_close_tab_cb). Never destroy a
 * tab Register page from inside the Close-tab button notify.
 */
static void deferred_close_tab(void *p)
{
	struct gui_window *gw;
	struct gui_window *t;

	gw = (struct gui_window *)p;
	if (gw == NULL || gw->bw == NULL) {
		return;
	}
	for (t = gw_list; t != NULL; t = t->next) {
		if (t == gw) {
			browser_window_destroy(gw->bw);
			return;
		}
	}
}

static void request_close_tab(struct gui_window *gw)
{
	if (gw == NULL || gw->bw == NULL) {
		return;
	}
	if (chrome_tab_count <= 1) {
		return;
	}
	tsunami_schedule(0, deferred_close_tab, gw);
}

static void handle_chrome_id(ULONG id)
{
	struct gui_window *gw;
	nsurl *url;
	const char *home;

	tabtracef("handle_chrome_id", id, 0);

	if (id == OID_TAB_SWITCH) {
		ULONG page;
		struct gui_window *t;

		tabtrace("handle OID_TAB_SWITCH");
		page = 0;
		if (chrome_pages != NULL) {
			get(chrome_pages, MUIA_Group_ActivePage, &page);
		}
		for (t = gw_list; t != NULL; t = t->next) {
			if ((ULONG)t->tab_index == page) {
				activate_tab(t);
				tabtracef("TAB_SWITCH register page", page, 0);
				return;
			}
		}
		tabtrace("TAB_SWITCH no match");
		return;
	}

	gw = active_gw();

	switch (id) {
	case OID_NEWTAB:
	case MID_NEW_TAB:
		/* Amiga Nami: ami_schedule(0, new_tab). Not PushMethod. */
		tabtrace("OID_NEWTAB — schedule deferred_new_tab");
		tsunami_schedule(0, deferred_new_tab, NULL);
		break;
	case MID_CLOSE_TAB:
	case OID_CLOSETAB:
		request_close_tab(active_gw());
		break;
	case OID_BACK:
	case MID_BACK:
		if (gw != NULL && gw->bw != NULL &&
		    browser_window_back_available(gw->bw)) {
			browser_window_history_back(gw->bw, false);
		}
		update_nav_buttons();
		break;
	case OID_FORWARD:
	case MID_FORWARD:
		if (gw != NULL && gw->bw != NULL &&
		    browser_window_forward_available(gw->bw)) {
			browser_window_history_forward(gw->bw, false);
		}
		update_nav_buttons();
		break;
	case OID_STOP_RELOAD:
		/* Amiga Nami: stop while loading, else reload. */
		if (gw != NULL && gw->bw != NULL) {
			if (browser_window_stop_available(gw->bw)) {
				browser_window_stop(gw->bw);
			} else if (browser_window_reload_available(gw->bw)) {
				browser_window_reload(gw->bw, true);
			}
		}
		update_nav_buttons();
		break;
	case MID_STOP:
		if (gw != NULL && gw->bw != NULL) {
			browser_window_stop(gw->bw);
		}
		update_nav_buttons();
		break;
	case MID_RELOAD:
		if (gw != NULL && gw->bw != NULL) {
			browser_window_reload(gw->bw, true);
		}
		update_nav_buttons();
		break;
	case OID_HOME:
	case MID_HOME:
		if (gw == NULL || gw->bw == NULL) {
			break;
		}
		home = nsoption_charp(homepage_url);
		if (home != NULL && nsurl_create(home, &url) == NSERROR_OK) {
			browser_window_navigate(gw->bw, url, NULL,
						BW_NAVIGATE_HISTORY,
						NULL, NULL, NULL);
			nsurl_unref(url);
		}
		break;
	case OID_GO:
		url_go_func(gw);
		break;
	case MID_ABOUT:
		if (gw != NULL && gw->bw != NULL &&
		    nsurl_create("about:about", &url) == NSERROR_OK) {
			browser_window_navigate(gw->bw, url, NULL,
						BW_NAVIGATE_HISTORY,
						NULL, NULL, NULL);
			nsurl_unref(url);
		}
		break;
	case MID_QUIT:
		tsunami_quit();
		break;
	case MID_REDRAW:
		if (gw != NULL) {
			tsunami_browser_redraw(gw->browser);
		}
		break;
	case MID_MUI_PREFS:
		tsunami_schedule(0, deferred_open_mui_prefs, NULL);
		break;
	case MID_OPEN:
		tsunami_schedule(0, deferred_file_open, gw);
		break;
	case MID_SAVE_SOURCE:
		request_file_save(gw, AMINS_SAVE_SOURCE);
		break;
	case MID_SAVE_TEXT:
		request_file_save(gw, AMINS_SAVE_TEXT);
		break;
	case MID_PRINT:
		status_note(gw, "Print not available");
		break;
	case MID_CUT:
		if (gw != NULL && gw->bw != NULL) {
			browser_window_key_press(gw->bw, NS_KEY_CUT_SELECTION);
		}
		break;
	case MID_COPY:
		if (gw != NULL && gw->bw != NULL) {
			browser_window_key_press(gw->bw, NS_KEY_COPY_SELECTION);
		}
		break;
	case MID_PASTE:
		if (gw != NULL && gw->bw != NULL) {
			browser_window_key_press(gw->bw, NS_KEY_PASTE);
		}
		break;
	case MID_SELALL:
		if (gw != NULL && gw->bw != NULL) {
			browser_window_key_press(gw->bw, NS_KEY_SELECT_ALL);
			gui_start_selection(gw);
		}
		break;
	case MID_FIND:
		tsunami_schedule(0, deferred_open_find, gw);
		break;
	case MID_HIST_LOCAL:
		tsunami_schedule(0, deferred_open_hist_local, gw);
		break;
	case MID_HIST_GLOBAL:
		tsunami_schedule(0, deferred_open_hist_global, NULL);
		break;
	case MID_COOKIES:
		tsunami_schedule(0, deferred_open_cookies, NULL);
		break;
	case MID_HOTLIST_ADD:
		if (gw != NULL && gw->bw != NULL &&
		    browser_window_has_content(gw->bw)) {
			hotlist_add_url(browser_window_access_url(gw->bw));
			status_note(gw, "Added to hotlist");
		}
		break;
	case MID_HOTLIST_SHOW:
		tsunami_schedule(0, deferred_open_hotlist, NULL);
		break;
	case MID_PREFS:
		tsunami_schedule(0, deferred_open_prefs, NULL);
		break;
	case MID_SCALE_DEC:
		if (gw != NULL && gw->bw != NULL) {
			browser_window_set_scale(gw->bw, -0.1, false);
		}
		break;
	case MID_SCALE_NORM:
		if (gw != NULL && gw->bw != NULL) {
			browser_window_set_scale(gw->bw, 1.0, true);
		}
		break;
	case MID_SCALE_INC:
		if (gw != NULL && gw->bw != NULL) {
			browser_window_set_scale(gw->bw, 0.1, false);
		}
		break;
	default:
		break;
	}
}

BOOL tsunami_gui_init(void)
{
	if (!tsunami_browser_class_init()) {
		return FALSE;
	}
	if (!tsunami_throbber_class_init()) {
		tsunami_browser_class_fini();
		return FALSE;
	}
	if (!tsunami_corewindow_class_init()) {
		tsunami_throbber_class_fini();
		tsunami_browser_class_fini();
		return FALSE;
	}

	/*
	 * Menustrip must be on the Application at create time (Voyager /
	 * NetSurf-mui). Setting it later often leaves the strip missing.
	 */
	chrome_menustrip = MUI_MakeObject(MUIO_MenustripNM,
					  (ULONG)tsunami_menu,
					  MUIO_MenustripNM_CommandKeyCheck);

	tsunami_app = ApplicationObject,
		MUIA_Application_Title, "Tsunami",
		MUIA_Application_Version, "$VER: Tsunami 3.12 (MUI)",
		MUIA_Application_Copyright, "(C) NetSurf contributors / AmigaZen",
		MUIA_Application_Author, "AmigaZen",
		MUIA_Application_Description, "MUI web browser (NetSurf)",
		MUIA_Application_Base, "TSUNAMI",
		MUIA_Application_Menustrip, chrome_menustrip,
	End;

	if (tsunami_app == NULL) {
		if (chrome_menustrip != NULL) {
			MUI_DisposeObject(chrome_menustrip);
			chrome_menustrip = NULL;
		}
		tsunami_corewindow_class_fini();
		tsunami_throbber_class_fini();
		tsunami_browser_class_fini();
		return FALSE;
	}

	ami_file_req_init();
	ami_clipboard_init();
	return TRUE;
}

void tsunami_gui_fini(void)
{
	ami_search_fini();
	ami_gui_opts_fini();
	ami_hotlist_close();
	ami_cookies_close();
	ami_history_global_close();
	ami_history_local_close();
	ami_file_req_free();
	ami_clipboard_free();

	while (gw_list != NULL) {
		browser_window_destroy(gw_list->bw);
	}

	if (tsunami_app != NULL) {
		MUI_DisposeObject(tsunami_app);
		tsunami_app = NULL;
	}
	tsunami_corewindow_class_fini();
	tsunami_throbber_class_fini();
	tsunami_browser_class_fini();
}

nserror tsunami_gui_open_initial(struct nsurl *url)
{
	struct browser_window *bw;
	nserror error;

	error = browser_window_create(BW_CREATE_HISTORY, url, NULL, NULL, &bw);
	return error;
}

void tsunami_gui_run(void)
{
	ULONG sigs;
	ULONG id;
	BYTE amihttp_sig;
	ULONG waitmask;
	unsigned rid_log;
	int next_ms;

	/*
	 * Pages.c shape: NewInput → handle ReturnID → Wait.
	 * schedule_run must also run AFTER ReturnID handling so work queued
	 * during handle (schedule(0) opens/rebuilds) is not stuck behind Wait.
	 */
	sigs = 0;
	rid_log = 0;
	tabtrace("event loop start");
	while (!tsunami_quitting()) {
		/* Network-only activity may not raise START_THROBBER. */
		if (throb_activity_score() > 0) {
			throbber_kick();
		}
		next_ms = tsunami_schedule_run();

		id = DoMethod(tsunami_app, MUIM_Application_NewInput, &sigs);
		if (id == MUIV_Application_ReturnID_Quit) {
			tabtrace("NewInput QUIT");
			tsunami_quit();
			break;
		}
		if (id != 0) {
			if (rid_log < 100) {
				tabtracef("NewInput ReturnID/sigs", id, sigs);
				rid_log++;
			}
			handle_chrome_id(id);
			if (id == (ULONG)OID_NEWTAB ||
			    id == (ULONG)MID_NEW_TAB) {
				tabtrace("handle_chrome_id returned from NEWTAB");
			}
			/* Due work scheduled inside handle — run before Wait. */
			next_ms = tsunami_schedule_run();
		}

		waitmask = sigs;
		amihttp_sig = fetch_amihttp_signal();
		if (amihttp_sig != -1) {
			waitmask |= (1UL << amihttp_sig);
		}

		/*
		 * Prefer Delay over Wait when a ReturnID was just handled, a
		 * callback is already due (next_ms==0), or the next due is
		 * within a tick — a Wait on only amihttp can sleep forever.
		 */
		if (waitmask != 0 && id == 0 && next_ms != 0 &&
		    (next_ms < 0 || next_ms > 20)) {
			sigs = Wait(waitmask | SIGBREAKF_CTRL_C);
			if (sigs & SIGBREAKF_CTRL_C) {
				tsunami_quit();
				break;
			}
		} else {
			Delay(1);
			sigs = 0;
		}
	}
	tabtrace("event loop end");
}
