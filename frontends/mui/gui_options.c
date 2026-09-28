/*
 * Copyright 2009 Chris Young <chris@unsatisfactorysoftware.co.uk>
 * Copyright 2026 amigazen project
 *
 * Forked from frontends/amiga/gui_options.c — MUI Register prefs for Tsunami.
 * Portable Amiga-parity tabs; Amiga-only screen/docky/pointer gadgets omitted.
 */

#include "mui/os3support.h"

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>

#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <libraries/mui.h>
#include <utility/hooks.h>

#include "utils/nsoption.h"
#include "utils/messages.h"
#include "utils/log.h"
#include "utils/nsurl.h"
#include "netsurf/browser_window.h"
#include "netsurf/plot_style.h"

#include "amiga/font.h"
#include "amiga/utf8.h"
#include "mui/gui.h"
#include "mui/gui_options.h"
#include "mui/schedule.h"
#include "mui/vbcc_defs.h"

/* ---------- gadget ids ---------- */

enum {
	OID_WIN = 0,
	/* General */
	OID_HOMEPAGE,
	OID_HOME_DEFAULT,
	OID_HOME_CURRENT,
	OID_HOME_BLANK,
	OID_HIDEADS,
	OID_CONTENTLANG,
	OID_FROMLOCALE,
	OID_HISTORY,
	OID_JAVASCRIPT,
	OID_REFERRAL,
	OID_DONOTTRACK,
	/* Display */
	OID_DARKMODE,
	OID_THEME,
	/* Network */
	OID_PROXY,
	OID_PROXY_HOST,
	OID_PROXY_PORT,
	OID_PROXY_USER,
	OID_PROXY_PASS,
	OID_PROXY_BYPASS,
	OID_FETCHMAX,
	OID_FETCHHOST,
	OID_FETCHCACHE,
	/* Rendering */
	OID_ANIMDISABLE,
	OID_FOREIMG,
	OID_BACKIMG,
	OID_DPI_Y,
	OID_SCALEQ,
	OID_DITHERQ,
	OID_NATIVEBM,
	/* Fonts */
	OID_FONT_SANS,
	OID_FONT_SERIF,
	OID_FONT_MONO,
	OID_FONT_CURSIVE,
	OID_FONT_FANTASY,
	OID_FONT_DEFAULT,
	OID_FONT_SIZE,
	OID_FONT_MINSIZE,
	OID_FONT_AA,
	OID_FONT_ENGINE,
	/* Cache */
	OID_CACHE_MEM,
	OID_CACHE_DISC,
	/* Tabs */
	OID_TAB_ACTIVE,
	OID_TAB_2,
	OID_TAB_ALWAYS,
	OID_TAB_CLOSE,
	/* Advanced */
	OID_OVERWRITE,
	OID_DLDIR,
	OID_STARTUP_NO_WIN,
	OID_CLOSE_NO_QUIT,
	OID_CLIPBOARD,
	OID_FASTSCROLL,
	OID_ENABLECSS,
	OID_SEARCH_PROV,
	/* Navigation / chrome */
	OID_CATLIST,
	OID_PAGES,
	/* Buttons */
	OID_USE,
	OID_SAVE,
	OID_CANCEL,
	OID_LAST
};

static Object *g[OID_LAST];
static struct Hook close_hook;
static struct Hook use_hook;
static struct Hook save_hook;
static struct Hook home_def_hook;
static struct Hook home_cur_hook;
static struct Hook home_blk_hook;
static struct Hook locale_hook;
static struct Hook proxy_hook;
static bool prefs_closing = false;

static const char *proxy_entries[] = {
	"None",
	"No authentication",
	"Basic",
	"NTLM",
	NULL
};
static const char *nativebm_entries[] = {
	"None",
	"Scaled",
	"All",
	NULL
};
static const char *dither_entries[] = {
	"Low",
	"Medium",
	"High",
	NULL
};
static const char *font_default_entries[] = {
	"Sans-serif",
	"Serif",
	"Monospace",
	"Cursive",
	"Fantasy",
	NULL
};
static const char *font_engine_entries[] = {
	"Auto",
	"Bullet",
	"Diskfont",
	"TTEngine",
	NULL
};
static const char *register_titles[] = {
	"General",
	"Display",
	"Network",
	"Rendering",
	"Fonts",
	"Cache",
	"Tabs",
	"Advanced",
	NULL
};

static void prefs_close_deferred(void *p);

/* ---------- helpers ---------- */

static Object *prefs_string(const char *contents, ULONG maxlen)
{
	return StringObject,
		MUIA_Frame, MUIV_Frame_String,
		MUIA_String_Contents, contents != NULL ? contents : "",
		MUIA_String_MaxLen, maxlen,
		MUIA_CycleChain, TRUE,
	End;
}

static Object *prefs_intstr(LONG value)
{
	char buf[32];

	sprintf(buf, "%ld", (long)value);
	return StringObject,
		MUIA_Frame, MUIV_Frame_String,
		MUIA_String_Contents, buf,
		MUIA_String_Accept, "0123456789",
		MUIA_String_MaxLen, 12,
		MUIA_CycleChain, TRUE,
	End;
}

