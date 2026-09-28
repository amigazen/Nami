/*
 * Copyright 2011 Chris Young <chris@unsatisfactorysoftware.co.uk>
 * Copyright 2026 amigazen project
 *
 * Forked from frontends/amiga/file.c for Tsunami — ASL open/save;
 * SOURCE and TEXT fully supported.
 */

#include "mui/os3support.h"

#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/muimaster.h>
#include <libraries/mui.h>
#include <libraries/asl.h>
#include <proto/asl.h>

#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

#include "utils/utils.h"
#include "utils/nsoption.h"
#include "utils/file.h"
#include "utils/messages.h"
#include "utils/nsurl.h"
#include "netsurf/browser_window.h"
#include "netsurf/content.h"
#include "content/content_factory.h"
#include "desktop/save_text.h"

#include "mui/gui.h"
#include "mui/file.h"
#include "mui/misc.h"
#include "amiga/misc.h"

struct FileRequester *filereq;
struct FileRequester *savereq;

static BOOL ami_download_check_overwrite(const char *file, struct Window *win,
					 ULONG size)
{
	(void)file;
	(void)win;
	(void)size;
	return TRUE;
}

void ami_file_open(struct gui_window *gw)
{
	char *temp;
	nsurl *url;
	struct Window *iwin;

	if (gw == NULL || gw->bw == NULL) {
		return;
	}

	iwin = NULL;
	if (gw->win != NULL) {
		get(gw->win, MUIA_Window_Window, (ULONG *)&iwin);
	}

	if (AslRequestTags(filereq,
			ASLFR_TitleText, messages_get("NetSurf"),
			ASLFR_Window, iwin,
			ASLFR_SleepWindow, TRUE,
			ASLFR_Screen, ami_gui_get_screen(),
			ASLFR_DoSaveMode, FALSE,
			ASLFR_RejectIcons, TRUE,
			TAG_DONE)) {
		temp = malloc(1024);
		if (temp != NULL) {
			strlcpy(temp, filereq->fr_Drawer, 1024);
			AddPart(temp, filereq->fr_File, 1024);
			if (netsurf_path_to_nsurl(temp, &url) != NSERROR_OK) {
				amiga_warn_user("NoMemory", 0);
			} else {
				browser_window_navigate(gw->bw, url, NULL,
							BW_NAVIGATE_HISTORY,
							NULL, NULL, NULL);
				nsurl_unref(url);
			}
			free(temp);
		}
	}
}

void ami_file_save(int type, char *fname, struct Window *win,
		   struct hlcache_handle *object,
		   struct hlcache_handle *favicon,
		   struct browser_window *bw)
{
	const uint8_t *source_data;
	size_t source_size;
	BPTR fh;

	(void)favicon;
	(void)bw;

	if (!ami_download_check_overwrite(fname, win, 0)) {
		return;
	}

	switch (type) {
	case AMINS_SAVE_SOURCE:
		source_data = content_get_source_data(object, &source_size);
		if (source_data != NULL) {
			fh = FOpen(fname, MODE_NEWFILE, 0);
			if (fh != 0) {
				FWrite(fh, (APTR)source_data, 1, source_size);
				FClose(fh);
			}
		}
		break;
	case AMINS_SAVE_TEXT:
		save_as_text(object, fname);
		break;
	default:
		amiga_warn_user("SaveIncomplete", 0);
		break;
	}

	if (object != NULL) {
		SetComment(fname, nsurl_access(hlcache_handle_get_url(object)));
	}
}

void ami_file_save_req(int type, struct gui_window *gw,
		       struct hlcache_handle *object)
{
	char *fname;
	char *initial_fname;
	char *fname_with_ext;
	bool strip_ext;
	struct Window *iwin;

	if (gw == NULL) {
		return;
	}

	fname = malloc(1024);
	if (fname == NULL) {
		return;
	}
	initial_fname = NULL;
	fname_with_ext = NULL;
	strip_ext = true;

	if (object != NULL) {
		if (type == AMINS_SAVE_SOURCE) {
			strip_ext = false;
		}
		nsurl_nice(hlcache_handle_get_url(object), &initial_fname,
			   strip_ext);
	}

	if (initial_fname != NULL) {
		fname_with_ext = malloc(strlen(initial_fname) + 5);
		if (fname_with_ext != NULL) {
			strcpy(fname_with_ext, initial_fname);
			if (type == AMINS_SAVE_TEXT ||
			    type == AMINS_SAVE_SELECTION) {
				strcat(fname_with_ext, ".txt");
			}
		}
		free(initial_fname);
	}

	iwin = NULL;
	if (gw->win != NULL) {
		get(gw->win, MUIA_Window_Window, (ULONG *)&iwin);
	}

	if (AslRequestTags(savereq,
			ASLFR_Window, iwin,
			ASLFR_SleepWindow, TRUE,
			ASLFR_TitleText, messages_get("NetSurf"),
			ASLFR_Screen, ami_gui_get_screen(),
			ASLFR_InitialFile,
				fname_with_ext != NULL ? fname_with_ext : "",
			TAG_DONE)) {
		strlcpy(fname, savereq->fr_Drawer, 1024);
		AddPart(fname, savereq->fr_File, 1024);
		ami_file_save(type, fname, iwin, object, NULL, gw->bw);
	}

	free(fname);
	if (fname_with_ext != NULL) {
		free(fname_with_ext);
	}
}

void ami_file_req_init(void)
{
	const char *initial_dir;
	Tag initial_dir_tag;

	initial_dir = nsoption_charp(download_dir);
	initial_dir_tag = ASLFR_InitialDrawer;
	if (initial_dir == NULL) {
		initial_dir_tag = TAG_IGNORE;
	}

	filereq = (struct FileRequester *)AllocAslRequest(ASL_FileRequest, NULL);
	savereq = (struct FileRequester *)AllocAslRequestTags(ASL_FileRequest,
			ASLFR_DoSaveMode, TRUE,
			ASLFR_RejectIcons, TRUE,
			initial_dir_tag, initial_dir,
			TAG_DONE);
}

void ami_file_req_free(void)
{
	if (filereq != NULL) {
		FreeAslRequest(filereq);
		filereq = NULL;
	}
	if (savereq != NULL) {
		FreeAslRequest(savereq);
		savereq = NULL;
	}
}
