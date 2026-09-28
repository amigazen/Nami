/*
 * Copyright 2017 Chris Young <chris@unsatisfactorysoftware.co.uk>
 * Copyright 2026 amigazen project
 *
 * Forked from frontends/amiga/corewindow.h for Tsunami (MUI).
 * Reaction Space/Window shells replaced by MUI Area + Window.
 */

#ifndef MUI_COREWINDOW_H
#define MUI_COREWINDOW_H

#include <exec/types.h>
#include <intuition/classusr.h>

#include "netsurf/core_window.h"
#include "netsurf/mouse.h"
#include "utils/errors.h"

struct nsurl;
struct gui_globals;
struct MinList;
struct rect;
struct redraw_context;

enum {
	GID_CW_WIN = 0,
	GID_CW_MAIN,
	GID_CW_DRAW,
	GID_CW_HSCROLL,
	GID_CW_VSCROLL,
	GID_CW_LAST
};

/**
 * Tsunami core window — same callback contract as Amiga FE so forked
 * hotlist/history/cookies/pageinfo keep their draw/key/mouse logic.
 */
struct ami_corewindow {
	Object *objects[GID_CW_LAST];
	struct Window *win;	/* Intuition window from MUIA_Window_Window */
	struct Hook scroll_hook;
	struct Hook close_hook;

	int mouse_x_click;
	int mouse_y_click;
	int mouse_state;
	int scrollx;
	int scrolly;
	int doc_w;
	int doc_h;
	int view_w;
	int view_h;

	bool close_window;
	bool redraw_scheduled;
	bool dragging;
	int drag_x_start;
	int drag_y_start;

	char *wintitle;

	struct gui_globals *gg;
	struct MinList *shared_pens;

	core_window_drag_status drag_status;

	nserror (*draw)(struct ami_corewindow *ami_cw, int x, int y,
			struct rect *r, struct redraw_context *ctx);
	nserror (*key)(struct ami_corewindow *ami_cw, uint32_t nskey);
	nserror (*mouse)(struct ami_corewindow *ami_cw,
			 browser_mouse_state mouse_state, int x, int y);
	BOOL (*event)(struct ami_corewindow *ami_cw, ULONG result);
	nserror (*drag_end)(struct ami_corewindow *ami_cw, int x, int y);
	nserror (*icon_drop)(struct ami_corewindow *ami_cw, struct nsurl *url,
			     const char *title, int x, int y);
	void (*close)(struct ami_corewindow *ami_cw);
};

extern struct core_window_table *amiga_core_window_table;

/**
 * Create MUI window + Area, allocate plot RA, open on tsunami_app.
 * Caller must set draw/key/mouse/close (and optional drag) first.
 */
nserror ami_corewindow_init(struct ami_corewindow *ami_cw);

/**
 * Open a core window previously created by ami_corewindow_init.
 * Call after the desktop manager (hotlist/cookies/…) is initialised —
 * Amiga FE opens inside ami_corewindow_init (RA_OpenWindow); MUI must
 * open after manager_init so the first paint is not empty / nested in
 * Application_NewInput.
 */
void ami_corewindow_open(struct ami_corewindow *ami_cw);

/**
 * Request destruction after the current MUI notify / NewInput returns.
 * Matches Amiga FE setting close_window and destroying from the event
 * loop — never DisposeObject from inside CloseRequest notify.
 */
void ami_corewindow_request_close(struct ami_corewindow *ami_cw);

/**
 * Close MUI window and free plot resources.
 */
nserror ami_corewindow_fini(struct ami_corewindow *ami_cw);

/**
 * Register / unregister the TsunamiCoreArea MUI class (gui init/fini).
 */
BOOL tsunami_corewindow_class_init(void);
void tsunami_corewindow_class_fini(void);

#endif /* MUI_COREWINDOW_H */