static Object *prefs_check(const char *label, BOOL selected)
{
	Object *o;

	o = MUI_MakeObject(MUIO_Checkmark, (ULONG)label);
	if (o != NULL) {
		set(o, MUIA_Selected, selected);
	}
	return o;
}

static Object *prefs_cycle(const char **entries, ULONG active)
{
	return CycleObject,
		MUIA_Cycle_Entries, entries,
		MUIA_Cycle_Active, active,
		MUIA_CycleChain, TRUE,
	End;
}

static char *prefs_get_str(Object *o)
{
	char *s;

	s = NULL;
	if (o == NULL) {
		return NULL;
	}
	get(o, MUIA_String_Contents, (ULONG *)&s);
	return s;
}

static LONG prefs_get_long(Object *o)
{
	char *s;
	long v;

	s = prefs_get_str(o);
	if (s == NULL || s[0] == '\0') {
		return 0;
	}
	v = 0;
	sscanf(s, "%ld", &v);
	return (LONG)v;
}

static ULONG prefs_get_sel(Object *o)
{
	ULONG v;

	v = 0;
	if (o != NULL) {
		get(o, MUIA_Selected, &v);
	}
	return v;
}

static ULONG prefs_get_cycle(Object *o)
{
	ULONG v;

	v = 0;
	if (o != NULL) {
		get(o, MUIA_Cycle_Active, &v);
	}
	return v;
}

static void prefs_sync_proxy_disable(void)
{
	ULONG active;
	BOOL need_host;
	BOOL need_auth;

	active = prefs_get_cycle(g[OID_PROXY]);
	need_host = (active > 0) ? TRUE : FALSE;
	need_auth = (active > 1) ? TRUE : FALSE;
	if (g[OID_PROXY_HOST] != NULL) {
		set(g[OID_PROXY_HOST], MUIA_Disabled, !need_host);
	}
	if (g[OID_PROXY_PORT] != NULL) {
		set(g[OID_PROXY_PORT], MUIA_Disabled, !need_host);
	}
	if (g[OID_PROXY_BYPASS] != NULL) {
		set(g[OID_PROXY_BYPASS], MUIA_Disabled, !need_host);
	}
	if (g[OID_PROXY_USER] != NULL) {
		set(g[OID_PROXY_USER], MUIA_Disabled, !need_auth);
	}
	if (g[OID_PROXY_PASS] != NULL) {
		set(g[OID_PROXY_PASS], MUIA_Disabled, !need_auth);
	}
}

static void prefs_sync_locale_disable(void)
{
	ULONG from_locale;

	from_locale = prefs_get_sel(g[OID_FROMLOCALE]);
	if (g[OID_CONTENTLANG] != NULL) {
		set(g[OID_CONTENTLANG], MUIA_Disabled, from_locale ? TRUE : FALSE);
	}
}

/* ---------- apply / close ---------- */

