/*
 * Copyright 2020 Chris Young <chris@unsatisfactorysoftware.co.uk>
 * Copyright 2026 amigazen project
 *
 * Forked from frontends/amiga/pageinfo.c for Tsunami MUI.
 */

#include "mui/os3support.h"

#include <stdlib.h>
#include <stdbool.h>

#include <proto/muimaster.h>
#include <libraries/mui.h>

#include "desktop/page-info.h"
#include "netsurf/browser_window.h"
#include "netsurf/keypress.h"
#include "netsurf/plotters.h"
#include "utils/log.h"
#include "utils/messages.h"

#include "amiga/utf8.h"
#include "mui/corewindow.h"
#include "mui/pageinfo.h"

struct ami_pageinfo_window {
	struct ami_corewindow core;
	struct page_info *pi;
};

static void ami_pageinfo_destroy(struct ami_corewindow *ami_cw)
{
	struct ami_pageinfo_window *pageinfo_win;

	pageinfo_win = (struct ami_pageinfo_window *)ami_cw;
	if (pageinfo_win->pi != NULL) {
		page_info_destroy(pageinfo_win->pi);
		pageinfo_win->pi = NULL;
	}
	ami_corewindow_fini(&pageinfo_win->core);
	free(pageinfo_win);
}

static nserror
ami_pageinfo_mouse(struct ami_corewindow *ami_cw,
		   browser_mouse_state mouse_state, int x, int y)
{
	bool did_something;
	struct ami_pageinfo_window *pageinfo_win;

	pageinfo_win = (struct ami_pageinfo_window *)ami_cw;
	did_something = false;
	if (page_info_mouse_action(pageinfo_win->pi, mouse_state, x, y,
				   &did_something) == NSERROR_OK) {
		if (did_something == true) {
			/* Same deferred-close path as CloseGadget. */
			ami_corewindow_request_close(ami_cw);
		}
	}
	return NSERROR_OK;
}

static nserror
ami_pageinfo_key(struct ami_corewindow *ami_cw, uint32_t nskey)
{
	struct ami_pageinfo_window *pageinfo_win;

	pageinfo_win = (struct ami_pageinfo_window *)ami_cw;
	if (page_info_keypress(pageinfo_win->pi, nskey)) {
		return NSERROR_OK;
	}
	return NSERROR_NOT_IMPLEMENTED;
}

static nserror
ami_pageinfo_draw(struct ami_corewindow *ami_cw, int x, int y,
		  struct rect *r, struct redraw_context *ctx)
{
	struct ami_pageinfo_window *pageinfo_win;

	pageinfo_win = (struct ami_pageinfo_window *)ami_cw;
	page_info_redraw(pageinfo_win->pi, x, y, r, ctx);
	return NSERROR_OK;
}

nserror ami_pageinfo_open(struct browser_window *bw, ULONG left, ULONG top)
{
	struct ami_pageinfo_window *ncwin;
	nserror res;
	int w, h;

	(void)left;
	(void)top;

	if (bw == NULL) {
		return NSERROR_BAD_PARAMETER;
	}

	ncwin = calloc(1, sizeof(*ncwin));
	if (ncwin == NULL) {
		return NSERROR_NOMEM;
	}

	ncwin->core.wintitle =
		ami_utf8_easy((char *)messages_get("PageInfo"));
	ncwin->core.draw = ami_pageinfo_draw;
	ncwin->core.key = ami_pageinfo_key;
	ncwin->core.mouse = ami_pageinfo_mouse;
	ncwin->core.close = ami_pageinfo_destroy;
	ncwin->core.event = NULL;
	ncwin->core.drag_end = NULL;
	ncwin->core.icon_drop = NULL;

	res = ami_corewindow_init(&ncwin->core);
	if (res != NSERROR_OK) {
		ami_utf8_free(ncwin->core.wintitle);
		free(ncwin);
		return res;
	}

	res = page_info_create((struct core_window *)ncwin, bw, &ncwin->pi);
	if (res != NSERROR_OK) {
		ami_corewindow_fini(&ncwin->core);
		free(ncwin);
		return res;
	}

	if (page_info_get_size(ncwin->pi, &w, &h) == NSERROR_OK) {
		amiga_core_window_table->set_extent(
			(struct core_window *)ncwin, w, h);
	}

	ami_corewindow_open(&ncwin->core);
	return NSERROR_OK;
}
