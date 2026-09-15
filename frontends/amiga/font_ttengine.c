/*
 * Copyright 2026 amigazen project
 *
 * This file is part of NetSurf, http://www.netsurf-browser.org/
 *
 * NetSurf is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * NetSurf is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * \file
 * Optional TrueType page fonts via ttengine.library.
 *
 * Uses TTEngine's family database (real names or CSS generics) and UTF-8
 * encoding so NetSurf strings need no local charset conversion.
 */

#include "amiga/os3support.h"

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/ttengine.h>
#include <libraries/ttengine.h>
#include <graphics/rpattr.h>

#include "utils/log.h"
#include "utils/nsoption.h"
#include "utils/utf8.h"

#include "amiga/font.h"
#include "amiga/font_ttengine.h"
#include "amiga/gui.h"

static APTR prev_font = NULL;
static plot_font_style_t prev_fstyle;
static bool prev_fstyle_valid = false;
static struct RastPort temp_rp;
static bool temp_rp_inited = false;
static STRPTR family_table[3];

/**
 * Map a NetSurf font family to a CSS-generic fallback name for TTEngine.
 */
static const char *
ami_font_tte_generic(plot_font_family_t family)
{
	switch (family) {
	case PLOT_FONT_FAMILY_SERIF:
		return "serif";
	case PLOT_FONT_FAMILY_MONOSPACE:
		return "monospaced";
	case PLOT_FONT_FAMILY_CURSIVE:
		return "cursive";
	case PLOT_FONT_FAMILY_FANTASY:
		return "fantasy";
	case PLOT_FONT_FAMILY_SANS_SERIF:
	default:
		return "sans-serif";
	}
}

/**
 * Preferred face name from NetSurf options for a CSS family.
 */
static char *
ami_font_tte_option_name(plot_font_family_t family)
{
	switch (family) {
	case PLOT_FONT_FAMILY_SERIF:
		return nsoption_charp(font_serif);
	case PLOT_FONT_FAMILY_MONOSPACE:
		return nsoption_charp(font_mono);
	case PLOT_FONT_FAMILY_CURSIVE:
		return nsoption_charp(font_cursive);
	case PLOT_FONT_FAMILY_FANTASY:
		return nsoption_charp(font_fantasy);
	case PLOT_FONT_FAMILY_SANS_SERIF:
	default:
		return nsoption_charp(font_sans);
	}
}

/**
 * Pixel size for TT_FontSize from a plot style.
 */
static ULONG
ami_font_tte_pixel_size(const plot_font_style_t *fstyle)
{
	ULONG fsize;

	fsize = (ULONG)(fstyle->size * nsoption_int(screen_ydpi) / 72);
	fsize /= PLOT_STYLE_SCALE;
	if (fsize < 1) {
		fsize = 1;
	}
	return fsize;
}

/**
 * Open (or reuse) a TTEngine font for fstyle and attach it to rp.
 */
static APTR
ami_font_tte_open(struct RastPort *rp, const plot_font_style_t *fstyle, bool aa)
{
	APTR font;
	char *optname;
	ULONG size;
	ULONG weight;
	ULONG style;
	ULONG soft;
	ULONG aa_mode;
	struct Screen *scrn;

	if (rp == NULL || fstyle == NULL || TTEngineBase == NULL) {
		return NULL;
	}

	if (prev_font != NULL && prev_fstyle_valid &&
	    fstyle->family == prev_fstyle.family &&
	    fstyle->size == prev_fstyle.size &&
	    fstyle->weight == prev_fstyle.weight &&
	    fstyle->flags == prev_fstyle.flags) {
		if (TT_SetFont(rp, prev_font) == FALSE) {
			return NULL;
		}
		goto apply_attrs;
	}

	if (prev_font != NULL) {
		TT_CloseFont(prev_font);
		prev_font = NULL;
		prev_fstyle_valid = false;
	}

	optname = ami_font_tte_option_name(fstyle->family);
	family_table[0] = (optname != NULL && optname[0] != '\0') ?
		(STRPTR)optname : (STRPTR)ami_font_tte_generic(fstyle->family);
	family_table[1] = (STRPTR)ami_font_tte_generic(fstyle->family);
	family_table[2] = NULL;

	size = ami_font_tte_pixel_size(fstyle);

	weight = TT_FontWeight_Normal;
	if (fstyle->weight >= 700) {
		weight = TT_FontWeight_Bold;
	}

	style = TT_FontStyle_Regular;
	if ((fstyle->flags & FONTF_ITALIC) || (fstyle->flags & FONTF_OBLIQUE)) {
		style = TT_FontStyle_Italic;
	}

	font = TT_OpenFont(
		TT_FamilyTable, (ULONG)family_table,
		TT_FontSize, size,
		TT_FontStyle, style,
		TT_FontWeight, weight,
		TAG_DONE);
	if (font == NULL) {
		NSLOG(netsurf, INFO, "TT_OpenFont failed for %s size %lu",
		      family_table[0], (unsigned long)size);
		return NULL;
	}

	if (TT_SetFont(rp, font) == FALSE) {
		TT_CloseFont(font);
		return NULL;
	}

	prev_font = font;
	prev_fstyle = *fstyle;
	prev_fstyle_valid = true;

apply_attrs:
	aa_mode = aa ? TT_Antialias_On : TT_Antialias_Off;
	soft = TT_SoftStyle_None;

	scrn = ami_gui_get_screen();
	TT_SetAttrs(rp,
		TT_Encoding, TT_Encoding_UTF8,
		TT_Antialias, aa_mode,
		TT_SoftStyle, soft,
		TT_Foreground, TT_Foreground_UseRastPort,
		TT_Background, TT_Background_UseRastPort,
		TT_DiskFontMetrics, TRUE,
		TT_Screen, (ULONG)scrn,
		TAG_DONE);

	return prev_font;
}