static void prefs_apply(void)
{
	char *s;
	ULONG v;
	LONG n;
	ULONG proxy;

	/* General */
	s = prefs_get_str(g[OID_HOMEPAGE]);
	if (s != NULL) {
		nsoption_set_charp(homepage_url, strdup(s));
	}
	s = prefs_get_str(g[OID_CONTENTLANG]);
	if (s != NULL) {
		nsoption_set_charp(accept_language, strdup(s));
	}
	nsoption_set_bool(accept_lang_locale,
			  prefs_get_sel(g[OID_FROMLOCALE]) ? true : false);
	nsoption_set_bool(block_advertisements,
			  prefs_get_sel(g[OID_HIDEADS]) ? true : false);
	nsoption_set_int(expire_url, (int)prefs_get_long(g[OID_HISTORY]));
	nsoption_set_bool(enable_javascript,
			  prefs_get_sel(g[OID_JAVASCRIPT]) ? true : false);
	nsoption_set_bool(send_referer,
			  prefs_get_sel(g[OID_REFERRAL]) ? true : false);
	nsoption_set_bool(do_not_track,
			  prefs_get_sel(g[OID_DONOTTRACK]) ? true : false);

	/* Display */
	nsoption_set_bool(prefer_dark_mode,
			  prefs_get_sel(g[OID_DARKMODE]) ? true : false);
	s = prefs_get_str(g[OID_THEME]);
	if (s != NULL) {
		nsoption_set_charp(theme, strdup(s));
	}

	/* Network */
	proxy = prefs_get_cycle(g[OID_PROXY]);
	if (proxy == 0) {
		nsoption_set_bool(http_proxy, false);
	} else {
		nsoption_set_bool(http_proxy, true);
		nsoption_set_int(http_proxy_auth, (int)proxy - 1);
	}
	s = prefs_get_str(g[OID_PROXY_HOST]);
	if (s != NULL) {
		nsoption_set_charp(http_proxy_host, strdup(s));
	}
	nsoption_set_int(http_proxy_port, (int)prefs_get_long(g[OID_PROXY_PORT]));
	s = prefs_get_str(g[OID_PROXY_USER]);
	if (s != NULL) {
		nsoption_set_charp(http_proxy_auth_user, strdup(s));
	}
	s = prefs_get_str(g[OID_PROXY_PASS]);
	if (s != NULL) {
		nsoption_set_charp(http_proxy_auth_pass, strdup(s));
	}
	s = prefs_get_str(g[OID_PROXY_BYPASS]);
	if (s != NULL) {
		nsoption_set_charp(http_proxy_noproxy, strdup(s));
	}
	nsoption_set_int(max_fetchers, (int)prefs_get_long(g[OID_FETCHMAX]));
	nsoption_set_int(max_fetchers_per_host,
			 (int)prefs_get_long(g[OID_FETCHHOST]));
	nsoption_set_int(max_cached_fetch_handles,
			 (int)prefs_get_long(g[OID_FETCHCACHE]));

	/* Rendering */
	nsoption_set_bool(animate_images,
			  prefs_get_sel(g[OID_ANIMDISABLE]) ? false : true);
	nsoption_set_bool(foreground_images,
			  prefs_get_sel(g[OID_FOREIMG]) ? true : false);
	nsoption_set_bool(background_images,
			  prefs_get_sel(g[OID_BACKIMG]) ? true : false);
	nsoption_set_int(screen_ydpi, (int)prefs_get_long(g[OID_DPI_Y]));
	nsoption_set_bool(scale_quality,
			  prefs_get_sel(g[OID_SCALEQ]) ? true : false);
	nsoption_set_int(dither_quality, (int)prefs_get_cycle(g[OID_DITHERQ]));
	nsoption_set_int(cache_bitmaps, (int)prefs_get_cycle(g[OID_NATIVEBM]));

	/* Fonts */
	s = prefs_get_str(g[OID_FONT_SANS]);
	if (s != NULL) {
		nsoption_set_charp(font_sans, strdup(s));
	}
	s = prefs_get_str(g[OID_FONT_SERIF]);
	if (s != NULL) {
		nsoption_set_charp(font_serif, strdup(s));
	}
	s = prefs_get_str(g[OID_FONT_MONO]);
	if (s != NULL) {
		nsoption_set_charp(font_mono, strdup(s));
	}
	s = prefs_get_str(g[OID_FONT_CURSIVE]);
	if (s != NULL) {
		nsoption_set_charp(font_cursive, strdup(s));
	}
	s = prefs_get_str(g[OID_FONT_FANTASY]);
	if (s != NULL) {
		nsoption_set_charp(font_fantasy, strdup(s));
	}
	nsoption_set_int(font_default,
			 (int)prefs_get_cycle(g[OID_FONT_DEFAULT]) +
			 PLOT_FONT_FAMILY_SANS_SERIF);
	n = prefs_get_long(g[OID_FONT_SIZE]);
	nsoption_set_int(font_size, (int)(n * 10));
	n = prefs_get_long(g[OID_FONT_MINSIZE]);
	nsoption_set_int(font_min_size, (int)(n * 10));
	nsoption_set_bool(font_antialiasing,
			  prefs_get_sel(g[OID_FONT_AA]) ? true : false);
	v = prefs_get_cycle(g[OID_FONT_ENGINE]);
	ami_font_fini();
	nsoption_set_int(font_engine, (int)v);
	nsoption_set_bool(bitmap_fonts,
			  ((int)v == AMI_FONTENG_DISKFONT) ? true : false);
	ami_font_init();

	/* Cache — UI is megabytes */
	n = prefs_get_long(g[OID_CACHE_MEM]);
	nsoption_set_int(memory_cache_size, (int)(n * 1048576));
	n = prefs_get_long(g[OID_CACHE_DISC]);
	nsoption_set_uint(disc_cache_size, (unsigned int)(n * 1048576));

	/* Tabs — TabActive checked means open in background (Amiga inverted) */
	nsoption_set_bool(foreground_new,
			  prefs_get_sel(g[OID_TAB_ACTIVE]) ? false : true);
	nsoption_set_bool(button_2_tab,
			  prefs_get_sel(g[OID_TAB_2]) ? true : false);
	nsoption_set_bool(tab_always_show,
			  prefs_get_sel(g[OID_TAB_ALWAYS]) ? true : false);
	nsoption_set_bool(tab_close_warn,
			  prefs_get_sel(g[OID_TAB_CLOSE]) ? true : false);

	/* Advanced */
	nsoption_set_bool(ask_overwrite,
			  prefs_get_sel(g[OID_OVERWRITE]) ? true : false);
	s = prefs_get_str(g[OID_DLDIR]);
	if (s != NULL) {
		nsoption_set_charp(download_dir, strdup(s));
	}
	nsoption_set_bool(startup_no_window,
			  prefs_get_sel(g[OID_STARTUP_NO_WIN]) ? true : false);
	nsoption_set_bool(close_no_quit,
			  prefs_get_sel(g[OID_CLOSE_NO_QUIT]) ? true : false);
	nsoption_set_bool(clipboard_write_utf8,
			  prefs_get_sel(g[OID_CLIPBOARD]) ? true : false);
	nsoption_set_bool(faster_scroll,
			  prefs_get_sel(g[OID_FASTSCROLL]) ? true : false);
	nsoption_set_bool(author_level_css,
			  prefs_get_sel(g[OID_ENABLECSS]) ? true : false);
	s = prefs_get_str(g[OID_SEARCH_PROV]);
	if (s != NULL) {
		nsoption_set_charp(search_web_provider, strdup(s));
	}
}

