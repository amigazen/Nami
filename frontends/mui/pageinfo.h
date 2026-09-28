/*
 * Copyright 2020 Chris Young <chris@unsatisfactorysoftware.co.uk>
 * Copyright 2026 amigazen project
 */

#ifndef MUI_PAGEINFO_H
#define MUI_PAGEINFO_H

#include <exec/types.h>
#include "utils/errors.h"

struct browser_window;

nserror ami_pageinfo_open(struct browser_window *bw, ULONG left, ULONG top);

#endif /* MUI_PAGEINFO_H */
