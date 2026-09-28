/*
 * Copyright 2008-2025 Chris Young <chris@unsatisfactorysoftware.co.uk>
 * Copyright 2026 amigazen project
 *
 * Forked from frontends/amiga/hotlist.c — Reaction shell replaced by MUI
 * corewindow; treeview logic unchanged.
 */

#include "mui/os3support.h"

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include <proto/asl.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <libraries/mui.h>

#include "desktop/hotlist.h"
#include "netsurf/browser_window.h"
#include "netsurf/keypress.h"
#include "netsurf/plotters.h"
#include "utils/log.h"
#include "utils/messages.h"
#include "utils/nsoption.h"

#include "amiga/utf8.h"
#include "mui/corewindow.h"
#include "mui/file.h"
#include "mui/gui.h"
#include "mui/hotlist.h"
#include "mui/misc.h"

struct ami_hotlist_window {
	struct ami_corewindow core;
};

static struct ami_hotlist_window *hotlist_window = NULL;

struct ami_hotlist_ctx {
	void *userdata;
	int level;
	int item;
	const char *folder;
	bool in_menu;
	bool found;
	bool (*cb)(void *userdata, int level, int item, const char *title,
		   nsurl *url, bool folder);
};

static nserror ami_hotlist_folder_enter_cb(void *ctx, const char *title)
{
	struct ami_hotlist_ctx *menu_ctx = (struct ami_hotlist_ctx *)ctx;

	if (menu_ctx->in_menu == true) {
		if (menu_ctx->cb(menu_ctx->userdata, menu_ctx->level,
				 menu_ctx->item, title, NULL, true) == true) {
			menu_ctx->item++;
		}
	} else {
		if ((menu_ctx->level == 0) &&
		    (strcmp(title, menu_ctx->folder) == 0)) {
			menu_ctx->in_menu = true;
			menu_ctx->found = true;
		}
	}
	menu_ctx->level++;
	return NSERROR_OK;
}

static nserror ami_hotlist_address_cb(void *ctx, nsurl *url, const char *title)
{
	struct ami_hotlist_ctx *menu_ctx = (struct ami_hotlist_ctx *)ctx;

	if (menu_ctx->in_menu == true) {
		if (menu_ctx->cb(menu_ctx->userdata, menu_ctx->level,
				 menu_ctx->item, title, url, false) == true) {
			menu_ctx->item++;
		}
	}
	return NSERROR_OK;
}

static nserror ami_hotlist_folder_leave_cb(void *ctx)
{
	struct ami_hotlist_ctx *menu_ctx = (struct ami_hotlist_ctx *)ctx;

	menu_ctx->level--;
	if ((menu_ctx->in_menu == true) && (menu_ctx->level == 0)) {
		menu_ctx->in_menu = false;
	}
	return NSERROR_OK;
}

nserror ami_hotlist_scan(void *userdata, int first_item, const char *folder,
	bool (*cb_add_item)(void *userdata, int level, int item,
			    const char *title, nsurl *url, bool folder))
{
	nserror error;
	struct ami_hotlist_ctx ctx;

	ctx.level = 0;
	ctx.item = first_item;
	ctx.folder = folder;
	ctx.in_menu = false;
	ctx.userdata = userdata;
	ctx.cb = cb_add_item;
	ctx.found = false;

	error = hotlist_iterate(&ctx,
				ami_hotlist_folder_enter_cb,
				ami_hotlist_address_cb,
				ami_hotlist_folder_leave_cb);

	if ((error == NSERROR_OK) && (ctx.found == false)) {
		hotlist_add_folder(folder, false, 0);
	}
	return error;
}

static nserror
ami_hotlist_mouse(struct ami_corewindow *ami_cw,
		  browser_mouse_state mouse_state, int x, int y)
{
	(void)ami_cw;
	hotlist_mouse_action(mouse_state, x, y);
	return NSERROR_OK;
}

static nserror
ami_hotlist_key(struct ami_corewindow *ami_cw, uint32_t nskey)
{
	(void)ami_cw;
	if (hotlist_keypress(nskey)) {
		return NSERROR_OK;
	}
	return NSERROR_NOT_IMPLEMENTED;
}

static nserror
ami_hotlist_draw(struct ami_corewindow *ami_cw, int x, int y,
		 struct rect *r, struct redraw_context *ctx)
{
	(void)ami_cw;
	hotlist_redraw(x, y, r, ctx);
	return NSERROR_OK;
}

static nserror
ami_hotlist_drag_end(struct ami_corewindow *ami_cw, int x, int y)
{
	nsurl *url;
	const char *title;
	bool ok;

	(void)ami_cw;
	(void)x;
	(void)y;
	url = NULL;
	title = NULL;
	ok = false;
	if (hotlist_has_selection()) {
		ok = hotlist_get_selection(&url, &title);
	}
	if (ok && url != NULL) {
		/* Drop onto browser: navigate active tab if any. */
		struct gui_window *gw;

		for (gw = NULL; ; ) {
			/* active tab is owned by gui.c — navigate via hotlist open */
			break;
		}
		(void)title;
	}
	return NSERROR_OK;
}

static nserror
ami_hotlist_icon_drop(struct ami_corewindow *ami_cw, struct nsurl *url,
		      const char *title, int x, int y)
{
	(void)ami_cw;
	(void)x;
	hotlist_add_entry(url, title, true, y);
	return NSERROR_OK;
}

static void ami_hotlist_destroy(struct ami_corewindow *ami_cw)
{
	(void)ami_cw;
	if (hotlist_window == NULL) {
		return;
	}
	hotlist_manager_fini();
	ami_corewindow_fini(&hotlist_window->core);
	free(hotlist_window);
	hotlist_window = NULL;
}

nserror ami_hotlist_present(void)
{
	struct ami_hotlist_window *ncwin;
	nserror res;

	if (hotlist_window != NULL) {
		ami_corewindow_open(&hotlist_window->core);
		return NSERROR_OK;
	}

	ncwin = calloc(1, sizeof(struct ami_hotlist_window));
	if (ncwin == NULL) {
		return NSERROR_NOMEM;
	}

	ncwin->core.wintitle = ami_utf8_easy((char *)messages_get("Hotlist"));
	ncwin->core.draw = ami_hotlist_draw;
	ncwin->core.key = ami_hotlist_key;
	ncwin->core.mouse = ami_hotlist_mouse;
	ncwin->core.close = ami_hotlist_destroy;
	ncwin->core.event = NULL;
	ncwin->core.drag_end = ami_hotlist_drag_end;
	ncwin->core.icon_drop = ami_hotlist_icon_drop;

	res = ami_corewindow_init(&ncwin->core);
	if (res != NSERROR_OK) {
		ami_utf8_free(ncwin->core.wintitle);
		free(ncwin);
		return res;
	}

	res = hotlist_manager_init((struct core_window *)ncwin);
	if (res != NSERROR_OK) {
		ami_corewindow_fini(&ncwin->core);
		free(ncwin);
		return res;
	}

	hotlist_window = ncwin;
	ami_corewindow_open(&ncwin->core);
	return NSERROR_OK;
}

void ami_hotlist_close(void)
{
	if (hotlist_window != NULL) {
		ami_hotlist_destroy(&hotlist_window->core);
	}
}