static void prefs_hide(void)
{
	/*
	 * Hide only — do not Dispose. Recreating this Listview+PageGroup tree
	 * on every open locked classic MUI on the second open; Amiga FE can
	 * Dispose Reaction windows safely, but MUI needs the object kept.
	 * Also clear CloseRequest so a reopen does not immediately re-fire
	 * the close notify.
	 */
	if (g[OID_WIN] == NULL) {
		return;
	}
	set(g[OID_WIN], MUIA_Window_Open, FALSE);
	set(g[OID_WIN], MUIA_Window_CloseRequest, FALSE);
	prefs_closing = false;
	tsunami_schedule(-1, prefs_close_deferred, NULL);
}

static void prefs_dispose(void)
{
	ULONG i;

	if (g[OID_WIN] == NULL) {
		return;
	}
	tsunami_schedule(-1, prefs_close_deferred, NULL);
	prefs_closing = false;
	set(g[OID_WIN], MUIA_Window_Open, FALSE);
	DoMethod(tsunami_app, OM_REMMEMBER, g[OID_WIN]);
	MUI_DisposeObject(g[OID_WIN]);
	for (i = 0; i < OID_LAST; i++) {
		g[i] = NULL;
	}
}

static void prefs_close_deferred(void *p)
{
	(void)p;
	/* Legacy scheduled dispose path — now just hide. */
	prefs_hide();
}

static void prefs_request_close(void)
{
	if (g[OID_WIN] == NULL) {
		return;
	}
	/*
	 * Defer hide so we leave the CloseRequest / button notify before
	 * touching Window_Open (same rule as corewindow dispose deferral).
	 */
	if (prefs_closing) {
		return;
	}
	prefs_closing = true;
	tsunami_schedule(0, prefs_close_deferred, NULL);
}

static void SAVEDS ASM
prefs_close_func(REG(a0, struct Hook *hook),
		 REG(a2, Object *obj),
		 REG(a1, APTR arg))
{
	(void)hook;
	(void)obj;
	(void)arg;
	prefs_request_close();
}

static void SAVEDS ASM
prefs_use_func(REG(a0, struct Hook *hook),
	       REG(a2, Object *obj),
	       REG(a1, APTR arg))
{
	(void)hook;
	(void)obj;
	(void)arg;
	prefs_apply();
	prefs_request_close();
}

static void SAVEDS ASM
prefs_save_func(REG(a0, struct Hook *hook),
		REG(a2, Object *obj),
		REG(a1, APTR arg))
{
	(void)hook;
	(void)obj;
	(void)arg;
	prefs_apply();
	nsoption_write("PROGDIR:Resources/Choices", NULL, NULL);
	prefs_request_close();
}

static void SAVEDS ASM
home_def_func(REG(a0, struct Hook *hook),
	      REG(a2, Object *obj),
	      REG(a1, APTR arg))
{
	(void)hook;
	(void)obj;
	(void)arg;
	if (g[OID_HOMEPAGE] != NULL) {
		set(g[OID_HOMEPAGE], MUIA_String_Contents, NETSURF_HOMEPAGE);
	}
}

static void SAVEDS ASM
home_cur_func(REG(a0, struct Hook *hook),
	      REG(a2, Object *obj),
	      REG(a1, APTR arg))
{
	struct gui_window *gw;
	nsurl *u;

	(void)hook;
	(void)obj;
	(void)arg;
	gw = tsunami_gui_get_active();
	if (gw == NULL || gw->bw == NULL || g[OID_HOMEPAGE] == NULL) {
		return;
	}
	u = browser_window_access_url(gw->bw);
	if (u != NULL) {
		set(g[OID_HOMEPAGE], MUIA_String_Contents, nsurl_access(u));
	}
}

static void SAVEDS ASM
home_blk_func(REG(a0, struct Hook *hook),
	      REG(a2, Object *obj),
	      REG(a1, APTR arg))
{
	(void)hook;
	(void)obj;
	(void)arg;
	if (g[OID_HOMEPAGE] != NULL) {
		set(g[OID_HOMEPAGE], MUIA_String_Contents, "about:blank");
	}
}

static void SAVEDS ASM
locale_func(REG(a0, struct Hook *hook),
	    REG(a2, Object *obj),
	    REG(a1, APTR arg))
{
	(void)hook;
	(void)obj;
	(void)arg;
	prefs_sync_locale_disable();
}

static void SAVEDS ASM
proxy_func(REG(a0, struct Hook *hook),
	   REG(a2, Object *obj),
	   REG(a1, APTR arg))
{
	(void)hook;
	(void)obj;
	(void)arg;
	prefs_sync_proxy_disable();
}

/* ---------- page builders ---------- */

