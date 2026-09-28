/*
 * Copyright 2004-2008 NetSurf contributors
 * Copyright 2026 amigazen project
 *
 * Forked from frontends/amiga/search.c — MUI Find window.
 */

#include "mui/os3support.h"

#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <libraries/mui.h>
#include <utility/hooks.h>

#include "utils/log.h"
#include "utils/messages.h"
#include "netsurf/browser_window.h"
#include "netsurf/search.h"
#include "desktop/search.h"

#include "mui/gui.h"
#include "mui/schedule.h"
#include "mui/search.h"

enum {
	OID_FIND_WIN = 0,
	OID_FIND_STRING,
	OID_FIND_CASE,
	OID_FIND_SHOWALL,
	OID_FIND_PREV,
	OID_FIND_NEXT,
	OID_FIND_LAST
};

struct find_window {
	Object *objects[OID_FIND_LAST];
	struct gui_window *gwin;
	struct Hook close_hook;
	struct Hook next_hook;
	struct Hook prev_hook;
	struct Hook string_hook;
};

static struct find_window *fwin = NULL;
static bool search_insert = true;
static bool search_closing = false;

static void ami_search_close_deferred(void *p);

static void ami_search_set_status(bool found, void *p)
{
	(void)found;
	(void)p;
}

static void ami_search_set_hourglass(bool active, void *p)
{
	(void)active;
	(void)p;
}

static void ami_search_add_recent(const char *string, void *p)
{
	(void)string;
	(void)p;
}

static void ami_search_set_forward_state(bool active, void *p)
{
	(void)p;
	if (fwin != NULL && fwin->objects[OID_FIND_NEXT] != NULL) {
		set(fwin->objects[OID_FIND_NEXT], MUIA_Disabled, !active);
	}
}

static void ami_search_set_back_state(bool active, void *p)
{
	(void)p;
	if (fwin != NULL && fwin->objects[OID_FIND_PREV] != NULL) {
		set(fwin->objects[OID_FIND_PREV], MUIA_Disabled, !active);
	}
}

static struct gui_search_table search_table = {
	ami_search_set_status,
	ami_search_set_hourglass,
	ami_search_add_recent,
	ami_search_set_forward_state,
	ami_search_set_back_state
};

struct gui_search_table *amiga_search_table = &search_table;

struct gui_window *ami_search_get_gwin(struct find_window *fw)
{
	if (fw != NULL) {
		return fw->gwin;
	}
	return NULL;
}

search_flags_t ami_search_flags(void)
{
	ULONG case_sensitive;
	ULONG showall;
	search_flags_t flags;

	case_sensitive = 0;
	showall = 0;
	flags = 0;
	if (fwin == NULL) {
		return flags;
	}
	get(fwin->objects[OID_FIND_CASE], MUIA_Selected, &case_sensitive);
	get(fwin->objects[OID_FIND_SHOWALL], MUIA_Selected, &showall);
	if (case_sensitive) {
		flags |= SEARCH_FLAG_CASE_SENSITIVE;
	}
	if (showall) {
		flags |= SEARCH_FLAG_SHOWALL;
	}
	return flags;
}

char *ami_search_string(void)
{
	char *text;

	text = NULL;
	if (fwin == NULL) {
		return NULL;
	}
	get(fwin->objects[OID_FIND_STRING], MUIA_String_Contents, (ULONG *)&text);
	return text;
}

static void find_do_search(search_flags_t dir_flag)
{
	search_flags_t flags;

	if (fwin == NULL || fwin->gwin == NULL || fwin->gwin->bw == NULL) {
		return;
	}
	search_insert = true;
	flags = dir_flag | ami_search_flags();
	browser_window_search(fwin->gwin->bw, NULL, flags, ami_search_string());
}

/*
 * Hide only — same rule as prefs. Disposing and recreating the Find window
 * from CloseRequest / second open can lock classic MUI; keep the tree.
 */
static void ami_search_hide(void)
{
	if (fwin == NULL) {
		return;
	}
	if (fwin->gwin != NULL && fwin->gwin->bw != NULL) {
		browser_window_search_clear(fwin->gwin->bw);
	}
	if (fwin->objects[OID_FIND_WIN] != NULL) {
		set(fwin->objects[OID_FIND_WIN], MUIA_Window_Open, FALSE);
		set(fwin->objects[OID_FIND_WIN], MUIA_Window_CloseRequest, FALSE);
	}
	search_closing = false;
	tsunami_schedule(-1, ami_search_close_deferred, NULL);
}

static void ami_search_dispose(void)
{
	if (fwin == NULL) {
		return;
	}
	tsunami_schedule(-1, ami_search_close_deferred, NULL);
	search_closing = false;
	if (fwin->gwin != NULL && fwin->gwin->bw != NULL) {
		browser_window_search_clear(fwin->gwin->bw);
	}
	if (fwin->objects[OID_FIND_WIN] != NULL) {
		set(fwin->objects[OID_FIND_WIN], MUIA_Window_Open, FALSE);
		DoMethod(tsunami_app, OM_REMMEMBER, fwin->objects[OID_FIND_WIN]);
		MUI_DisposeObject(fwin->objects[OID_FIND_WIN]);
	}
	free(fwin);
	fwin = NULL;
}

void ami_search_close(void)
{
	ami_search_hide();
}

void ami_search_fini(void)
{
	ami_search_dispose();
}

void ami_search_close_if_gw(struct gui_window *gw)
{
	if (fwin != NULL && fwin->gwin == gw) {
		ami_search_hide();
		fwin->gwin = NULL;
	}
}

