/*
 * Copyright 2017 Chris Young <chris@unsatisfactorysoftware.co.uk>
 * Copyright 2026 amigazen project
 */

#ifndef MUI_HOTLIST_H
#define MUI_HOTLIST_H

#include <stdbool.h>

#include "utils/nsurl.h"
#include "utils/errors.h"

nserror ami_hotlist_present(void);
void ami_hotlist_close(void);
nserror ami_hotlist_scan(void *userdata, int first_item, const char *folder,
	bool (*cb_add_item)(void *userdata, int level, int item,
			    const char *title, nsurl *url, bool folder));

#endif /* MUI_HOTLIST_H */
