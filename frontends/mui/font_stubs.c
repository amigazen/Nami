/*
 * Copyright 2026 amigazen project
 *
 * This file is part of NetSurf / Tsunami (MUI frontend).
 *
 * Stub only the unicode font scanner UI (Reaction window/fuelgauge in
 * font_scan.c). Tsunami links the real Amiga bullet + font_cache code;
 * missing glyphs simply fall back without a scan requester.
 */

#include <stdbool.h>
#include <stddef.h>

#include <libwapcaplet/libwapcaplet.h>

#include "amiga/font_scan.h"

/* Quiet unused-parameter noise on vbcc; bodies intentionally empty. */

void ami_font_scan_init(const char *filename, bool force_scan, bool save,
			lwc_string **glypharray)
{
	(void)filename;
	(void)force_scan;
	(void)save;
	(void)glypharray;
}

void ami_font_scan_fini(lwc_string **glypharray)
{
	(void)glypharray;
}

void ami_font_scan_save(const char *filename, lwc_string **glypharray)
{
	(void)filename;
	(void)glypharray;
}

const char *ami_font_scan_lookup(const uint16 *code, lwc_string **glypharray)
{
	(void)code;
	(void)glypharray;
	return NULL;
}
