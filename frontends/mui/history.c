/*
 * Copyright 2017 Chris Young <chris@unsatisfactorysoftware.co.uk>
 * Copyright 2026 amigazen project
 *
 * Forked from frontends/amiga/history.c for Tsunami MUI corewindow.
 */

#include "mui/os3support.h"

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include <proto/muimaster.h>
#include <libraries/mui.h>

#include "desktop/global_history.h"
#include "netsurf/keypress.h"
#include "netsurf/plotters.h"
#include "utils/log.h"
#include "utils/messages.h"

#include "amiga/utf8.h"
#include "mui/corewindow.h"
#include "mui/history.h"

struct ami_history_global_window {
	struct ami_corewindow core;
};

static struct ami_history_global_window *history_window = NULL;

static nserror
ami_history_global_mouse(struct ami_corewindow *ami_cw,
			 browser_mouse_state mouse_state, int x, int y)
{
	(void)ami_cw;
	global_history_mouse_action(mouse_state, x, y);
	return NSERROR_OK;
}

static nserror
ami_history_global_key(struct ami_corewindow *ami_cw, uint32_t nskey)
{
	(void)ami_cw;
	if (global_history_keypress(nskey)) {
		return NSERROR_OK;
	}
	return NSERROR_NOT_IMPLEMENTED;
}

static nserror
ami_history_global_draw(struct ami_corewindow *ami_cw, int x, int y,
			struct rect *r, struct redraw_context *ctx)
{
	(void)ami_cw;
	global_history_redraw(x, y, r, ctx);
	return NSERROR_OK;
}

static void ami_history_global_destroy(struct ami_corewindow *ami_cw)
{
	(void)ami_cw;
	if (history_window == NULL) {
		return;
	}
	global_history_fini();
	ami_corewindow_fini(&history_window->core);
	free(history_window);
	history_window = NULL;
}

void ami_history_global_close(void)
{
	if (history_window != NULL) {
		ami_history_global_destroy(&history_window->core);
	}
}

nserror ami_history_global_present(void)
{
	struct ami_history_global_window *ncwin;
	nserror res;

	if (history_window != NULL) {
		ami_corewindow_open(&history_window->core);
		return NSERROR_OK;
	}

	ncwin = calloc(1, sizeof(*ncwin));
	if (ncwin == NULL) {
		return NSERROR_NOMEM;
	}

	ncwin->core.wintitle =
		ami_utf8_easy((char *)messages_get("GlobalHistory"));
	ncwin->core.draw = ami_history_global_draw;
	ncwin->core.key = ami_history_global_key;
	ncwin->core.mouse = ami_history_global_mouse;
	ncwin->core.close = ami_history_global_destroy;
	ncwin->core.event = NULL;
	ncwin->core.drag_end = NULL;
	ncwin->core.icon_drop = NULL;

	res = ami_corewindow_init(&ncwin->core);
	if (res != NSERROR_OK) {
		ami_utf8_free(ncwin->core.wintitle);
		free(ncwin);
		return res;
	}

	res = global_history_init((struct core_window *)ncwin);
	if (res != NSERROR_OK) {
		ami_corewindow_fini(&ncwin->core);
		free(ncwin);
		return res;
	}

	history_window = ncwin;
	ami_corewindow_open(&ncwin->core);
	return NSERROR_OK;
}
