/*
 * Copyright 2008 - 2016 Chris Young <chris@unsatisfactorysoftware.co.uk>
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

#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "amiga/os3support.h"

#include <proto/diskfont.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/graphics.h>

#include "utils/log.h"
#include "utils/nsoption.h"
#include "netsurf/browser.h"
#include "netsurf/layout.h"

#include "amiga/font.h"
#include "amiga/font_bullet.h"
#include "amiga/font_diskfont.h"
#include "amiga/font_scan.h"
#include "amiga/font_ttengine.h"

static ULONG ami_devicedpi = 72;
static ULONG ami_xdpi = 72;
/** Engine that ami_font_init() actually started (for matching fini). */
static int ami_font_active_engine = AMI_FONTENG_BULLET;

struct ami_font_functions *ami_nsfont = NULL;

ULONG ami_font_dpi_get_devicedpi(void)
{
	return (ami_xdpi << 16) | ami_devicedpi;
}

ULONG ami_font_dpi_get_xdpi(void)
{
	return ami_xdpi;
}

void ami_font_setdevicedpi(int id)
{
	DisplayInfoHandle dih;
	struct DisplayInfo dinfo;

	ULONG ydpi = nsoption_int(screen_ydpi);
	ULONG xdpi = nsoption_int(screen_ydpi);
	browser_set_dpi(nsoption_int(screen_ydpi));

	if(id && (nsoption_int(monitor_aspect_x) != 0) && (nsoption_int(monitor_aspect_y) != 0))
	{
		if((dih = FindDisplayInfo(id)))
		{
			if(GetDisplayInfoData(dih, &dinfo, sizeof(struct DisplayInfo),
				DTAG_DISP, 0))
			{
				int xres = dinfo.Resolution.x;
				int yres = dinfo.Resolution.y;

				if((nsoption_int(monitor_aspect_x) != 4) || (nsoption_int(monitor_aspect_y) != 3))
				{
					/* AmigaOS sees 4:3 modes as square in the DisplayInfo database,
					 * so we correct other modes to "4:3 equiv" here. */
					xres = (xres * nsoption_int(monitor_aspect_x)) / 4;
					yres = (yres * nsoption_int(monitor_aspect_y)) / 3;
				}

				xdpi = (yres * ydpi) / xres;

				NSLOG(netsurf, INFO,
				      "XDPI = %ld, YDPI = %ld (DisplayInfo resolution %d x %d, corrected %d x %d)",
				      xdpi,
				      ydpi,
				      dinfo.Resolution.x,
				      dinfo.Resolution.y,
				      xres,
				      yres);
			}
		}
	}

	ami_xdpi = xdpi;
	ami_devicedpi = (xdpi << 16) | ydpi;
}

/* The below are simple font routines which should not be used for page rendering */

struct TextFont *ami_font_open_disk_font(struct TextAttr *tattr)
{
	struct TextFont *tfont = OpenDiskFont(tattr);
	return tfont;
}

void ami_font_close_disk_font(struct TextFont *tfont)
{
	CloseFont(tfont);
}

/**
 * Resolve AUTO / explicit engine choice to a concrete backend.
 */
static int
ami_font_resolve_engine(void)
{
	int eng;

	eng = nsoption_int(font_engine);

	if (eng == AMI_FONTENG_TTENGINE) {
		if (ami_font_ttengine_available()) {
			return AMI_FONTENG_TTENGINE;
		}
		NSLOG(netsurf, INFO,
		      "ttengine requested but unavailable; falling back");
		eng = AMI_FONTENG_AUTO;
	}

	if (eng == AMI_FONTENG_BULLET || eng == AMI_FONTENG_DISKFONT) {
		return eng;
	}

	/* AUTO: prefer ttengine when present, else legacy bitmap_fonts flag. */
	if (ami_font_ttengine_available()) {
		return AMI_FONTENG_TTENGINE;
	}

	if (nsoption_bool(bitmap_fonts)) {
		return AMI_FONTENG_DISKFONT;
	}

	return AMI_FONTENG_BULLET;
}

/**
 * First FONTS: name.otag that exists, else first candidate (for nsoption).
 */
static const char *
ami_font_first_otag(const char **names)
{
	BPTR lock;
	char path[108];
	int i;

	if (names == NULL || names[0] == NULL) {
		return NULL;
	}

	for (i = 0; names[i] != NULL; i++) {
		path[0] = '\0';
		strncat(path, "FONTS:", sizeof(path) - 1);
		strncat(path, names[i], sizeof(path) - 1 - strlen(path));
		strncat(path, ".otag", sizeof(path) - 1 - strlen(path));
		lock = Lock(path, ACCESS_READ);
		if (lock != 0) {
			UnLock(lock);
			NSLOG(netsurf, INFO, "CG font pick: %s", names[i]);
			return names[i];
		}
	}
	return names[0];
}

