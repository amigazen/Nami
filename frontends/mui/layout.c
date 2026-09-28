/*
 * Copyright 2026 amigazen project
 *
 * This file is part of NetSurf / Tsunami (MUI frontend).
 *
 * Approximate font metrics until diskfont/TTEngine layout is ported.
 */

#include <stddef.h>

#include "utils/utf8.h"
#include "netsurf/plot_style.h"
#include "netsurf/layout.h"
#include "mui/layout.h"

static nserror nsfont_width(const plot_font_style_t *fstyle,
			    const char *string, size_t length,
			    int *width)
{
	*width = (fstyle->size * utf8_bounded_length(string, length)) /
		PLOT_STYLE_SCALE;
	return NSERROR_OK;
}

static nserror nsfont_position_in_string(const plot_font_style_t *fstyle,
					 const char *string, size_t length,
					 int x, size_t *char_offset, int *actual_x)
{
	(void)string;
	*char_offset = (size_t)(x / (fstyle->size / PLOT_STYLE_SCALE));
	if (*char_offset > length) {
		*char_offset = length;
	}
	*actual_x = (int)(*char_offset * (fstyle->size / PLOT_STYLE_SCALE));
	return NSERROR_OK;
}

static nserror nsfont_split(const plot_font_style_t *fstyle,
			    const char *string, size_t length,
			    int x, size_t *char_offset, int *actual_x)
{
	int c_off;

	c_off = (int)(x / (fstyle->size / PLOT_STYLE_SCALE));
	*char_offset = (size_t)c_off;
	if (*char_offset > length) {
		*char_offset = length;
	} else {
		while (*char_offset > 0) {
			if (string[*char_offset] == ' ') {
				break;
			}
			(*char_offset)--;
		}
		if (*char_offset == 0) {
			*char_offset = (size_t)c_off;
			while (*char_offset < length &&
			       string[*char_offset] != ' ') {
				(*char_offset)++;
			}
		}
	}
	*actual_x = (int)(*char_offset * (fstyle->size / PLOT_STYLE_SCALE));
	return NSERROR_OK;
}

static struct gui_layout_table layout_table = {
	.width = nsfont_width,
	.position = nsfont_position_in_string,
	.split = nsfont_split,
};

struct gui_layout_table *tsunami_layout_table = &layout_table;
