/*
 * Copyright 2026 amigazen project
 *
 * This file is part of NetSurf / Tsunami (MUI frontend).
 *
 * Stubs for Amiga FE symbols that shared image/font code expects, without
 * pulling Reaction requesters / guigfx into the MUI frontend.
 */

#include "mui/os3support.h"

#include <stdio.h>
#include <proto/exec.h>
#include <intuition/intuition.h>

#include "utils/errors.h"
#include "utils/log.h"
#include "amiga/misc.h"

/* Optional; bitmap path works when this stays NULL. */
struct Library *GuiGFXBase;

nserror amiga_warn_user(const char *warning, const char *detail)
{
	NSLOG(netsurf, WARNING, "Tsunami: %s %s",
	      warning != NULL ? warning : "",
	      detail != NULL ? detail : "");
	fprintf(stderr, "Tsunami: %s %s\n",
		warning != NULL ? warning : "",
		detail != NULL ? detail : "");
	return NSERROR_OK;
}

int32 amiga_warn_user_multi(const char *body, const char *opt1,
			    const char *opt2, struct Window *win)
{
	(void)body;
	(void)opt1;
	(void)opt2;
	(void)win;
	return 0;
}