static Object *page_general(void)
{
	const char *home;
	const char *lang;

	home = nsoption_charp(homepage_url);
	lang = nsoption_charp(accept_language);
	if (home == NULL) {
		home = "";
	}
	if (lang == NULL) {
		lang = "";
	}

	g[OID_HOMEPAGE] = prefs_string(home, 512);
	g[OID_HOME_DEFAULT] = SimpleButton("Default");
	g[OID_HOME_CURRENT] = SimpleButton("Current");
	g[OID_HOME_BLANK] = SimpleButton("Blank");
	g[OID_HIDEADS] = prefs_check("Block advertisements",
		nsoption_bool(block_advertisements));
	g[OID_CONTENTLANG] = prefs_string(lang, 64);
	g[OID_FROMLOCALE] = prefs_check("From locale",
		nsoption_bool(accept_lang_locale));
	g[OID_HISTORY] = prefs_intstr(nsoption_int(expire_url));
	g[OID_JAVASCRIPT] = prefs_check("JavaScript",
		nsoption_bool(enable_javascript));
	g[OID_REFERRAL] = prefs_check("Send referer",
		nsoption_bool(send_referer));
	g[OID_DONOTTRACK] = prefs_check("Do Not Track",
		nsoption_bool(do_not_track));

	return VGroup,
		MUIA_Group_SameWidth, FALSE,
		Child, ColGroup(2),
			Child, Label2("Homepage"),
			Child, g[OID_HOMEPAGE],
		End,
		Child, HGroup,
			Child, g[OID_HOME_DEFAULT],
			Child, g[OID_HOME_CURRENT],
			Child, g[OID_HOME_BLANK],
		End,
		Child, HGroup,
			Child, g[OID_HIDEADS],
			Child, Label2("Block advertisements"),
		End,
		Child, ColGroup(2),
			Child, Label2("Content language"),
			Child, g[OID_CONTENTLANG],
		End,
		Child, HGroup,
			Child, g[OID_FROMLOCALE],
			Child, Label2("From locale"),
		End,
		Child, ColGroup(2),
			Child, Label2("History expiry (days)"),
			Child, g[OID_HISTORY],
		End,
		Child, HGroup,
			Child, g[OID_JAVASCRIPT],
			Child, Label2("JavaScript"),
			Child, g[OID_REFERRAL],
			Child, Label2("Send referer"),
		End,
		Child, HGroup,
			Child, g[OID_DONOTTRACK],
			Child, Label2("Do Not Track"),
		End,
		Child, HVSpace,
	End;
}

static Object *page_display(void)
{
	const char *themepath;

	themepath = nsoption_charp(theme);
	if (themepath == NULL) {
		themepath = "";
	}
	g[OID_DARKMODE] = prefs_check("Prefer dark mode",
		nsoption_bool(prefer_dark_mode));
	g[OID_THEME] = prefs_string(themepath, 256);

	return VGroup,
		Child, HGroup,
			Child, g[OID_DARKMODE],
			Child, Label2("Prefer dark page theme"),
		End,
		Child, ColGroup(2),
			Child, Label2("GUI theme path"),
			Child, g[OID_THEME],
		End,
		Child, TextObject,
			MUIA_Text_Contents,
			"\33l(Amiga screen mode / public screen / OS pointers "
			"are not used by Tsunami MUI.)",
		End,
		Child, HVSpace,
	End;
}

static Object *page_network(void)
{
	ULONG proxy_active;
	const char *host;
	const char *user;
	const char *pass;
	const char *bypass;

	proxy_active = 0;
	if (nsoption_bool(http_proxy)) {
		proxy_active = (ULONG)nsoption_int(http_proxy_auth) + 1;
		if (proxy_active > 3) {
			proxy_active = 3;
		}
	}
	host = nsoption_charp(http_proxy_host);
	user = nsoption_charp(http_proxy_auth_user);
	pass = nsoption_charp(http_proxy_auth_pass);
	bypass = nsoption_charp(http_proxy_noproxy);
	if (host == NULL) {
		host = "";
	}
	if (user == NULL) {
		user = "";
	}
	if (pass == NULL) {
		pass = "";
	}
	if (bypass == NULL) {
		bypass = "";
	}

	g[OID_PROXY] = prefs_cycle(proxy_entries, proxy_active);
	g[OID_PROXY_HOST] = prefs_string(host, 256);
	g[OID_PROXY_PORT] = prefs_intstr(nsoption_int(http_proxy_port));
	g[OID_PROXY_USER] = prefs_string(user, 128);
	g[OID_PROXY_PASS] = prefs_string(pass, 128);
	g[OID_PROXY_BYPASS] = prefs_string(bypass, 256);
	g[OID_FETCHMAX] = prefs_intstr(nsoption_int(max_fetchers));
	g[OID_FETCHHOST] = prefs_intstr(nsoption_int(max_fetchers_per_host));
	g[OID_FETCHCACHE] = prefs_intstr(nsoption_int(max_cached_fetch_handles));

	return VGroup,
		Child, ColGroup(2),
			Child, Label2("HTTP proxy"),
			Child, g[OID_PROXY],
			Child, Label2("Host"),
			Child, g[OID_PROXY_HOST],
			Child, Label2("Port"),
			Child, g[OID_PROXY_PORT],
			Child, Label2("Username"),
			Child, g[OID_PROXY_USER],
			Child, Label2("Password"),
			Child, g[OID_PROXY_PASS],
			Child, Label2("No-proxy hosts"),
			Child, g[OID_PROXY_BYPASS],
			Child, Label2("Max fetchers"),
			Child, g[OID_FETCHMAX],
			Child, Label2("Max per host"),
			Child, g[OID_FETCHHOST],
			Child, Label2("Cached handles"),
			Child, g[OID_FETCHCACHE],
		End,
		Child, HVSpace,
	End;
}