static void ami_search_close_deferred(void *p)
{
	(void)p;
	ami_search_hide();
}

static void ami_search_request_close(void)
{
	if (fwin == NULL || search_closing) {
		return;
	}
	/*
	 * Defer hide so we leave the CloseRequest notify before touching
	 * Window_Open (same rule as prefs / corewindow).
	 */
	search_closing = true;
	tsunami_schedule(0, ami_search_close_deferred, NULL);
}

static void SAVEDS ASM
find_close_func(REG(a0, struct Hook *hook),
		REG(a2, Object *obj),
		REG(a1, APTR arg))
{
	(void)hook;
	(void)obj;
	(void)arg;
	ami_search_request_close();
}

static void SAVEDS ASM
find_next_func(REG(a0, struct Hook *hook),
	       REG(a2, Object *obj),
	       REG(a1, APTR arg))
{
	(void)hook;
	(void)obj;
	(void)arg;
	find_do_search(SEARCH_FLAG_FORWARDS);
}

static void SAVEDS ASM
find_prev_func(REG(a0, struct Hook *hook),
	       REG(a2, Object *obj),
	       REG(a1, APTR arg))
{
	(void)hook;
	(void)obj;
	(void)arg;
	find_do_search(0);
}

static void SAVEDS ASM
find_string_func(REG(a0, struct Hook *hook),
		 REG(a2, Object *obj),
		 REG(a1, APTR arg))
{
	(void)hook;
	(void)obj;
	(void)arg;
	if (fwin != NULL && fwin->gwin != NULL && fwin->gwin->bw != NULL) {
		browser_window_search_clear(fwin->gwin->bw);
	}
	set(fwin->objects[OID_FIND_PREV], MUIA_Disabled, FALSE);
	set(fwin->objects[OID_FIND_NEXT], MUIA_Disabled, FALSE);
	find_do_search(SEARCH_FLAG_FORWARDS);
}

void ami_search_open(struct gui_window *gwin)
{
	Object *win;
	Object *str;
	Object *caseb;
	Object *showall;
	Object *prev;
	Object *next;

	if (gwin == NULL) {
		return;
	}

	if (fwin != NULL) {
		if (search_closing) {
			tsunami_schedule(-1, ami_search_close_deferred, NULL);
			search_closing = false;
		}
		fwin->gwin = gwin;
		set(fwin->objects[OID_FIND_WIN], MUIA_Window_CloseRequest, FALSE);
		set(fwin->objects[OID_FIND_WIN], MUIA_Window_Open, TRUE);
		set(fwin->objects[OID_FIND_WIN], MUIA_Window_Activate, TRUE);
		return;
	}

	fwin = calloc(1, sizeof(*fwin));
	if (fwin == NULL) {
		return;
	}
	fwin->gwin = gwin;

	str = StringObject,
		MUIA_Frame, MUIV_Frame_String,
		MUIA_CycleChain, TRUE,
	End;
	caseb = MUI_MakeObject(MUIO_Checkmark, (ULONG)"Case");
	showall = MUI_MakeObject(MUIO_Checkmark, (ULONG)"All");
	prev = SimpleButton("Prev");
	next = SimpleButton("Next");

	win = WindowObject,
		MUIA_Window_Title, messages_get("FindTextNS"),
		MUIA_Window_ID, MAKE_ID('T','S','F','N'),
		MUIA_Window_CloseGadget, TRUE,
		WindowContents, VGroup,
			Child, str,
			Child, HGroup,
				Child, caseb,
				Child, Label2("Case sensitive"),
				Child, showall,
				Child, Label2("Show all"),
			End,
			Child, HGroup,
				Child, prev,
				Child, next,
			End,
		End,
	End;

	if (win == NULL) {
		free(fwin);
		fwin = NULL;
		return;
	}

	fwin->objects[OID_FIND_WIN] = win;
	fwin->objects[OID_FIND_STRING] = str;
	fwin->objects[OID_FIND_CASE] = caseb;
	fwin->objects[OID_FIND_SHOWALL] = showall;
	fwin->objects[OID_FIND_PREV] = prev;
	fwin->objects[OID_FIND_NEXT] = next;

	set(prev, MUIA_Disabled, TRUE);
	set(next, MUIA_Disabled, TRUE);

	fwin->close_hook.h_Entry = (HOOKFUNC)find_close_func;
	fwin->next_hook.h_Entry = (HOOKFUNC)find_next_func;
	fwin->prev_hook.h_Entry = (HOOKFUNC)find_prev_func;
	fwin->string_hook.h_Entry = (HOOKFUNC)find_string_func;

	DoMethod(win, MUIM_Notify, MUIA_Window_CloseRequest, TRUE,
		 win, 3, MUIM_CallHook, &fwin->close_hook, 0);
	DoMethod(next, MUIM_Notify, MUIA_Pressed, FALSE,
		 win, 3, MUIM_CallHook, &fwin->next_hook, 0);
	DoMethod(prev, MUIM_Notify, MUIA_Pressed, FALSE,
		 win, 3, MUIM_CallHook, &fwin->prev_hook, 0);
	DoMethod(str, MUIM_Notify, MUIA_String_Acknowledge, MUIV_EveryTime,
		 win, 3, MUIM_CallHook, &fwin->string_hook, 0);

	DoMethod(tsunami_app, OM_ADDMEMBER, win);
	set(win, MUIA_Window_Open, TRUE);
	set(win, MUIA_Window_ActiveObject, str);
}
