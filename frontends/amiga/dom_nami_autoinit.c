/*
 * dom_nami_autoinit.c - DomBase + OpenLibrary for the Nami binary
 *
 * Nami links dom LVO stubs instead of static dom.lib and loads
 * PROGDIR:libs/dom.library (copied next to the binary at build time).
 *
 * hubbub.library must be available too — dom.library opens it during
 * its own LibInit (same PROGDIR:libs/ search).
 */

#define __USE_SYSBASE

#include <exec/types.h>
#include <proto/exec.h>

#include "dom_stub_open.h"
#include "hubbub_stub_open.h"

#ifndef __NOLIBBASE__
struct Library *DomBase = NULL;
#endif

static struct Library *DomBaseAuto;

static struct Library *open_dom_library(void)
{
	struct Library *base;

	base = OpenLibrary((STRPTR)"PROGDIR:libs/dom.library", 1);
	if (base == NULL) {
		base = OpenLibrary((STRPTR)"LIBS:dom.library", 1);
	}
	if (base == NULL) {
		base = OpenLibrary((STRPTR)"dom.library", 1);
	}
	return base;
}

int dom_stub_ensure_open(void)
{
	if (DomBase != NULL) {
		return 1;
	}
	/* Prefer hubbub resident before dom.library's own OpenLibrary. */
	(void)hubbub_stub_ensure_open();
	DomBaseAuto = open_dom_library();
	DomBase = DomBaseAuto;
	return (DomBase != NULL);
}

/* After hubbub (_INIT_5): dom.library opens hubbub again internally. */
void _INIT_6_DomBase(void)
{
	(void)dom_stub_ensure_open();
}

void _EXIT_6_DomBase(void)
{
	if (DomBaseAuto != NULL) {
		CloseLibrary(DomBaseAuto);
		DomBaseAuto = NULL;
		DomBase = NULL;
	}
}
