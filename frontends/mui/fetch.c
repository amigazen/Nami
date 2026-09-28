/*
 * Copyright 2026 amigazen project
 *
 * This file is part of NetSurf / Tsunami (MUI frontend).
 */

#include <stdlib.h>
#include <limits.h>

#include "utils/errors.h"
#include "utils/file.h"
#include "utils/nsurl.h"
#include "utils/filepath.h"
#include "netsurf/fetch.h"

#include "mui/filetype.h"
#include "mui/fetch.h"

char **tsunami_respaths;

static nsurl *gui_get_resource_url(const char *path)
{
	char buf[PATH_MAX];
	nsurl *url;

	url = NULL;
	netsurf_path_to_nsurl(filepath_sfind(tsunami_respaths, buf, path), &url);
	return url;
}

static struct gui_fetch_table fetch_table = {
	.filetype = tsunami_fetch_filetype,
	.get_resource_url = gui_get_resource_url,
};

struct gui_fetch_table *tsunami_fetch_table = &fetch_table;
