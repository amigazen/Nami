/*
 * hubbub_nami_autoinit.c - HubbubBase + OpenLibrary for the Nami binary
 *
 * Nami links hubbub LVO stubs instead of static hubbub.lib and loads
 * PROGDIR:libs/hubbub.library (copied next to the binary at build time).
 *
 * Also required by dom.library (opened again inside that library's init).
 */

#define __USE_SYSBASE

#include <exec/types.h>
#include <proto/exec.h>

#include "hubbub_stub_open.h"

#ifndef __NOLIBBASE__
struct Library *HubbubBase = NULL;
#endif

static struct Library *HubbubBaseAuto;

static struct Library *open_hubbub_library(void)
{
	struct Library *base;

	base = OpenLibrary((STRPTR)"PROGDIR:libs/hubbub.library", 1);
	if (base == NULL) {
		base = OpenLibrary((STRPTR)"LIBS:hubbub.library", 1);
	}
	if (base == NULL) {
		base = OpenLibrary((STRPTR)"hubbub.library", 1);
	}
	return base;
}

int hubbub_stub_ensure_open(void)
{
	if (HubbubBase != NULL) {
		return 1;
	}
	HubbubBaseAuto = open_hubbub_library();
	HubbubBase = HubbubBaseAuto;
	return (HubbubBase != NULL);
}

void _INIT_5_HubbubBase(void)
{
	(void)hubbub_stub_ensure_open();
}

void _EXIT_5_HubbubBase(void)
{
	if (HubbubBaseAuto != NULL) {
		CloseLibrary(HubbubBaseAuto);
		HubbubBaseAuto = NULL;
		HubbubBase = NULL;
	}
}
