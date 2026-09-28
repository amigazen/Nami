/*
 * Copyright 2026 amigazen project
 *
 * This file is part of NetSurf / Tsunami (MUI frontend).
 *
 * Amiga $VER string for Workbench Version.
 */

#include "mui/os3support.h"
#include "testament.h"

#define TSUNAMI_VERSION_MAJOR "3"
#define TSUNAMI_VERSION_MINOR "12"

/* Array (not pointer) so vbcc accepts a constant initializer. */
const char tsunami_verstag[] =
	"\0$VER: Tsunami " TSUNAMI_VERSION_MAJOR "."
	TSUNAMI_VERSION_MINOR " (" WT_COMPILEDATE ") MUI\0";

const char * const verdate = WT_COMPILEDATE;
const char * const wt_revid = WT_REVID;
