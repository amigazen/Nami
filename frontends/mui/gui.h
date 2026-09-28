/*
 * Copyright 2026 amigazen project
 *
 * This file is part of NetSurf / Tsunami (MUI frontend).
 */

#ifndef MUI_GUI_H
#define MUI_GUI_H

#include <exec/types.h>
#include <intuition/classusr.h>

#include "utils/errors.h"

struct browser_window;
struct nsurl;
struct gui_window_table;

/**
 * One Tsunami browser view (NetSurf gui_window / tab page).
 *
 * Several gui_windows share one MUI Window (main_win): each owns a page
 * in the RegisterGroup plus Prop scrollbars.
 */
struct gui_window {
	struct browser_window *bw;
	Object *win;		/* Shared MUI Window (same pointer for all tabs) */
	Object *page;		/* This tab's scroll+browser group (Register child) */
	Object *browser;	/* Browser Area MCC */
	Object *hbar;		/* Horizontal ScrollbarObject */
	Object *vbar;		/* Vertical ScrollbarObject */
	Object *tab_btn;	/* Unused on MUI 3.8 Register; reserved */
	Object *url;		/* Shared URL gadget (same for all tabs) */
	Object *status;		/* Shared status text */
	Object *gauge;		/* Shared loading gauge */
	/* Reaction BitMapObjs for MUIA_Image_OldImage (dispose after Window). */
	Object *tb_bm[12];
	int tb_bm_count;
	int scrollx;
	int scrolly;
	int width;
	int height;
	int doc_w;
	int doc_h;
	int loading;
	int tab_index;
	struct gui_window *next;
};

extern Object *tsunami_app;
extern struct gui_window_table *tsunami_window_table;

/**
 * Create the MUI Application and register custom classes.
 */
BOOL tsunami_gui_init(void);

/**
 * Destroy the MUI Application and custom classes.
 */
void tsunami_gui_fini(void);

/**
 * Run the MUI NewInput event loop until quit.
 */
void tsunami_gui_run(void);

/**
 * Open the first browser window for homepage (or url).
 */
nserror tsunami_gui_open_initial(struct nsurl *url);

/**
 * Return the active tab gui_window (or first in list).
 */
struct gui_window *tsunami_gui_get_active(void);

/**
 * Refresh Prop Entries/Visible from document extents and viewport size.
 * Called from browser Area size sync and GW_EVENT_UPDATE_EXTENT.
 */
void tsunami_gui_sync_scrollbars(struct gui_window *gw);

/**
 * Advance the activity throbber if enough wall time has passed.
 * Safe to call from paint strips / long schedule callbacks while the
 * event loop cannot run — keeps the KITT eye alive during layout/paint.
 */
void tsunami_gui_throbber_pulse(void);

/** Mark nested long work (paint/layout) so pulses count as activity. */
void tsunami_gui_throbber_heavy_begin(void);
void tsunami_gui_throbber_heavy_end(void);

#endif /* MUI_GUI_H */
