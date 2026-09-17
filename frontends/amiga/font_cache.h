/*
 * Copyright 2015 Chris Young <chris@unsatisfactorysoftware.co.uk>
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

#ifndef AMIGA_FONT_CACHE_H
#define AMIGA_FONT_CACHE_H

#include <proto/timer.h>
#include <exec/types.h>

struct OutlineFont;

/* Latin-1 design-unit widths (bullet FIXED) — avoids OT_GlyphMap on measure */
#define AMI_FONT_W_LATIN	256
/* Sparse overflow for codepoints >= 256 */
#define AMI_FONT_W_EXTRA	96

struct ami_font_cache_node
{
#ifdef __amigaos4__
	struct SkipNode skip_node;
#endif
	struct OutlineFont *font;
	char *restrict bold;
	char *restrict italic;
	char *restrict bolditalic;
	struct TimeVal lastused;
#ifndef __amigaos4__
	/* Bullet width cache: FontGlyphCache on disk only maps code→font name */
	UBYTE w_latin_have[AMI_FONT_W_LATIN];
	LONG w_latin[AMI_FONT_W_LATIN];
	UWORD w_extra_code[AMI_FONT_W_EXTRA];
	LONG w_extra[AMI_FONT_W_EXTRA];
	UBYTE w_extra_have[AMI_FONT_W_EXTRA];
#endif
};


/* locate an entry in the font cache, NULL if not found */
struct ami_font_cache_node *ami_font_cache_locate(const char *font);

/* allocate a cache entry */
struct ami_font_cache_node *ami_font_cache_alloc_entry(const char *font);

/* insert a cache entry into the list (OS3) */
void ami_font_cache_insert(struct ami_font_cache_node *nodedata, const char *font);

#ifndef __amigaos4__
/* Find open-font cache node by OutlineFont pointer (OS3 bullet path). */
struct ami_font_cache_node *ami_font_cache_find_ofont(struct OutlineFont *ofont);
#endif

/* initialise the cache */
void ami_font_cache_init(void);

/* cache clean-up */
void ami_font_cache_fini(void);

#endif
