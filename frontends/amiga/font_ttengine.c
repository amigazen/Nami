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
 * Follows ttengine.doc patterns: TT_FamilyTable (CSS faces → websafe →
 * generic → default), TT_SetAttrs + TT_SetFont, JAM1 + Move + TT_Text,
 * UTF-8 encoding, TT_DoneRastPort before disposing RastPorts.
 */

#include "amiga/os3support.h"

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/ttengine.h>
#include <libraries/ttengine.h>
#include <intuition/screens.h>
#include <graphics/rpattr.h>

#include <libwapcaplet/libwapcaplet.h>

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
static struct BitMap *temp_bm = NULL;
static bool temp_rp_inited = false;
/*
 * FamilyTable slots: CSS faces, option/websafe, alts, generic, default.
 * ttengine.doc: {"Arial", "sans-serif", "default", NULL}
 */
static STRPTR family_table[12];
static bool tte_active = false;
/* Last CSS weight / flags used with TT_SetFont (bold/italic ink pad). */
static int tte_last_weight = 400;
static plot_font_flags_t tte_last_font_flags = FONTF_NONE;

/**
 * Faux-bold / italic often paints a few pixels past the rounded advance
 */
static ULONG
ami_font_tte_ink_pad(const plot_font_style_t *fstyle)
{
	ULONG n;
	plot_font_flags_t flags;
	int weight;

	n = 0;
	flags = tte_last_font_flags;
	weight = tte_last_weight;
	if (fstyle != NULL) {
		flags = fstyle->flags;
		weight = fstyle->weight;
	}
	if (weight >= 700) {
		n += 2;
	}
	if ((flags & FONTF_ITALIC) || (flags & FONTF_OBLIQUE)) {
		n += 1;
	}
	return n;
}

/**
 * Pixel width of a UTF-8 string already counted in Unicode codepoints.
 * Prefer max(advance, ink span) + bold/italic pad.
 */
static int
ami_font_tte_measure_width(struct RastPort *rp, const char *string,
		size_t nchars, const plot_font_style_t *fstyle)
{
	struct TextExtent te;
	ULONG adv;
	LONG ink;
	ULONG outw;
	WORD wc;

	if (nchars == 0) {
		return 0;
	}
	if (nchars > 32767) {
		nchars = 32767;
	}
	wc = (WORD)nchars;
	memset(&te, 0, sizeof(te));
	TT_TextExtent(rp, (APTR)string, wc, &te);
	adv = (ULONG)te.te_Width;
	ink = (LONG)te.te_Extent.MaxX - (LONG)te.te_Extent.MinX + 1L;
	if (ink < 1) {
		ink = (LONG)adv;
	}
	outw = adv;
	if ((ULONG)ink > outw) {
		outw = (ULONG)ink;
	}
	outw += ami_font_tte_ink_pad(fstyle);
	return (int)outw;
}

/**
 * TT_TextFit width constraint with bold/italic slack reserved.
 */
static ULONG
ami_font_tte_fit_width(ULONG cwidth, const plot_font_style_t *fstyle)
{
	ULONG pad;

	if (cwidth == 0) {
		return 0;
	}
	pad = ami_font_tte_ink_pad(fstyle);
	if (pad >= cwidth) {
		return 1;
	}
	return cwidth - pad;
}

/**
 * Stock ttengine TT_TextFit (NULL constr_extent) compares horizontal ink to
 * the height argument and vertical ink to width — swap args.
 */
static ULONG
ami_font_tte_text_fit(struct RastPort *rp, const char *string, size_t nchars,
		struct TextExtent *te, ULONG cwidth,
		const plot_font_style_t *fstyle)
{
	ULONG fitw;
	ULONG ucnt;

	if (nchars == 0) {
		if (te != NULL) {
			memset(te, 0, sizeof(*te));
		}
		return 0;
	}
	ucnt = (ULONG)nchars;
	if (ucnt > 65535UL) {
		ucnt = 65535UL;
	}
	fitw = ami_font_tte_fit_width(cwidth, fstyle);
	/* Swap: pass fit width as height, large height as width */
	return TT_TextFit(rp, (APTR)string, (ULONG)ucnt, te, NULL, 1,
			32767, (ULONG)fitw);
}

/**
 * Websafe TrueType defaults for each CSS generic family.
 */
static const char *
ami_font_tte_websafe(plot_font_generic_family_t family)
{
	switch (family) {
	case PLOT_FONT_FAMILY_SERIF:
		return "Times New Roman";
	case PLOT_FONT_FAMILY_MONOSPACE:
		return "Courier New";
	case PLOT_FONT_FAMILY_CURSIVE:
		return "Comic Sans MS";
	case PLOT_FONT_FAMILY_FANTASY:
		return "Impact";
	case PLOT_FONT_FAMILY_SANS_SERIF:
	default:
		return "Arial";
	}
}

