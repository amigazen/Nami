/*
 * Copyright 2026 amigazen project
 *
 * This file is part of NetSurf / Tsunami (MUI frontend).
 *
 * Process entry: register NetSurf operation tables, initialise resources,
 * open the MUI application, and run the NewInput loop.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include <proto/dos.h>
#include <proto/exec.h>

#include "utils/config.h"
#include "utils/log.h"
#include "utils/messages.h"
#include "utils/filepath.h"
#include "utils/nsoption.h"
#include "utils/nsurl.h"
#include "utils/file.h"
#include "netsurf/misc.h"
#include "netsurf/netsurf.h"
#include "netsurf/url_db.h"
#include "netsurf/cookie_db.h"
#include "netsurf/bitmap.h"
#include "content/backing_store.h"

#include "amiga/font.h"
#include "amiga/bitmap.h"
#include "amiga/datatypes.h"
#include "amiga/filetype.h"

extern struct gui_utf8_table *amiga_utf8_table;
extern struct core_window_table *amiga_core_window_table;
extern struct gui_clipboard_table *amiga_clipboard_table;
extern struct gui_search_table *amiga_search_table;

#include "mui/libs.h"
#include "mui/gui.h"
#include "mui/misc.h"
#include "mui/schedule.h"
#include "mui/fetch.h"
#include "mui/filetype.h"
#include "mui/vbcc_defs.h"

#include "desktop/hotlist.h"
#include "desktop/page-info.h"
#include "netsurf/clipboard.h"
#include "netsurf/search.h"
#include "netsurf/core_window.h"

/**
 * Match Amiga FE OS3 font policy after Choices may have forced TT names.
 *
 * Amiga FE uses Compugraphic via bullet.library (bitmap_fonts=false).
 * ami_font_init() installs CGTriumvirate/CGTimes/Courier when that path runs.
 */
static void tsunami_match_amiga_fonts(void)
{
	nsoption_set_bool(bitmap_fonts, false);
	nsoption_set_bool(font_antialiasing, false);
	nsoption_set_int(font_engine, AMI_FONTENG_BULLET);
}

/**
 * Fatal error — print and exit.
 */
static void die(const char *error)
{
	fprintf(stderr, "Tsunami: %s\n", error);
	exit(EXIT_FAILURE);
}

/**
 * Build resource path vector for Amiga.
 *
 * Use POSIX-style /PROGDIR/... paths so filepath_* (colon-split + realpath)
 * never treats Amiga volume names as path separators, and never invents a
 * ".tsunami" volume from a Unix ${HOME}/.tsunami path.
 */
static char **tsunami_init_resource(void)
{
	char **pathv;
	char **respath;
	static const char *langv[] = { "en", "C", NULL };

	pathv = calloc(4, sizeof(char *));
	if (pathv == NULL) {
		return NULL;
	}

	pathv[0] = strdup("/PROGDIR/Resources");
	pathv[1] = strdup("PROGDIR:Resources");
	if (pathv[0] == NULL || pathv[1] == NULL) {
		filepath_free_strvec(pathv);
		return NULL;
	}

	respath = filepath_generate(pathv, langv);
	filepath_free_strvec(pathv);

	if (respath != NULL && respath[0] == NULL) {
		filepath_free_strvec(respath);
		respath = calloc(4, sizeof(char *));
		if (respath != NULL) {
			respath[0] = strdup("/PROGDIR/Resources/en");
			respath[1] = strdup("/PROGDIR/Resources");
			respath[2] = strdup("PROGDIR:Resources");
		}
	}

	return respath;
}

/**
 * Load AISSClassic (or Default) Theme messages for toolbar icon keys.
 */
static void tsunami_theme_init(void)
{
	char themefile[256];
	BPTR lock;

	if (nsoption_charp(theme) == NULL) {
		nsoption_set_charp(theme,
				   strdup("PROGDIR:Resources/Themes/AISSClassic"));
	}

	strncpy(themefile, nsoption_charp(theme), sizeof(themefile) - 1);
	themefile[sizeof(themefile) - 1] = '\0';
	AddPart(themefile, "Theme", sizeof(themefile));

	lock = Lock(themefile, ACCESS_READ);
	if (lock == 0) {
		NSLOG(netsurf, INFO, "Theme %s missing — using Default",
		      themefile);
		strcpy(themefile, "PROGDIR:Resources/Themes/Default/Theme");
		nsoption_set_charp(theme,
				   strdup("PROGDIR:Resources/Themes/Default"));
	} else {
		UnLock(lock);
	}

	messages_add_from_file(themefile);
	NSLOG(netsurf, INFO, "Theme file %s", themefile);
}