static nserror
amiga_tte_nsfont_width(const plot_font_style_t *fstyle,
		       const char *string, size_t length,
		       int *width)
{
	APTR font;
	size_t nchars;

	*width = (int)length;

	font = ami_font_tte_open(&temp_rp, fstyle,
				 nsoption_bool(font_antialiasing));
	if (font == NULL) {
		return NSERROR_INVALID;
	}

	/* TTEngine UTF-8 mode: count is Unicode codepoints, not bytes. */
	nchars = utf8_bounded_length(string, length);
	*width = (int)TT_TextLength(&temp_rp, (APTR)string, (ULONG)nchars);
	return NSERROR_OK;
}

static nserror
amiga_tte_nsfont_position_in_string(const plot_font_style_t *fstyle,
				    const char *string, size_t length,
				    int x, size_t *char_offset, int *actual_x)
{
	struct TextExtent extent;
	APTR font;
	ULONG co;
	size_t nchars;

	font = ami_font_tte_open(&temp_rp, fstyle,
				 nsoption_bool(font_antialiasing));
	if (font == NULL) {
		return NSERROR_INVALID;
	}

	nchars = utf8_bounded_length(string, length);
	co = TT_TextFit(&temp_rp, (APTR)string, (ULONG)nchars,
			&extent, NULL, 1, (ULONG)x, 32767);
	*char_offset = utf8_bounded_byte_length(string, length, (size_t)co);
	*actual_x = extent.te_Extent.MaxX;

	return NSERROR_OK;
}

static nserror
amiga_tte_nsfont_split(const plot_font_style_t *fstyle,
		       const char *string, size_t length,
		       int x, size_t *char_offset, int *actual_x)
{
	struct TextExtent extent;
	APTR font;
	ULONG co_cp;
	ULONG offset_cp;
	size_t nchars;
	size_t byte_off;
	size_t split_bytes;

	font = ami_font_tte_open(&temp_rp, fstyle,
				 nsoption_bool(font_antialiasing));
	if (font == NULL) {
		return NSERROR_INVALID;
	}

	nchars = utf8_bounded_length(string, length);
	offset_cp = TT_TextFit(&temp_rp, (APTR)string, (ULONG)nchars,
			       &extent, NULL, 1, (ULONG)x, 32767);

	byte_off = utf8_bounded_byte_length(string, length, (size_t)offset_cp);
	co_cp = offset_cp;

	while (byte_off > 0 && string[byte_off] != ' ') {
		byte_off = utf8_prev(string, byte_off);
		if (co_cp > 0) {
			co_cp--;
		}
	}

	if (byte_off == 0) {
		byte_off = utf8_bounded_byte_length(string, length,
						    (size_t)offset_cp);
		co_cp = offset_cp;
		while (byte_off < length && string[byte_off] != ' ') {
			byte_off = utf8_next(string, length, byte_off);
			co_cp++;
		}
	}

	split_bytes = byte_off;
	if (split_bytes > 0 && split_bytes < length) {
		*actual_x = (int)TT_TextLength(&temp_rp, (APTR)string, co_cp);
		*char_offset = split_bytes;
	} else {
		*actual_x = x;
		*char_offset = length;
	}

	return NSERROR_OK;
}

static ULONG
amiga_tte_nsfont_text(struct RastPort *rp, const char *string, ULONG length,
		      const plot_font_style_t *fstyle, ULONG dx, ULONG dy,
		      bool aa)
{
	APTR font;
	size_t nchars;

	if (string == NULL || string[0] == '\0' || length == 0 || rp == NULL) {
		return 0;
	}

	font = ami_font_tte_open(rp, fstyle, aa);
	if (font == NULL) {
		return 0;
	}

	nchars = utf8_bounded_length(string, (size_t)length);
	Move(rp, dx, dy);
	TT_Text(rp, (APTR)string, (ULONG)nchars);
	return 0;
}

static const struct ami_font_functions ami_font_ttengine_table = {
	amiga_tte_nsfont_width,
	amiga_tte_nsfont_position_in_string,
	amiga_tte_nsfont_split,
	amiga_tte_nsfont_text
};

bool
ami_font_ttengine_available(void)
{
	return (TTEngineBase != NULL);
}

bool
ami_font_ttengine_init(void)
{
	if (TTEngineBase == NULL) {
		NSLOG(netsurf, INFO, "ttengine.library not available");
		return false;
	}

	ami_nsfont = &ami_font_ttengine_table;

	if (temp_rp_inited == false) {
		InitRastPort(&temp_rp);
		temp_rp_inited = true;
	}

	nsoption_setnull_charp(font_sans, (char *)strdup("sans-serif"));
	nsoption_setnull_charp(font_serif, (char *)strdup("serif"));
	nsoption_setnull_charp(font_mono, (char *)strdup("monospaced"));
	nsoption_setnull_charp(font_cursive, (char *)strdup("cursive"));
	nsoption_setnull_charp(font_fantasy, (char *)strdup("fantasy"));

	NSLOG(netsurf, INFO, "Using ttengine.library for page fonts");
	return true;
}

void
ami_font_ttengine_fini(void)
{
	if (prev_font != NULL) {
		TT_CloseFont(prev_font);
		prev_font = NULL;
	}
	prev_fstyle_valid = false;

	if (temp_rp_inited && TTEngineBase != NULL) {
		TT_DoneRastPort(&temp_rp);
	}
}