static Object *page_rendering(void)
{
	ULONG dither;
	ULONG nativebm;

	dither = (ULONG)nsoption_int(dither_quality);
	if (dither > 2) {
		dither = 2;
	}
	nativebm = (ULONG)nsoption_int(cache_bitmaps);
	if (nativebm > 2) {
		nativebm = 2;
	}

	g[OID_ANIMDISABLE] = prefs_check("Disable animations",
		nsoption_bool(animate_images) ? FALSE : TRUE);
	g[OID_FOREIMG] = prefs_check("Foreground images",
		nsoption_bool(foreground_images));
	g[OID_BACKIMG] = prefs_check("Background images",
		nsoption_bool(background_images));
	g[OID_DPI_Y] = prefs_intstr(nsoption_int(screen_ydpi));
	g[OID_SCALEQ] = prefs_check("Scale quality",
		nsoption_bool(scale_quality));
	g[OID_DITHERQ] = prefs_cycle(dither_entries, dither);
	g[OID_NATIVEBM] = prefs_cycle(nativebm_entries, nativebm);

	return VGroup,
		Child, HGroup,
			Child, g[OID_ANIMDISABLE],
			Child, Label2("Disable image animations"),
		End,
		Child, HGroup,
			Child, g[OID_FOREIMG],
			Child, Label2("Foreground images"),
			Child, g[OID_BACKIMG],
			Child, Label2("Background images"),
		End,
		Child, HGroup,
			Child, g[OID_SCALEQ],
			Child, Label2("Scale quality"),
		End,
		Child, ColGroup(2),
			Child, Label2("Screen DPI (Y)"),
			Child, g[OID_DPI_Y],
			Child, Label2("Dither quality"),
			Child, g[OID_DITHERQ],
			Child, Label2("Cache bitmaps"),
			Child, g[OID_NATIVEBM],
		End,
		Child, HVSpace,
	End;
}

static Object *page_fonts(void)
{
	const char *sans;
	const char *serif;
	const char *mono;
	const char *cursive;
	const char *fantasy;
	ULONG def;
	ULONG eng;

	sans = nsoption_charp(font_sans);
	serif = nsoption_charp(font_serif);
	mono = nsoption_charp(font_mono);
	cursive = nsoption_charp(font_cursive);
	fantasy = nsoption_charp(font_fantasy);
	if (sans == NULL) {
		sans = "";
	}
	if (serif == NULL) {
		serif = "";
	}
	if (mono == NULL) {
		mono = "";
	}
	if (cursive == NULL) {
		cursive = "";
	}
	if (fantasy == NULL) {
		fantasy = "";
	}
	def = (ULONG)nsoption_int(font_default);
	if (def >= PLOT_FONT_FAMILY_SANS_SERIF) {
		def -= PLOT_FONT_FAMILY_SANS_SERIF;
	}
	if (def > 4) {
		def = 0;
	}
	eng = (ULONG)nsoption_int(font_engine);
	if (eng > 3) {
		eng = 0;
	}

	g[OID_FONT_SANS] = prefs_string(sans, 128);
	g[OID_FONT_SERIF] = prefs_string(serif, 128);
	g[OID_FONT_MONO] = prefs_string(mono, 128);
	g[OID_FONT_CURSIVE] = prefs_string(cursive, 128);
	g[OID_FONT_FANTASY] = prefs_string(fantasy, 128);
	g[OID_FONT_DEFAULT] = prefs_cycle(font_default_entries, def);
	g[OID_FONT_SIZE] = prefs_intstr(nsoption_int(font_size) / 10);
	g[OID_FONT_MINSIZE] = prefs_intstr(nsoption_int(font_min_size) / 10);
	g[OID_FONT_AA] = prefs_check("Antialiasing",
		nsoption_bool(font_antialiasing));
	g[OID_FONT_ENGINE] = prefs_cycle(font_engine_entries, eng);

	return VGroup,
		Child, ColGroup(2),
			Child, Label2("Sans-serif"),
			Child, g[OID_FONT_SANS],
			Child, Label2("Serif"),
			Child, g[OID_FONT_SERIF],
			Child, Label2("Monospace"),
			Child, g[OID_FONT_MONO],
			Child, Label2("Cursive"),
			Child, g[OID_FONT_CURSIVE],
			Child, Label2("Fantasy"),
			Child, g[OID_FONT_FANTASY],
			Child, Label2("Default family"),
			Child, g[OID_FONT_DEFAULT],
			Child, Label2("Size (pt)"),
			Child, g[OID_FONT_SIZE],
			Child, Label2("Minimum (pt)"),
			Child, g[OID_FONT_MINSIZE],
			Child, Label2("Font engine"),
			Child, g[OID_FONT_ENGINE],
		End,
		Child, HGroup,
			Child, g[OID_FONT_AA],
			Child, Label2("Antialiasing"),
		End,
		Child, HVSpace,
	End;
}