static void
ami_font_init_bullet_defaults(void)
{
#ifdef __amigaos4__
	nsoption_setnull_charp(font_sans, (char *)strdup("DejaVu Sans"));
	nsoption_setnull_charp(font_serif, (char *)strdup("DejaVu Serif"));
	nsoption_setnull_charp(font_mono, (char *)strdup("DejaVu Sans Mono"));
	nsoption_setnull_charp(font_cursive, (char *)strdup("DejaVu Sans"));
	nsoption_setnull_charp(font_fantasy, (char *)strdup("DejaVu Serif"));
#else
	/* Stock OS3 Compugraphic / Intellifont faces, with common aliases. */
	static const char *sans_cands[] = {
		"CGTriumvirate", "Triumvirate", NULL
	};
	static const char *serif_cands[] = {
		"CGTimes", "Times", NULL
	};
	static const char *mono_cands[] = {
		"Courier", "LetterGothic", NULL
	};
	const char *sans;
	const char *serif;
	const char *mono;

	sans = ami_font_first_otag(sans_cands);
	serif = ami_font_first_otag(serif_cands);
	mono = ami_font_first_otag(mono_cands);

	/*
	 * Always install CG faces for the bullet engine. Prefs often still
	 * hold diskfont names (helvetica/times/topaz) from AUTO/bitmap mode;
	 * those cannot OpenOutlineFont and produced blank pages.
	 */
	nsoption_set_charp(font_sans, (char *)strdup(sans));
	nsoption_set_charp(font_serif, (char *)strdup(serif));
	nsoption_set_charp(font_mono, (char *)strdup(mono));
	nsoption_set_charp(font_cursive, (char *)strdup(sans));
	nsoption_set_charp(font_fantasy, (char *)strdup(serif));
	NSLOG(netsurf, INFO,
	      "bullet fonts: sans=%s serif=%s mono=%s",
	      sans, serif, mono);
#endif
}

static void
ami_font_init_diskfont_defaults(void)
{
	nsoption_setnull_charp(font_sans, (char *)strdup("helvetica"));
	nsoption_setnull_charp(font_serif, (char *)strdup("times"));
	nsoption_setnull_charp(font_mono, (char *)strdup("topaz"));
	nsoption_setnull_charp(font_cursive, (char *)strdup("garnet"));
	nsoption_setnull_charp(font_fantasy, (char *)strdup("emerald"));
}

/* Font initialisation */
void ami_font_init(void)
{
	int eng;

	eng = ami_font_resolve_engine();

	if (eng == AMI_FONTENG_TTENGINE) {
		if (ami_font_ttengine_init()) {
			ami_font_active_engine = AMI_FONTENG_TTENGINE;
			NSLOG(netsurf, INFO, "ami_font_init: engine=ttengine");
			return;
		}
		NSLOG(netsurf, WARNING,
		      "ami_font_init: ttengine init failed, falling back");
		eng = nsoption_bool(bitmap_fonts) ?
			AMI_FONTENG_DISKFONT : AMI_FONTENG_BULLET;
	}

	if (eng == AMI_FONTENG_DISKFONT) {
		ami_font_init_diskfont_defaults();
		ami_font_diskfont_init();
		ami_font_active_engine = AMI_FONTENG_DISKFONT;
		NSLOG(netsurf, INFO, "ami_font_init: engine=diskfont");
		return;
	}

	ami_font_init_bullet_defaults();
	ami_font_bullet_init();
	ami_font_active_engine = AMI_FONTENG_BULLET;
	NSLOG(netsurf, INFO, "ami_font_init: engine=bullet");
}

void ami_font_fini(void)
{
	switch (ami_font_active_engine) {
	case AMI_FONTENG_TTENGINE:
		ami_font_ttengine_fini();
		break;
	case AMI_FONTENG_DISKFONT:
		ami_font_diskfont_fini();
		break;
	case AMI_FONTENG_BULLET:
	default:
		ami_font_bullet_fini();
		break;
	}

	ami_nsfont = NULL;
}

/* Stub entry points */
static nserror ami_font_width(const plot_font_style_t *fstyle,
		const char *string, size_t length,
		int *width)
{
	if(__builtin_expect(ami_nsfont == NULL, 0)) return false;
	return ami_nsfont->width(fstyle, string, length, width);
}

static nserror ami_font_position(const plot_font_style_t *fstyle,
		const char *string, size_t length,
		int x, size_t *char_offset, int *actual_x)
{
	if(__builtin_expect(ami_nsfont == NULL, 0)) return false;
	return ami_nsfont->posn(fstyle, string, length, x, char_offset, actual_x);
}

static nserror ami_font_split(const plot_font_style_t *fstyle,
		const char *string, size_t length,
		int x, size_t *char_offset, int *actual_x)
{
	if(__builtin_expect(ami_nsfont == NULL, 0)) return false;
	return ami_nsfont->split(fstyle, string, length, x, char_offset, actual_x);
}

static struct gui_layout_table layout_table = {
	.width = ami_font_width,
	.position = ami_font_position,
	.split = ami_font_split,
};

struct gui_layout_table *ami_layout_table = &layout_table;
