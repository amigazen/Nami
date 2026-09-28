/*
 * Copyright 2017 Chris Young <chris@unsatisfactorysoftware.co.uk>
 * Copyright 2026 amigazen project
 *
 * Forked from frontends/amiga/cookies.c for Tsunami MUI corewindow.
 */

#include "mui/os3support.h"

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include <proto/muimaster.h>
#include <libraries/mui.h>

#include "desktop/cookie_manager.h"
#include "netsurf/keypress.h"
#include "netsurf/plotters.h"
#include "utils/log.h"
#include "utils/messages.h"

#include "amiga/utf8.h"
#include "mui/corewindow.h"
#include "mui/cookies.h"

struct ami_cookies_window {
	struct ami_corewindow core;
};

static struct ami_cookies_window *cookies_window = NULL;

static nserror
ami_cookies_mouse(struct ami_corewindow *ami_cw,
		  browser_mouse_state mouse_state, int x, int y)
{
	(void)ami_cw;
	cookie_manager_mouse_action(mouse_state, x, y);
	return NSERROR_OK;
}

static nserror
ami_cookies_key(struct ami_corewindow *ami_cw, uint32_t nskey)
{
	(void)ami_cw;
	if (cookie_manager_keypress(nskey)) {
		return NSERROR_OK;
	}
	return NSERROR_NOT_IMPLEMENTED;
}

static nserror
ami_cookies_draw(struct ami_corewindow *ami_cw, int x, int y,
		 struct rect *r, struct redraw_context *ctx)
{
	(void)ami_cw;
	cookie_manager_redraw(x, y, r, ctx);
	return NSERROR_OK;
}

static void ami_cookies_destroy(struct ami_corewindow *ami_cw)
{
	(void)ami_cw;
	if (cookies_window == NULL) {
		return;
	}
	cookie_manager_fini();
	ami_corewindow_fini(&cookies_window->core);
	free(cookies_window);
	cookies_window = NULL;
}

void ami_cookies_close(void)
{
	if (cookies_window != NULL) {
		ami_cookies_destroy(&cookies_window->core);
	}
}

nserror ami_cookies_present(const char *search_term)
{
	struct ami_cookies_window *ncwin;
	nserror res;

	(void)search_term;

	if (cookies_window != NULL) {
		ami_corewindow_open(&cookies_window->core);
		return NSERROR_OK;
	}

	ncwin = calloc(1, sizeof(*ncwin));
	if (ncwin == NULL) {
		return NSERROR_NOMEM;
	}

	ncwin->core.wintitle = ami_utf8_easy((char *)messages_get("Cookies"));
	ncwin->core.draw = ami_cookies_draw;
	ncwin->core.key = ami_cookies_key;
	ncwin->core.mouse = ami_cookies_mouse;
	ncwin->core.close = ami_cookies_destroy;
	ncwin->core.event = NULL;
	ncwin->core.drag_end = NULL;
	ncwin->core.icon_drop = NULL;

	res = ami_corewindow_init(&ncwin->core);
	if (res != NSERROR_OK) {
		ami_utf8_free(ncwin->core.wintitle);
		free(ncwin);
		return res;
	}

	res = cookie_manager_init((struct core_window *)ncwin);
	if (res != NSERROR_OK) {
		ami_corewindow_fini(&ncwin->core);
		free(ncwin);
		return res;
	}

	cookies_window = ncwin;
	ami_corewindow_open(&ncwin->core);
	return NSERROR_OK;
}
