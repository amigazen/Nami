/*
 * Copyright 2010 Chris Young <chris@unsatisfactorysoftware.co.uk>
 * Copyright 2026 amigazen project
 *
 * Theme helpers — copied from frontends/amiga/theme.c /
 * frontends/amiga/gui.c (ami_get_theme_filename + ami_gui_theme_bitmap).
 */

#include "mui/os3support.h"

#include <string.h>
#include <stddef.h>
#include <stdbool.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <intuition/imageclass.h>
#include <images/bitmap.h>

#include "utils/messages.h"
#include "utils/nsoption.h"
#include "mui/libs.h"
#include "mui/theme.h"

void ami_get_theme_filename(char *filename, const char *themestring, bool protocol)
{
	if (protocol) {
		strcpy(filename, "file:///");
	} else {
		strcpy(filename, "");
	}

	if (messages_get(themestring)[0] == '*') {
		strncat(filename, messages_get(themestring) + 1, 100);
	} else {
		strcat(filename, nsoption_charp(theme));
		AddPart(filename, messages_get(themestring), 100);
	}
}

/**
 * Load a theme BitMapObj; reject zero-size (missing AISS file).
 * fallback_key may be another messages key (e.g. theme_closetab).
 * If a TBImages:list_FOO path fails, also try TBImages:FOO.
 * Copied from frontends/amiga/gui.c ami_gui_theme_bitmap().
 */
Object *ami_gui_theme_bitmap(struct Screen *scrn,
			     const char *theme_key,
			     const char *fallback_key)
{
	char path[100];
	char alt[100];
	Object *bmo;
	struct Image *im;
	char *listp;
	size_t prefix;

	bmo = NULL;
	im = NULL;
	path[0] = '\0';
	alt[0] = '\0';
	listp = NULL;
	prefix = 0;

	if (BitMapClass == NULL || scrn == NULL) {
		return NULL;
	}

	if (theme_key != NULL) {
		ami_get_theme_filename(path, theme_key, false);
		if (path[0] != '\0') {
			bmo = BitMapObj,
					BITMAP_SourceFile, path,
					BITMAP_Screen, scrn,
					BITMAP_Masking, TRUE,
				BitMapEnd;
		}
	}

	im = (struct Image *)bmo;
	if (bmo != NULL && (im == NULL || im->Width < 1 || im->Height < 1)) {
		DisposeObject(bmo);
		bmo = NULL;
	}

	/* list_close missing → try close, etc. */
	if (bmo == NULL && path[0] != '\0') {
		listp = strstr(path, "list_");
		if (listp != NULL) {
			prefix = (size_t)(listp - path);
			if (prefix + strlen(listp + 5) + 1 < sizeof(alt)) {
				memcpy(alt, path, prefix);
				strcpy(alt + prefix, listp + 5);
				bmo = BitMapObj,
						BITMAP_SourceFile, alt,
						BITMAP_Screen, scrn,
						BITMAP_Masking, TRUE,
					BitMapEnd;
				im = (struct Image *)bmo;
				if (bmo != NULL &&
				    (im == NULL || im->Width < 1 ||
				     im->Height < 1)) {
					DisposeObject(bmo);
					bmo = NULL;
				}
			}
		}
	}

	/*
	 * AISS: prefer toolbar.  If missing, try selecttoggle then toggle —
	 * never fall back to "tool" (wrong glyph).
	 */
	if (bmo == NULL && path[0] != '\0') {
		listp = strstr(path, "toolbar");
		if (listp != NULL && strcmp(listp, "toolbar") == 0) {
			prefix = (size_t)(listp - path);
			if (prefix + 14 < sizeof(alt)) {
				memcpy(alt, path, prefix);
				strcpy(alt + prefix, "selecttoggle");
				bmo = BitMapObj,
						BITMAP_SourceFile, alt,
						BITMAP_Screen, scrn,
						BITMAP_Masking, TRUE,
					BitMapEnd;
				im = (struct Image *)bmo;
				if (bmo != NULL &&
				    (im == NULL || im->Width < 1 ||
				     im->Height < 1)) {
					DisposeObject(bmo);
					bmo = NULL;
				}
			}
			if (bmo == NULL && prefix + 7 < sizeof(alt)) {
				memcpy(alt, path, prefix);
				strcpy(alt + prefix, "toggle");
				bmo = BitMapObj,
						BITMAP_SourceFile, alt,
						BITMAP_Screen, scrn,
						BITMAP_Masking, TRUE,
					BitMapEnd;
				im = (struct Image *)bmo;
				if (bmo != NULL &&
				    (im == NULL || im->Width < 1 ||
				     im->Height < 1)) {
					DisposeObject(bmo);
					bmo = NULL;
				}
			}
		}
	}

	if (bmo == NULL && fallback_key != NULL &&
	    (theme_key == NULL || strcmp(theme_key, fallback_key) != 0)) {
		ami_get_theme_filename(path, fallback_key, false);
		if (path[0] != '\0') {
			bmo = BitMapObj,
					BITMAP_SourceFile, path,
					BITMAP_Screen, scrn,
					BITMAP_Masking, TRUE,
				BitMapEnd;
			im = (struct Image *)bmo;
			if (bmo != NULL &&
			    (im == NULL || im->Width < 1 || im->Height < 1)) {
				DisposeObject(bmo);
				bmo = NULL;
			}
		}
	}

	return bmo;
}
