/*
 * Copyright 2008-2012 Chris Young <chris@unsatisfactorysoftware.co.uk>
 * Copyright 2026 amigazen project
 *
 * Forked from frontends/amiga/clipboard.c — IFF clipboard for Tsunami.
 */

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include <proto/iffparse.h>
#include <proto/intuition.h>
#include <proto/exec.h>

#include <datatypes/textclass.h>

#include "utils/nsoption.h"
#include "utils/utf8.h"
#include "netsurf/clipboard.h"
#include "netsurf/browser_window.h"
#include "netsurf/keypress.h"

#include "mui/clipboard.h"
#include "mui/gui.h"
#include "amiga/utf8.h"

#define ID_UTF8 MAKE_ID('U','T','F','8')

static struct IFFHandle *iffh = NULL;

static struct IFFHandle *ami_clipboard_init_internal(int unit)
{
	struct IFFHandle *iffhandle;

	iffhandle = AllocIFF();
	if (iffhandle != NULL) {
		iffhandle->iff_Stream = (ULONG)OpenClipboard(unit);
		if (iffhandle->iff_Stream != 0) {
			InitIFFasClip(iffhandle);
		}
	}
	return iffhandle;
}

void ami_clipboard_init(void)
{
	iffh = ami_clipboard_init_internal(0);
}

static void ami_clipboard_free_internal(struct IFFHandle *iffhandle)
{
	if (iffhandle == NULL) {
		return;
	}
	if (iffhandle->iff_Stream != 0) {
		CloseClipboard((struct ClipboardHandle *)iffhandle->iff_Stream);
	}
	FreeIFF(iffhandle);
}

void ami_clipboard_free(void)
{
	ami_clipboard_free_internal(iffh);
	iffh = NULL;
}

void gui_start_selection(struct gui_window *g)
{
	(void)g;
	/* Menu enable/disable handled by Tsunami Menustrip as needed. */
}

bool ami_easy_clipboard(const char *text)
{
	struct IFFHandle *ih;
	size_t len;

	if (text == NULL) {
		return false;
	}
	len = strlen(text);
	ih = ami_clipboard_init_internal(0);
	if (ih == NULL || ih->iff_Stream == 0) {
		ami_clipboard_free_internal(ih);
		return false;
	}

	if (OpenIFF(ih, IFFF_WRITE) == 0) {
		if (PushChunk(ih, ID_FTXT, ID_CHRS, len) == 0) {
			WriteChunkBytes(ih, (APTR)text, len);
			PopChunk(ih);
		}
		CloseIFF(ih);
	}
	ami_clipboard_free_internal(ih);
	return true;
}

bool ami_easy_clipboard_bitmap(struct bitmap *bitmap)
{
	(void)bitmap;
	return false;
}

void ami_drag_selection(struct gui_window *g)
{
	(void)g;
}

static void gui_get_clipboard(char **buffer, size_t *length)
{
	struct IFFHandle *ih;
	struct ContextNode *cn;
	char *buf;
	LONG r;

	*buffer = NULL;
	*length = 0;
	ih = ami_clipboard_init_internal(0);
	if (ih == NULL || ih->iff_Stream == 0) {
		ami_clipboard_free_internal(ih);
		return;
	}

	if (OpenIFF(ih, IFFF_READ) == 0) {
		if (StopChunk(ih, ID_FTXT, ID_CHRS) == 0) {
			while (TRUE) {
				r = ParseIFF(ih, IFFPARSE_SCAN);
				if (r == IFFERR_EOC) {
					continue;
				}
				if (r != 0) {
					break;
				}
				cn = CurrentChunk(ih);
				if (cn != NULL && cn->cn_Type == ID_FTXT &&
				    cn->cn_ID == ID_CHRS && cn->cn_Size > 0) {
					buf = malloc((size_t)cn->cn_Size + 1);
					if (buf != NULL) {
						ReadChunkBytes(ih, buf,
							       cn->cn_Size);
						buf[cn->cn_Size] = '\0';
						*buffer = buf;
						*length = (size_t)cn->cn_Size;
					}
					break;
				}
			}
		}
		CloseIFF(ih);
	}
	ami_clipboard_free_internal(ih);
}

static void gui_set_clipboard(const char *buffer, size_t length,
			      nsclipboard_styles styles[], int n_styles)
{
	(void)styles;
	(void)n_styles;
	if (buffer == NULL || length == 0) {
		return;
	}
	/* Ensure NUL-terminated temporary for ami_easy_clipboard */
	{
		char *tmp;

		tmp = malloc(length + 1);
		if (tmp == NULL) {
			return;
		}
		memcpy(tmp, buffer, length);
		tmp[length] = '\0';
		ami_easy_clipboard(tmp);
		free(tmp);
	}
}

static struct gui_clipboard_table clipboard_table = {
	gui_get_clipboard,
	gui_set_clipboard
};

struct gui_clipboard_table *amiga_clipboard_table = &clipboard_table;
