/*
 * Copyright 2026 amigazen project
 *
 * This file is part of NetSurf / Tsunami (MUI frontend).
 */

#ifndef MUI_BROWSER_H
#define MUI_BROWSER_H

#include <exec/types.h>

struct gui_window;

/* Custom attribute: pointer to owning gui_window. */
#define MUIA_Tsunami_Browser_GuiWindow (TAG_USER | 0x540001)

/* Prop scrollbar moved — sync scrollx/scrolly and redraw (Voyager-style). */
#define MUIM_Tsunami_Browser_SyncScroll (TAG_USER | 0x540010)

/**
 * Create / delete the Browser Area custom class.
 */
BOOL tsunami_browser_class_init(void);
void tsunami_browser_class_fini(void);

/**
 * MUI class pointer for NewObject(BrowserClass, ...).
 */
extern struct MUI_CustomClass *TsunamiBrowserClass;

#define TsunamiBrowserObject NewObject(TsunamiBrowserClass->mcc_Class, NULL

/**
 * Ask the browser Area to redraw.
 */
void tsunami_browser_redraw(Object *browser);

/**
 * Clear gui_window link and cancel a pending deferred redraw before the
 * page (and this Area) are RemMember'd / disposed.
 */
void tsunami_browser_detach(Object *browser);

/**
 * Free the shared Amiga plot render area (ami_plot_ra).
 */
void tsunami_browser_plot_fini(void);

/**
 * Release palette pens (Amiga FE does this on new content).
 */
void tsunami_browser_release_pens(void);

#endif /* MUI_BROWSER_H */