static Object *page_cache(void)
{
	g[OID_CACHE_MEM] = prefs_intstr(nsoption_int(memory_cache_size) / 1048576);
	g[OID_CACHE_DISC] = prefs_intstr((LONG)(nsoption_uint(disc_cache_size) / 1048576));

	return VGroup,
		Child, ColGroup(2),
			Child, Label2("Memory cache (MB)"),
			Child, g[OID_CACHE_MEM],
			Child, Label2("Disc cache (MB)"),
			Child, g[OID_CACHE_DISC],
		End,
		Child, TextObject,
			MUIA_Text_Contents,
			"\33lTsunami may clamp memory/fetcher limits at startup.",
		End,
		Child, HVSpace,
	End;
}

static Object *page_tabs(void)
{
	g[OID_TAB_ACTIVE] = prefs_check("Open new tabs in background",
		nsoption_bool(foreground_new) ? FALSE : TRUE);
	g[OID_TAB_2] = prefs_check("Middle-click opens tab",
		nsoption_bool(button_2_tab));
	g[OID_TAB_ALWAYS] = prefs_check("Always show tab bar",
		nsoption_bool(tab_always_show));
	g[OID_TAB_CLOSE] = prefs_check("Confirm closing multiple tabs",
		nsoption_bool(tab_close_warn));

	return VGroup,
		Child, HGroup,
			Child, g[OID_TAB_ACTIVE],
			Child, Label2("Open new tabs in background"),
		End,
		Child, HGroup,
			Child, g[OID_TAB_2],
			Child, Label2("Middle-click opens tab"),
		End,
		Child, HGroup,
			Child, g[OID_TAB_ALWAYS],
			Child, Label2("Always show tab bar"),
		End,
		Child, HGroup,
			Child, g[OID_TAB_CLOSE],
			Child, Label2("Confirm closing multiple tabs"),
		End,
		Child, HVSpace,
	End;
}

static Object *page_advanced(void)
{
	const char *dldir;
	const char *search;

	dldir = nsoption_charp(download_dir);
	search = nsoption_charp(search_web_provider);
	if (dldir == NULL) {
		dldir = "";
	}
	if (search == NULL) {
		search = "";
	}

	g[OID_OVERWRITE] = prefs_check("Ask before overwrite",
		nsoption_bool(ask_overwrite));
	g[OID_DLDIR] = prefs_string(dldir, 256);
	g[OID_STARTUP_NO_WIN] = prefs_check("No window at startup",
		nsoption_bool(startup_no_window));
	g[OID_CLOSE_NO_QUIT] = prefs_check("Close last window does not quit",
		nsoption_bool(close_no_quit));
	g[OID_CLIPBOARD] = prefs_check("Clipboard UTF-8",
		nsoption_bool(clipboard_write_utf8));
	g[OID_FASTSCROLL] = prefs_check("Faster scrolling",
		nsoption_bool(faster_scroll));
	g[OID_ENABLECSS] = prefs_check("Author-level CSS",
		nsoption_bool(author_level_css));
	g[OID_SEARCH_PROV] = prefs_string(search, 128);

	return VGroup,
		Child, HGroup,
			Child, g[OID_OVERWRITE],
			Child, Label2("Ask before overwrite"),
		End,
		Child, ColGroup(2),
			Child, Label2("Download directory"),
			Child, g[OID_DLDIR],
			Child, Label2("Search provider"),
			Child, g[OID_SEARCH_PROV],
		End,
		Child, HGroup,
			Child, g[OID_STARTUP_NO_WIN],
			Child, Label2("No window at startup"),
		End,
		Child, HGroup,
			Child, g[OID_CLOSE_NO_QUIT],
			Child, Label2("Close last window does not quit"),
		End,
		Child, HGroup,
			Child, g[OID_CLIPBOARD],
			Child, Label2("Write UTF-8 clipboard"),
		End,
		Child, HGroup,
			Child, g[OID_FASTSCROLL],
			Child, Label2("Faster scrolling"),
		End,
		Child, HGroup,
			Child, g[OID_ENABLECSS],
			Child, Label2("Author-level CSS"),
		End,
		Child, HVSpace,
	End;
}

/* ---------- open ---------- */