/**
 * Configure default nsoption paths for Amiga (must run before netsurf_init
 * so amihttp sees ca_bundle).
 */
static nserror set_defaults(struct nsoption_s *defaults)
{
	BPTR ca_lock;

	(void)defaults;
	nsoption_setnull_charp(cookie_file, strdup("PROGDIR:Cookies"));
	nsoption_setnull_charp(cookie_jar, strdup("PROGDIR:Cookies"));
	nsoption_setnull_charp(url_file, strdup("PROGDIR:URLs"));
	nsoption_setnull_charp(hotlist_file, strdup("PROGDIR:Hotlist"));
	nsoption_setnull_charp(homepage_url, strdup(NETSURF_HOMEPAGE));
	nsoption_setnull_charp(theme,
			       strdup("PROGDIR:Resources/Themes/AISSClassic"));

	/*
	 * Prefer shipped PROGDIR CA bundle; fall back to shared system
	 * bundle if PROGDIR is absent.
	 */
	ca_lock = Lock("PROGDIR:Resources/ca-bundle", ACCESS_READ);
	if (ca_lock != 0) {
		UnLock(ca_lock);
		nsoption_setnull_charp(ca_bundle,
				       strdup("PROGDIR:Resources/ca-bundle"));
	} else {
		ca_lock = Lock("AWeb:Certs/cacert.pem", ACCESS_READ);
		if (ca_lock != 0) {
			UnLock(ca_lock);
			nsoption_setnull_charp(ca_bundle,
					       strdup("AWeb:Certs/cacert.pem"));
		} else {
			nsoption_setnull_charp(ca_bundle,
					       strdup("PROGDIR:Resources/ca-bundle"));
		}
	}

	/*
	 * Match Amiga FE OS3: Compugraphic via bullet.library (not diskfont).
	 * Diskfont Topaz/Helvetica looks poor; bullet is what Nami/Amiga uses.
	 */
	nsoption_set_bool(bitmap_fonts, false);
	nsoption_set_bool(font_antialiasing, false);
	nsoption_set_int(font_engine, AMI_FONTENG_BULLET);
	nsoption_set_bool(download_notify, false);
	nsoption_set_uint(disc_cache_size, 0);
	nsoption_set_int(memory_cache_size, 768 * 1024);
	nsoption_set_int(max_fetchers, 8);
	nsoption_set_int(max_fetchers_per_host, 2);
	nsoption_set_int(max_cached_fetch_handles, 2);
	nsoption_set_int(cache_bitmaps, 2);
	/* OS3: tile size 0 = single buffer >= screen (Amiga FE ami_set_screen_defaults). */
	nsoption_set_int(redraw_tile_size_x, 0);
	nsoption_set_int(redraw_tile_size_y, 0);

	return NSERROR_OK;
}

static bool nslog_stream_configure(FILE *fptr)
{
	setbuf(fptr, NULL);
	return true;
}

