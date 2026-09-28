/*
 * Copyright 2026 amigazen project
 *
 * Theme path / BitMapObj helpers for Tsunami (from Amiga FE).
 */

#ifndef MUI_THEME_H
#define MUI_THEME_H

#include <stdbool.h>
#include <intuition/classusr.h>

struct Screen;

void ami_get_theme_filename(char *filename, const char *themestring, bool protocol);

/**
 * Load a theme BitMapObj (images/bitmap.image), same rules as Amiga FE
 * ami_gui_theme_bitmap — reject zero-size, try list_/toolbar fallbacks.
 *
 * Caller owns the object and must DisposeObject it.
 */
Object *ami_gui_theme_bitmap(struct Screen *scrn,
			     const char *theme_key,
			     const char *fallback_key);

#endif /* MUI_THEME_H */