void ami_gui_opts_open(void)
{
	Object *page0;
	Object *page1;
	Object *page2;
	Object *page3;
	Object *page4;
	Object *page5;
	Object *page6;
	Object *page7;
	Object *sidebar;
	Object *pages;

	if (g[OID_WIN] != NULL) {
		/* Cancel a pending hide and bring the existing window back. */
		if (prefs_closing) {
			tsunami_schedule(-1, prefs_close_deferred, NULL);
			prefs_closing = false;
		}
		set(g[OID_WIN], MUIA_Window_CloseRequest, FALSE);
		set(g[OID_WIN], MUIA_Window_Open, TRUE);
		set(g[OID_WIN], MUIA_Window_Activate, TRUE);
		return;
	}
	if (tsunami_app == NULL) {
		return;
	}

	memset(g, 0, sizeof(g));

	page0 = page_general();
	page1 = page_display();
	page2 = page_network();
	page3 = page_rendering();
	page4 = page_fonts();
	page5 = page_cache();
	page6 = page_tabs();
	page7 = page_advanced();

	g[OID_USE] = SimpleButton("Use");
	g[OID_SAVE] = SimpleButton("Save");
	g[OID_CANCEL] = SimpleButton("Cancel");

	/*
	 * Mac-style settings: category Listview on the left, PageGroup on
	 * the right. List Active index drives Group_ActivePage.
	 */
	{
		Object *catlist;

		catlist = ListObject,
			InputListFrame,
			MUIA_List_SourceArray, register_titles,
			MUIA_List_Active, 0,
		End;
		g[OID_CATLIST] = ListviewObject,
			MUIA_Listview_List, catlist,
			MUIA_CycleChain, TRUE,
			MUIA_Listview_MultiSelect, MUIV_Listview_MultiSelect_None,
		End;

		pages = PageGroup,
			MUIA_Group_ActivePage, 0,
			MUIA_Weight, 80,
			Child, page0,
			Child, page1,
			Child, page2,
			Child, page3,
			Child, page4,
			Child, page5,
			Child, page6,
			Child, page7,
		End;
		g[OID_PAGES] = pages;

		sidebar = VGroup,
			MUIA_Weight, 20,
			Child, TextObject,
				MUIA_Text_Contents, "\33c\33bSettings",
			End,
			Child, g[OID_CATLIST],
		End;

		g[OID_WIN] = WindowObject,
			MUIA_Window_Title, "Tsunami Preferences",
			MUIA_Window_ID, MAKE_ID('T','S','P','R'),
			MUIA_Window_CloseGadget, TRUE,
			MUIA_Window_Width, MUIV_Window_Width_Visible(75),
			MUIA_Window_Height, MUIV_Window_Height_Visible(70),
			WindowContents, VGroup,
				Child, HGroup,
					Child, sidebar,
					Child, BalanceObject, End,
					Child, pages,
				End,
				Child, HGroup,
					Child, HVSpace,
					Child, g[OID_USE],
					Child, g[OID_SAVE],
					Child, g[OID_CANCEL],
				End,
			End,
		End;

		if (g[OID_WIN] == NULL) {
			memset(g, 0, sizeof(g));
			return;
		}

		close_hook.h_Entry = (HOOKFUNC)prefs_close_func;
		use_hook.h_Entry = (HOOKFUNC)prefs_use_func;
		save_hook.h_Entry = (HOOKFUNC)prefs_save_func;
		home_def_hook.h_Entry = (HOOKFUNC)home_def_func;
		home_cur_hook.h_Entry = (HOOKFUNC)home_cur_func;
		home_blk_hook.h_Entry = (HOOKFUNC)home_blk_func;
		locale_hook.h_Entry = (HOOKFUNC)locale_func;
		proxy_hook.h_Entry = (HOOKFUNC)proxy_func;

		DoMethod(g[OID_WIN], MUIM_Notify, MUIA_Window_CloseRequest, TRUE,
			 g[OID_WIN], 3, MUIM_CallHook, &close_hook, 0);
		DoMethod(g[OID_CANCEL], MUIM_Notify, MUIA_Pressed, FALSE,
			 g[OID_WIN], 3, MUIM_CallHook, &close_hook, 0);
		DoMethod(g[OID_USE], MUIM_Notify, MUIA_Pressed, FALSE,
			 g[OID_WIN], 3, MUIM_CallHook, &use_hook, 0);
		DoMethod(g[OID_SAVE], MUIM_Notify, MUIA_Pressed, FALSE,
			 g[OID_WIN], 3, MUIM_CallHook, &save_hook, 0);
		DoMethod(g[OID_HOME_DEFAULT], MUIM_Notify, MUIA_Pressed, FALSE,
			 g[OID_WIN], 3, MUIM_CallHook, &home_def_hook, 0);
		DoMethod(g[OID_HOME_CURRENT], MUIM_Notify, MUIA_Pressed, FALSE,
			 g[OID_WIN], 3, MUIM_CallHook, &home_cur_hook, 0);
		DoMethod(g[OID_HOME_BLANK], MUIM_Notify, MUIA_Pressed, FALSE,
			 g[OID_WIN], 3, MUIM_CallHook, &home_blk_hook, 0);
		DoMethod(g[OID_FROMLOCALE], MUIM_Notify, MUIA_Selected, MUIV_EveryTime,
			 g[OID_WIN], 3, MUIM_CallHook, &locale_hook, 0);
		DoMethod(g[OID_PROXY], MUIM_Notify, MUIA_Cycle_Active, MUIV_EveryTime,
			 g[OID_WIN], 3, MUIM_CallHook, &proxy_hook, 0);

		/* Sidebar list → settings page (notify on List, not Listview). */
		DoMethod(catlist, MUIM_Notify, MUIA_List_Active, MUIV_EveryTime,
			 pages, 3, MUIM_Set, MUIA_Group_ActivePage,
			 MUIV_TriggerValue);

		prefs_sync_proxy_disable();
		prefs_sync_locale_disable();

		DoMethod(tsunami_app, OM_ADDMEMBER, g[OID_WIN]);
		set(g[OID_WIN], MUIA_Window_CloseRequest, FALSE);
		set(g[OID_WIN], MUIA_Window_Open, TRUE);
	}
}

void ami_gui_opts_fini(void)
{
	prefs_dispose();
}

struct List *ami_gui_opts_websearch(int *idx)
{
	(void)idx;
	return NULL;
}

void ami_gui_opts_websearch_free(struct List *websearchlist)
{
	(void)websearchlist;
}
