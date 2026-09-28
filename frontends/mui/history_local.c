/*
 * Copyright 2017-2025 Chris Young <chris@unsatisfactorysoftware.co.uk>
 * Copyright 2026 amigazen project
 *
 * Forked from frontends/amiga/history_local.c for Tsunami MUI.
 */

#include "mui/os3support.h"

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include <proto/muimaster.h>
#include <libraries/mui.h>

#include "desktop/local_history.h"
#include "netsurf/keypress.h"
#include "netsurf/plotters.h"
#include "utils/log.h"
#include "utils/messages.h"

#include "amiga/utf8.h"
#include "mui/corewindow.h"
#include "mui/gui.h"
#include "mui/history_local.h"

struct ami_history_local_window {
	struct ami_corewindow core;
	struct gui_window *gw;
	struct local_history_session *session;
};

static struct ami_history_local_window *history_local_window = NULL;

nserror
ami_history_local_destroy(struct ami_history_local_window *history_local_win)
{
	if (history_local_win == NULL) {
		return NSERROR_OK;
	}
	local_history_fini(history_local_win->session);
	ami_corewindow_fini(&history_local_win->core);
	if (history_local_window == history_local_win) {
		history_local_window = NULL;
	}
	free(history_local_win);
	return NSERROR_OK;
}

static void ami_history_local_destroy_cw(struct ami_corewindow *ami_cw)
{
	ami_history_local_destroy((struct ami_history_local_window *)ami_cw);
}

static nserror
ami_history_local_mouse(struct ami_corewindow *ami_cw,
			browser_mouse_state mouse_state, int x, int y)
{
	struct ami_history_local_window *hw;

	hw = (struct ami_history_local_window *)ami_cw;
	local_history_mouse_action(hw->session, mouse_state, x, y);
	return NSERROR_OK;
}

static nserror
ami_history_local_key(struct ami_corewindow *ami_cw, uint32_t nskey)
{
	struct ami_history_local_window *hw;

	hw = (struct ami_history_local_window *)ami_cw;
	if (local_history_keypress(hw->session, nskey)) {
		return NSERROR_OK;
	}
	return NSERROR_NOT_IMPLEMENTED;
}

static nserror
ami_history_local_draw(struct ami_corewindow *ami_cw, int x, int y,
		       struct rect *r, struct redraw_context *ctx)
{
	struct ami_history_local_window *hw;

	hw = (struct ami_history_local_window *)ami_cw;
	local_history_redraw(hw->session, x, y, r, ctx);
	return NSERROR_OK;
}

void ami_history_local_close(void)
{
	if (history_local_window != NULL) {
		ami_history_local_destroy(history_local_window);
	}
}

void ami_history_local_close_if_gw(struct gui_window *gw)
{
	if (history_local_window != NULL && history_local_window->gw == gw) {
		ami_history_local_destroy(history_local_window);
	}
}

nserror ami_history_local_present(struct gui_window *gw)
{
	struct ami_history_local_window *ncwin;
	nserror res;

	if (gw == NULL || gw->bw == NULL) {
		return NSERROR_BAD_PARAMETER;
	}

	if (history_local_window != NULL) {
		history_local_window->gw = gw;
		ami_corewindow_open(&history_local_window->core);
		return NSERROR_OK;
	}

	ncwin = calloc(1, sizeof(*ncwin));
	if (ncwin == NULL) {
		return NSERROR_NOMEM;
	}

	ncwin->gw = gw;
	ncwin->core.wintitle =
		ami_utf8_easy((char *)messages_get("LocalHistory"));
	ncwin->core.draw = ami_history_local_draw;
	ncwin->core.key = ami_history_local_key;
	ncwin->core.mouse = ami_history_local_mouse;
	ncwin->core.close = ami_history_local_destroy_cw;
	ncwin->core.event = NULL;
	ncwin->core.drag_end = NULL;
	ncwin->core.icon_drop = NULL;

	res = ami_corewindow_init(&ncwin->core);
	if (res != NSERROR_OK) {
		ami_utf8_free(ncwin->core.wintitle);
		free(ncwin);
		return res;
	}

	res = local_history_init((struct core_window *)ncwin, gw->bw,
				 &ncwin->session);
	if (res != NSERROR_OK) {
		ami_corewindow_fini(&ncwin->core);
		free(ncwin);
		return res;
	}

	history_local_window = ncwin;
	ami_corewindow_open(&ncwin->core);
	return NSERROR_OK;
}
