/*
 * Copyright 2026 amigazen project
 *
 * This file is part of NetSurf / Tsunami (MUI frontend).
 *
 * Extension-to-MIME mapping seeded with browser essentials and optional
 * mime.types file (same approach as the monkey frontend).
 */

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "utils/log.h"
#include "utils/ascii.h"
#include "utils/hashtable.h"
#include "mui/filetype.h"

#define HASH_SIZE 117
#define MAX_LINE_LEN 256

static struct hash_table *mime_hash = NULL;

void tsunami_fetch_filetype_init(const char *mimefile)
{
	struct stat statbuf;
	FILE *fh;

	mime_hash = hash_create(HASH_SIZE);

	hash_add(mime_hash, "css", "text/css");
	hash_add(mime_hash, "htm", "text/html");
	hash_add(mime_hash, "html", "text/html");
	hash_add(mime_hash, "jpg", "image/jpeg");
	hash_add(mime_hash, "jpeg", "image/jpeg");
	hash_add(mime_hash, "bmp", "image/bmp");
	hash_add(mime_hash, "gif", "image/gif");
	hash_add(mime_hash, "png", "image/png");
	hash_add(mime_hash, "ico", "image/ico");
	hash_add(mime_hash, "webp", "image/webp");
	hash_add(mime_hash, "svg", "image/svg");
	hash_add(mime_hash, "js", "text/javascript");
	hash_add(mime_hash, "txt", "text/plain");

	fh = NULL;
	if (mimefile != NULL) {
		if ((stat(mimefile, &statbuf) == 0) && S_ISREG(statbuf.st_mode)) {
			fh = fopen(mimefile, "r");
		}
	}

	if (fh == NULL) {
		NSLOG(netsurf, INFO,
		      "Using built-in mime.types for Tsunami");
		return;
	}

	while (!feof(fh)) {
		char line[MAX_LINE_LEN];
		char *ptr;
		char *type;
		char *ext;

		if (fgets(line, MAX_LINE_LEN, fh) == NULL) {
			break;
		}
		if (feof(fh) || line[0] == '#') {
			continue;
		}

		ptr = line;
		while (ascii_is_space(*ptr)) {
			ptr++;
		}
		if (*ptr == '\n' || *ptr == '\0') {
			continue;
		}

		type = ptr;
		while (*ptr && (!ascii_is_space(*ptr))) {
			ptr++;
		}
		if (*ptr == '\0' || *ptr == '\n') {
			continue;
		}
		*ptr++ = '\0';

		while (ascii_is_space(*ptr)) {
			ptr++;
		}

		while (1) {
			ext = ptr;
			while (*ptr && (!ascii_is_space(*ptr))) {
				ptr++;
			}
			if (*ptr == '\0' || *ptr == '\n') {
				*ptr = '\0';
				hash_add(mime_hash, ext, type);
				break;
			}
			*ptr++ = '\0';
			hash_add(mime_hash, ext, type);
			while (*ptr && ascii_is_space(*ptr) && *ptr != '\n') {
				ptr++;
			}
		}
	}

	fclose(fh);
}

void tsunami_fetch_filetype_fin(void)
{
	if (mime_hash != NULL) {
		hash_destroy(mime_hash);
		mime_hash = NULL;
	}
}

const char *tsunami_fetch_filetype(const char *unix_path)
{
	struct stat statbuf;
	char *ext;
	const char *ptr;
	char *lowerchar;
	const char *type;

	if (stat(unix_path, &statbuf) != 0) {
		return "text/plain";
	}
	if (S_ISDIR(statbuf.st_mode)) {
		return "application/x-netsurf-directory";
	}

	if (strchr(unix_path, '.') == NULL) {
		return "text/plain";
	}

	ptr = unix_path + strlen(unix_path);
	while (*ptr != '.' && *ptr != '/' && ptr > unix_path) {
		ptr--;
	}
	if (*ptr != '.') {
		return "text/plain";
	}

	ext = strdup(ptr + 1);
	if (ext == NULL) {
		return "text/plain";
	}

	lowerchar = ext;
	while (*lowerchar) {
		*lowerchar = ascii_to_lower(*lowerchar);
		lowerchar++;
	}

	type = hash_get(mime_hash, ext);
	free(ext);

	return type != NULL ? type : "text/plain";
}
