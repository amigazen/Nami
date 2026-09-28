/*
 * Copyright 2026 amigazen project
 *
 * This file is part of NetSurf / Tsunami (MUI frontend).
 *
 * Shims so Amiga font engines (and later shared code) can call the few
 * symbols they expect from the Reaction gui without linking gui.c.
 */

#include <proto/intuition.h>
#include <proto/exec.h>

#include <string.h>
#include <stdlib.h>

#include "utils/nsurl.h"
#include "utils/file.h"
#include "utils/errors.h"
#include "netsurf/misc.h"
#include "amiga/schedule.h"
#include "mui/schedule.h"
#include "mui/misc.h"
#include "mui/libs.h"

#ifndef SLEN
#define SLEN(x) (sizeof(x) - 1)
#endif

static int tsunami_done;
static struct gui_file_table file_table;
static int file_table_ready;
static struct Screen *tsunami_screen;

void tsunami_quit(void)
{
	tsunami_done = 1;
}

int tsunami_quitting(void)
{
	return tsunami_done;
}

void tsunami_set_screen(struct Screen *scr)
{
	tsunami_screen = scr;
}

/**
 * Screen pointer for TTEngine antialias / DPI helpers.
 */
struct Screen *ami_gui_get_screen(void)
{
	if (tsunami_screen != NULL) {
		return tsunami_screen;
	}
	if (IntuitionBase != NULL) {
		return IntuitionBase->ActiveScreen;
	}
	return NULL;
}

STRPTR ami_gui_get_screen_title(void)
{
	return (STRPTR)"Tsunami";
}

/**
 * Map Ami schedule API onto Tsunami's gettimeofday scheduler.
 */
nserror ami_schedule(int t, void (*callback)(void *p), void *p)
{
	return tsunami_schedule(t, callback, p);
}

static void gui_quit(void)
{
	tsunami_done = 1;
}

static nserror gui_launch_url(struct nsurl *url)
{
	(void)url;
	return NSERROR_OK;
}

static nserror gui_present_cookies(const char *domain)
{
	(void)domain;
	return NSERROR_OK;
}

static struct gui_misc_table misc_table = {
	.schedule = tsunami_schedule,
	.quit = gui_quit,
	.launch_url = gui_launch_url,
	.present_cookies = gui_present_cookies,
};

struct gui_misc_table *tsunami_misc_table = &misc_table;

/**
 * Amiga Vol:path -> file:///Vol/path (keep assign names like PROGDIR).
 */
static nserror tsunami_path_to_nsurl(const char *path, struct nsurl **url_out)
{
	char *colon;
	char *r;
	char newpath[1024];
	nserror ret;

	if (path == NULL || url_out == NULL) {
		return NSERROR_BAD_PARAMETER;
	}

	if (path[0] == '/') {
		return default_file_table->path_to_nsurl(path, url_out);
	}

	strncpy(newpath, path, sizeof(newpath) - 1);
	newpath[sizeof(newpath) - 1] = '\0';

	r = malloc(strlen(newpath) + SLEN("file:///") + 1);
	if (r == NULL) {
		return NSERROR_NOMEM;
	}

	colon = strchr(newpath, ':');
	if (colon != NULL) {
		*colon = '/';
	}

	strcpy(r, "file:///");
	strcat(r, newpath);

	ret = nsurl_create(r, url_out);
	free(r);
	return ret;
}

/**
 * file:///Vol/path -> Vol:path (inverse of tsunami_path_to_nsurl).
 */
static nserror tsunami_nsurl_to_path(struct nsurl *url, char **path_out)
{
	const char *url_s;
	char *path;
	char *slash;

	if (url == NULL || path_out == NULL) {
		return NSERROR_BAD_PARAMETER;
	}

	url_s = nsurl_access(url);
	if (url_s == NULL || strncmp(url_s, "file:///", 8) != 0) {
		return NSERROR_BAD_PARAMETER;
	}

	path = strdup(url_s + 8);
	if (path == NULL) {
		return NSERROR_NOMEM;
	}

	if (path[0] != '\0' && strchr(path, ':') == NULL) {
		slash = strchr(path, '/');
		if (slash != NULL && slash != path) {
			*slash = ':';
		}
	}

	*path_out = path;
	return NSERROR_OK;
}

struct gui_file_table *tsunami_file_table_get(void)
{
	if (!file_table_ready) {
		file_table = *default_file_table;
		file_table.path_to_nsurl = tsunami_path_to_nsurl;
		file_table.nsurl_to_path = tsunami_nsurl_to_path;
		file_table_ready = 1;
	}
	return &file_table;
}