/**
 * Extra websafe alternates (before CSS generic / default).
 */
static void
ami_font_tte_websafe_alts(plot_font_generic_family_t family,
		const char **alt0, const char **alt1)
{
	*alt0 = NULL;
	*alt1 = NULL;
	switch (family) {
	case PLOT_FONT_FAMILY_SERIF:
		*alt0 = "Georgia";
		*alt1 = "Times";
		break;
	case PLOT_FONT_FAMILY_MONOSPACE:
		*alt0 = "Andale Mono";
		*alt1 = "Courier";
		break;
	case PLOT_FONT_FAMILY_CURSIVE:
		*alt0 = "Comic Sans MS";
		break;
	case PLOT_FONT_FAMILY_FANTASY:
		*alt0 = "Impact";
		*alt1 = "Arial Black";
		break;
	case PLOT_FONT_FAMILY_SANS_SERIF:
	default:
		*alt0 = "Helvetica";
		*alt1 = "Verdana";
		break;
	}
}

/**
 * CSS generic name recognized by TT_FamilyTable (ttengine.doc).
 */
static const char *
ami_font_tte_generic(plot_font_generic_family_t family)
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
 * Preferred face from NetSurf options for a CSS generic family.
 */
static char *
ami_font_tte_option_name(plot_font_generic_family_t family)
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
 * True if name looks like Compugraphic/bullet/diskfont, not a TTF family.
 */
static bool
ami_font_tte_is_bullet_name(const char *name)
{
	if (name == NULL || name[0] == '\0') {
		return true;
	}
	if (strncmp(name, "CG", 2) == 0) {
		return true;
	}
	if (strcmp(name, "Triumvirate") == 0 ||
	    strcmp(name, "LetterGothic") == 0 ||
	    strcmp(name, "helvetica") == 0 ||
	    strcmp(name, "topaz") == 0 ||
	    strcmp(name, "garnet") == 0 ||
	    strcmp(name, "emerald") == 0) {
		return true;
	}
	return false;
}

/**
 * Append name to family_table if not already present.
 */
static void
ami_font_tte_table_add(int *i, const char *name)
{
	int j;

	if (name == NULL || name[0] == '\0' || *i >= 11) {
		return;
	}
	for (j = 0; j < *i; j++) {
		if (family_table[j] != NULL &&
		    strcasecmp((char *)family_table[j], (char *)name) == 0) {
			return;
		}
	}
	family_table[(*i)++] = (STRPTR)name;
}

/**
 * Pixel size for TT_FontSize (baseline-to-baseline, ttengine.doc).
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
 * CSS weight 100–900 for TT_FontWeight (ttengine.doc).
 */
static ULONG
ami_font_tte_weight(const plot_font_style_t *fstyle)
{
	int w;

	w = fstyle->weight;
	if (w < 100) {
		w = TT_FontWeight_Normal;
	}
	if (w > 999) {
		w = 999;
	}
	return (ULONG)w;
}

/**
 * Build FamilyTable per ttengine.doc HTML FACE pattern.
 *
 * Order: page CSS faces → option/websafe → alts → CSS generic → default.
 */
static void
ami_font_tte_fill_family_table(const plot_font_style_t *fstyle)
{
	char *optname;
	const char *websafe;
	const char *alt0;
	const char *alt1;
	const char *generic;
	int i;
	int n;

	i = 0;

	/* Page font-family list — exactly what TT_FamilyTable is for. */
	if (fstyle->families != NULL) {
		for (n = 0; fstyle->families[n] != NULL && i < 4; n++) {
			ami_font_tte_table_add(&i,
					lwc_string_data(fstyle->families[n]));
		}
	}

	optname = ami_font_tte_option_name(fstyle->family);
	websafe = ami_font_tte_websafe(fstyle->family);
	generic = ami_font_tte_generic(fstyle->family);
	ami_font_tte_websafe_alts(fstyle->family, &alt0, &alt1);

	if (optname != NULL && ami_font_tte_is_bullet_name(optname) == false) {
		ami_font_tte_table_add(&i, optname);
	}
	ami_font_tte_table_add(&i, websafe);
	ami_font_tte_table_add(&i, alt0);
	ami_font_tte_table_add(&i, alt1);
	ami_font_tte_table_add(&i, generic);
	ami_font_tte_table_add(&i, "default");
	family_table[i] = NULL;
}

/**
 * Open via FamilyTable (already ends with "default" — no second open).
 */