int main(int argc, char **argv)
{
	char *messages;
	char *options;
	char buf[PATH_MAX];
	nserror ret;
	struct nsurl *url;
	struct netsurf_table tsunami_table;

	memset(&tsunami_table, 0, sizeof(tsunami_table));
	tsunami_table.misc = tsunami_misc_table;
	tsunami_table.window = tsunami_window_table;
	tsunami_table.fetch = tsunami_fetch_table;
	tsunami_table.bitmap = amiga_bitmap_table;
	tsunami_table.layout = ami_layout_table;
	tsunami_table.utf8 = amiga_utf8_table;
	tsunami_table.llcache = filesystem_llcache_table;
	tsunami_table.file = tsunami_file_table_get();
	tsunami_table.corewindow = amiga_core_window_table;
	tsunami_table.clipboard = amiga_clipboard_table;
	tsunami_table.search = amiga_search_table;

	if (!tsunami_libs_open()) {
		die("Failed to open intuition/graphics/diskfont/muimaster");
	}

	ret = netsurf_register(&tsunami_table);
	if (ret != NSERROR_OK) {
		tsunami_libs_close();
		die("NetSurf operation table failed registration");
	}

	tsunami_respaths = tsunami_init_resource();
	if (tsunami_respaths == NULL) {
		die("Failed to initialise resource paths");
	}

	nslog_init(nslog_stream_configure, &argc, argv);

	if (verbose_log == false) {
		char *force_argv[4];
		int force_argc;

		force_argv[0] = (char *)"Tsunami";
		force_argv[1] = (char *)"-V";
		force_argv[2] = (char *)"PROGDIR:ns.log";
		force_argv[3] = NULL;
		force_argc = 3;
		nslog_init(nslog_stream_configure, &force_argc, force_argv);
	}

	NSLOG(netsurf, INFO, "Tsunami starting");

	/* Match Amiga FE: soft bitmaps are ARGB for cybergraphics blit. */
	{
		bitmap_fmt_t bf;

		memset(&bf, 0, sizeof(bf));
		bf.layout = BITMAP_LAYOUT_ARGB8888;
		bitmap_set_format(&bf);
	}

	ret = nsoption_init(set_defaults, &nsoptions, &nsoptions_default);
	if (ret != NSERROR_OK) {
		die("Options failed to initialise");
	}
	options = filepath_find(tsunami_respaths, "Choices");
	nsoption_read(options, nsoptions);
	free(options);
	nsoption_commandline(&argc, argv, nsoptions);

	/* Choices may re-enable hostile desktop defaults — clamp like Amiga FE. */
	nsoption_set_uint(disc_cache_size, 0);
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
	nsoption_set_int(redraw_tile_size_x, 0);
	nsoption_set_int(redraw_tile_size_y, 0);
	/* Re-assert Amiga FE font engine after Choices. */
	tsunami_match_amiga_fonts();
	/* Choices often still has about:welcome (Nami); force Tsunami home. */
	nsoption_set_charp(homepage_url,
			   strdup("resource:welcome-tsunami.html"));

	NSLOG(netsurf, INFO,
	      "ca_bundle=%s cache=%d fetchers=%d/%d tile=%dx%d",
	      nsoption_charp(ca_bundle) != NULL ?
	      nsoption_charp(ca_bundle) : "(null)",
	      nsoption_int(memory_cache_size),
	      nsoption_int(max_fetchers),
	      nsoption_int(max_fetchers_per_host),
	      nsoption_int(redraw_tile_size_x),
	      nsoption_int(redraw_tile_size_y));

	messages = filepath_find(tsunami_respaths, "Messages");
	ret = messages_add_from_file(messages);
	if (ret != NSERROR_OK) {
		NSLOG(netsurf, INFO, "Messages failed to load from %s",
		      messages != NULL ? messages : "(null)");
	}

	tsunami_theme_init();

	/* After Choices: same bullet/CG faces as Amiga FE on OS3. */
	tsunami_match_amiga_fonts();

	ret = netsurf_init(NULL);
	free(messages);
	if (ret != NSERROR_OK) {
		die("NetSurf failed to initialise");
	}

	ami_font_init();
	ami_font_setdevicedpi(0);
	NSLOG(netsurf, INFO,
	      "Font engine ready (ami_nsfont=%p) sans=%s engine=%d bitmap=%d",
	      (void *)ami_nsfont,
	      nsoption_charp(font_sans) != NULL ? nsoption_charp(font_sans) : "(null)",
	      nsoption_int(font_engine),
	      nsoption_bool(bitmap_fonts) ? 1 : 0);

	ami_mime_init("PROGDIR:Resources/mimetypes");
	ret = amiga_datatypes_init();
	if (ret != NSERROR_OK) {
		NSLOG(netsurf, WARNING,
		      "amiga_datatypes_init failed (%d) — images may not display",
		      ret);
	} else {
		NSLOG(netsurf, INFO, "DataTypes picture handlers registered");
	}

	filepath_sfinddef(tsunami_respaths, buf, "mime.types",
			  "/PROGDIR/Resources");
	tsunami_fetch_filetype_init(buf);

	urldb_load(nsoption_charp(url_file));
	urldb_load_cookies(nsoption_charp(cookie_file));

	hotlist_init(nsoption_charp(hotlist_file),
		     nsoption_charp(hotlist_file));
	page_info_init();

	if (!tsunami_gui_init()) {
		die("Failed to create MUI application");
	}

	url = NULL;
	if (argc > 1) {
		nsurl_create(argv[1], &url);
	} else if (nsoption_charp(homepage_url) != NULL) {
		nsurl_create(nsoption_charp(homepage_url), &url);
	} else {
		nsurl_create(NETSURF_HOMEPAGE, &url);
	}

	NSLOG(netsurf, INFO, "Opening initial URL %s",
	      url != NULL ? nsurl_access(url) : "(none)");

	ret = tsunami_gui_open_initial(url);
	if (url != NULL) {
		nsurl_unref(url);
	}
	if (ret != NSERROR_OK) {
		die("Failed to open initial browser window");
	}

	tsunami_gui_run();

	tsunami_gui_fini();
	hotlist_fini();
	page_info_fini();
	ami_font_fini();
	tsunami_fetch_filetype_fin();

	urldb_save_cookies(nsoption_charp(cookie_file));
	urldb_save(nsoption_charp(url_file));

	netsurf_exit();
	nsoption_finalise(nsoptions, nsoptions_default);
	nslog_finalise();
	tsunami_libs_close();

	return 0;
}
