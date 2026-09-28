/*
 * Copyright 2026 amigazen project
 *
 * Tsunami iconv entry points: prefer iconv.library when present, otherwise
 * the AmigaOS3 lightweight converter (compiled as tsunami_fb_iconv_*).
 * Amiga Reaction FE keeps linking frontends/amiga/iconv/iconv.c unchanged.
 */

#include "mui/os3support.h"

#include <proto/exec.h>
#include <exec/libraries.h>
#include <exec/types.h>

#include <stddef.h>
#include <errno.h>

/* POSIX-shaped API NetSurf core links against (frontends/amiga/iconv/iconv.h). */
#include "iconv.h"

#include "utils/log.h"

/*
 * VBCC LVO inlines for iconv.library. Do not include libraries/iconv.h —
 * it renames iconv_open → libiconv_open and would collide with our exports.
 */
struct Library *IConvBase;
#include <inline/iconv_protos.h>

/* Fallback symbols from frontends/amiga/iconv/iconv.c (renamed at compile). */
extern iconv_t tsunami_fb_iconv_open(const char *tocode, const char *fromcode);
extern size_t tsunami_fb_iconv(iconv_t cd, const char **inbuf,
			       size_t *inbytesleft, char **outbuf,
			       size_t *outbytesleft);
extern int tsunami_fb_iconv_close(iconv_t cd);

void tsunami_iconv_lib_open(void)
{
	if (IConvBase != NULL) {
		return;
	}
	/* Version 3 matches iconv.library SDK / example. */
	IConvBase = OpenLibrary("iconv.library", 3);
	if (IConvBase != NULL) {
		NSLOG(netsurf, INFO, "iconv.library v%d.%d",
		      IConvBase->lib_Version, IConvBase->lib_Revision);
	} else {
		NSLOG(netsurf, INFO,
		      "iconv.library missing — using built-in converter");
	}
}

void tsunami_iconv_lib_close(void)
{
	if (IConvBase != NULL) {
		CloseLibrary(IConvBase);
		IConvBase = NULL;
	}
}

iconv_t iconv_open(const char *tocode, const char *fromcode)
{
	if (IConvBase != NULL) {
		return libiconv_open(tocode, fromcode);
	}
	return tsunami_fb_iconv_open(tocode, fromcode);
}

size_t iconv(iconv_t cd, const char **inbuf, size_t *inbytesleft,
	     char **outbuf, size_t *outbytesleft)
{
	if (IConvBase != NULL) {
		return libiconv(cd, inbuf, inbytesleft, outbuf, outbytesleft);
	}
	return tsunami_fb_iconv(cd, inbuf, inbytesleft, outbuf, outbytesleft);
}

int iconv_close(iconv_t cd)
{
	if (IConvBase != NULL) {
		return libiconv_close(cd);
	}
	return tsunami_fb_iconv_close(cd);
}