static APTR
ami_font_tte_open_font(ULONG size, ULONG style, ULONG weight)
{
	APTR font;

	font = TT_OpenFont(
		TT_FamilyTable, (ULONG)family_table,
		TT_FontSize, size,
		TT_FontStyle, style,
		TT_FontWeight, weight,
		TAG_DONE);
	if (font == NULL) {
		NSLOG(netsurf, WARNING,
		      "TT_OpenFont failed (%s size %lu style %lu weight %lu)",
		      family_table[0] != NULL ? (char *)family_table[0] : "?",
		      (unsigned long)size,
		      (unsigned long)style,
		      (unsigned long)weight);
	}
	return font;
}

/**
 * Antialias: Off on LUT (doc: needs 15-bit+ RTG); Auto on deep screens.
 */
static ULONG
ami_font_tte_aa_mode(bool want_aa)
{
	struct Screen *scrn;
	ULONG depth;

	if (want_aa == false) {
		return TT_Antialias_Off;
	}

	scrn = ami_gui_get_screen();
	if (scrn == NULL || scrn->RastPort.BitMap == NULL) {
		return TT_Antialias_Off;
	}
	depth = GetBitMapAttr(scrn->RastPort.BitMap, BMA_DEPTH);
	if (depth <= 8) {
		return TT_Antialias_Off;
	}
	/* Respect database SMOOTHSMALL/SMOOTHBIG thresholds. */
	return TT_Antialias_Auto;
}

/**
 * Apply per-RastPort attrs (ttengine.doc SetFont example order).
 *
 * DiskFontMetrics stays FALSE — that mode is for JAM2 text editors and
 * clips glyphs outside accented ascender/real descender.
 */
static void
ami_font_tte_set_attrs(struct RastPort *rp, bool aa)
{
	struct Screen *scrn;

	scrn = ami_gui_get_screen();
	TT_SetAttrs(rp,
		TT_Encoding, TT_Encoding_UTF8,
		TT_Antialias, ami_font_tte_aa_mode(aa),
		TT_SoftStyle, TT_SoftStyle_None,
		TT_Foreground, TT_Foreground_UseRastPort,
		TT_Background, TT_Background_UseRastPort,
		TT_DiskFontMetrics, FALSE,
		TT_Screen, (ULONG)scrn,
		TAG_DONE);
}

/**
 * Open (or reuse) a font and attach it to rp for metrics or paint.
 */
static APTR
ami_font_tte_open(struct RastPort *rp, const plot_font_style_t *fstyle, bool aa)
{
	APTR font;
	ULONG size;
	ULONG weight;
	ULONG style;

	if (rp == NULL || fstyle == NULL || TTEngineBase == NULL) {
		return NULL;
	}

	if (prev_font != NULL && prev_fstyle_valid &&
	    fstyle->family == prev_fstyle.family &&
	    fstyle->size == prev_fstyle.size &&
	    fstyle->weight == prev_fstyle.weight &&
	    fstyle->flags == prev_fstyle.flags &&
	    fstyle->families == prev_fstyle.families) {
		ami_font_tte_set_attrs(rp, aa);
		if (TT_SetFont(rp, prev_font) == FALSE) {
			return NULL;
		}
		tte_last_weight = fstyle->weight;
		tte_last_font_flags = fstyle->flags;
		return prev_font;
	}

	if (prev_font != NULL) {
		TT_CloseFont(prev_font);
		prev_font = NULL;
		prev_fstyle_valid = false;
	}

	ami_font_tte_fill_family_table(fstyle);
	size = ami_font_tte_pixel_size(fstyle);
	weight = ami_font_tte_weight(fstyle);

	style = TT_FontStyle_Regular;
	if ((fstyle->flags & FONTF_ITALIC) || (fstyle->flags & FONTF_OBLIQUE)) {
		style = TT_FontStyle_Italic;
	}

	font = ami_font_tte_open_font(size, style, weight);
	/* No italic/oblique face — fall back to regular at same weight. */
	if (font == NULL && style != TT_FontStyle_Regular) {
		font = ami_font_tte_open_font(size, TT_FontStyle_Regular, weight);
	}
	/* Odd weight missing — try normal. */
	if (font == NULL && weight != (ULONG)TT_FontWeight_Normal) {
		font = ami_font_tte_open_font(size, style,
				(ULONG)TT_FontWeight_Normal);
	}
	if (font == NULL && style != TT_FontStyle_Regular &&
	    weight != (ULONG)TT_FontWeight_Normal) {
		font = ami_font_tte_open_font(size, TT_FontStyle_Regular,
				(ULONG)TT_FontWeight_Normal);
	}
	if (font == NULL) {
		return NULL;
	}

	/* Doc example: SetAttrs then SetFont then Text. */
	ami_font_tte_set_attrs(rp, aa);
	if (TT_SetFont(rp, font) == FALSE) {
		TT_CloseFont(font);
		return NULL;
	}

	prev_font = font;
	prev_fstyle = *fstyle;
	prev_fstyle_valid = true;
	tte_last_weight = fstyle->weight;
	tte_last_font_flags = fstyle->flags;
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

	font = ami_font_tte_open(&temp_rp, fstyle, false);
	if (font == NULL) {
		return NSERROR_INVALID;
	}

	/* UTF-8 byte span → Unicode count for TT_Encoding_UTF8 */
	nchars = utf8_bounded_length(string, length);
	*width = ami_font_tte_measure_width(&temp_rp, string, nchars, fstyle);
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

	font = ami_font_tte_open(&temp_rp, fstyle, false);
	if (font == NULL) {
		return NSERROR_INVALID;
	}

	nchars = utf8_bounded_length(string, length);
	co = ami_font_tte_text_fit(&temp_rp, string, nchars, &extent,
			(ULONG)x, fstyle);
	*char_offset = utf8_bounded_byte_length(string, length, (size_t)co);
	*actual_x = ami_font_tte_measure_width(&temp_rp, string,
			(size_t)co, fstyle);

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

	font = ami_font_tte_open(&temp_rp, fstyle, false);
	if (font == NULL) {
		return NSERROR_INVALID;
	}

	nchars = utf8_bounded_length(string, length);
	offset_cp = ami_font_tte_text_fit(&temp_rp, string, nchars, &extent,
			(ULONG)x, fstyle);

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
		*actual_x = ami_font_tte_measure_width(&temp_rp, string,
				(size_t)co_cp, fstyle);
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
	/* Doc example: JAM1 + Move + TT_Text (transparent background). */
	SetDrMd(rp, JAM1);
	Move(rp, (LONG)dx, (LONG)dy);
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
ami_font_ttengine_active(void)
{
	return tte_active;
}

void
ami_font_ttengine_done_rastport(struct RastPort *rp)
{
	if (tte_active && rp != NULL && TTEngineBase != NULL) {
		TT_DoneRastPort(rp);
	}
}

bool
ami_font_ttengine_init(void)
{
	if (TTEngineBase == NULL) {
		NSLOG(netsurf, INFO, "ttengine.library not available");
		return false;
	}

	ami_nsfont = (struct ami_font_functions *)&ami_font_ttengine_table;
	tte_active = true;

	if (temp_rp_inited == false) {
		InitRastPort(&temp_rp);
		/* Metrics need a BitMap; empty RastPort fails TT_SetFont. */
		temp_bm = AllocBitMap(32, 32, 1, BMF_CLEAR, NULL);
		if (temp_bm != NULL) {
			temp_rp.BitMap = temp_bm;
		}
		SetDrMd(&temp_rp, JAM1);
		temp_rp_inited = true;
	}

	/*
	 * Replace Compugraphic/diskfont leftovers with websafe TTF names.
	 * FamilyTable still falls through to CSS generic and "default".
	 */
	if (ami_font_tte_is_bullet_name(nsoption_charp(font_sans))) {
		nsoption_set_charp(font_sans,
				(char *)strdup(ami_font_tte_websafe(
						PLOT_FONT_FAMILY_SANS_SERIF)));
	}
	if (ami_font_tte_is_bullet_name(nsoption_charp(font_serif))) {
		nsoption_set_charp(font_serif,
				(char *)strdup(ami_font_tte_websafe(
						PLOT_FONT_FAMILY_SERIF)));
	}
	if (ami_font_tte_is_bullet_name(nsoption_charp(font_mono))) {
		nsoption_set_charp(font_mono,
				(char *)strdup(ami_font_tte_websafe(
						PLOT_FONT_FAMILY_MONOSPACE)));
	}
	if (ami_font_tte_is_bullet_name(nsoption_charp(font_cursive))) {
		nsoption_set_charp(font_cursive,
				(char *)strdup(ami_font_tte_websafe(
						PLOT_FONT_FAMILY_CURSIVE)));
	}
	if (ami_font_tte_is_bullet_name(nsoption_charp(font_fantasy))) {
		nsoption_set_charp(font_fantasy,
				(char *)strdup(ami_font_tte_websafe(
						PLOT_FONT_FAMILY_FANTASY)));
	}

	NSLOG(netsurf, INFO,
	      "Using ttengine.library v%lu.%lu — websafe defaults "
	      "sans=%s serif=%s mono=%s",
	      (unsigned long)TTEngineBase->lib_Version,
	      (unsigned long)TTEngineBase->lib_Revision,
	      nsoption_charp(font_sans) ? nsoption_charp(font_sans) : "?",
	      nsoption_charp(font_serif) ? nsoption_charp(font_serif) : "?",
	      nsoption_charp(font_mono) ? nsoption_charp(font_mono) : "?");
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
	if (temp_bm != NULL) {
		FreeBitMap(temp_bm);
		temp_bm = NULL;
	}
	temp_rp_inited = false;
	tte_active = false;
}
